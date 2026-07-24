#include "vf/EndGen.h"

#include <cmath>
#include <cstdlib>

namespace vf {

namespace {

constexpr int unboundedSpan = 1 << 28;

uint32_t mixCoord(uint32_t seed, int gx, int gy, int gz) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint32_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(seed);
    mix(static_cast<uint32_t>(gx));
    mix(static_cast<uint32_t>(gy));
    mix(static_cast<uint32_t>(gz));
    h ^= h >> 33;
    return static_cast<uint32_t>(h ^ (h >> 32));
}

uint32_t nextState(uint32_t& state) {
    state += 0x9E3779B9u;
    uint32_t z = state;
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}

const Vec3i horizontalSteps[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

float horizontalDistance(int gx, int gz, const Vec3i& center) {
    float dx = static_cast<float>(gx - center.x);
    float dz = static_cast<float>(gz - center.z);
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

EndGenerator::EndGenerator() : EndGenerator(Params{}) {}

EndGenerator::EndGenerator(const Params& endParams)
    : params(endParams),
      edgeNoise(endParams.seed ^ 0x45444745u),
      surfaceNoise(endParams.seed ^ 0x53555246u),
      outerNoise(endParams.seed ^ 0x4f555445u) {}

EndIsland EndGenerator::centralIsland() const {
    EndIsland island;
    island.center = {0, params.islandY, 0};
    island.radius = params.centralRadius;
    island.thickness = params.centralThickness;
    island.central = true;
    return island;
}

EndIsland EndGenerator::outerIslandFor(int cellX, int cellZ) const {
    EndIsland island;
    uint32_t roll = mixCoord(params.seed ^ 0x49534c44u, cellX, 0, cellZ);
    if (roll % static_cast<uint32_t>(params.outerChance) != 0u) return island;

    float presence = outerNoise.simplex2D(cellX * 0.31f, cellZ * 0.31f);
    if (presence < -0.55f) return island;

    int half = params.outerCellSize / 2;
    int jitter = half - 60;
    int offsetX = static_cast<int>((roll >> 5) % static_cast<uint32_t>(jitter * 2 + 1)) - jitter;
    int offsetZ = static_cast<int>((roll >> 15) % static_cast<uint32_t>(jitter * 2 + 1)) - jitter;

    island.center = {cellX * params.outerCellSize + half + offsetX,
                     params.islandY + static_cast<int>((roll >> 9) % 17u) - 8,
                     cellZ * params.outerCellSize + half + offsetZ};
    island.radius = 18.0f + static_cast<float>((roll >> 21) % 21u);
    island.thickness = 7.0f + static_cast<float>((roll >> 25) % 11u);
    island.central = false;
    return island;
}

EndIsland EndGenerator::islandAt(int gx, int gz) const {
    float fromOrigin = std::sqrt(static_cast<float>(gx) * static_cast<float>(gx) +
                                 static_cast<float>(gz) * static_cast<float>(gz));

    if (fromOrigin < static_cast<float>(params.outerDistance)) {
        EndIsland central = centralIsland();
        if (fromOrigin <= central.radius + params.edgeAmplitude) return central;
        return EndIsland{};
    }

    int cellX = floorDiv(gx, params.outerCellSize);
    int cellZ = floorDiv(gz, params.outerCellSize);
    EndIsland best;
    float bestDistance = 0.0f;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            EndIsland candidate = outerIslandFor(cellX + dx, cellZ + dz);
            if (!candidate.valid()) continue;
            float distance = horizontalDistance(gx, gz, candidate.center);
            if (distance > candidate.radius + params.edgeAmplitude) continue;
            if (!best.valid() || distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
    }
    return best;
}

bool EndGenerator::columnExtent(int gx, int gz, int& topY, int& bottomY) const {
    EndIsland island = islandAt(gx, gz);
    if (!island.valid()) return false;

    float distance = horizontalDistance(gx, gz, island.center);
    float wobble = edgeNoise.fbm2D(gx * params.edgeFrequency, gz * params.edgeFrequency,
                                   3, 2.0f, 0.5f);
    float reach = island.radius + wobble * params.edgeAmplitude;
    if (reach < 4.0f || distance > reach) return false;

    float falloff = 1.0f - distance / reach;
    float lens = std::sqrt(falloff);
    float bump = surfaceNoise.fbm2D(gx * 0.049f, gz * 0.049f, 3, 2.0f, 0.5f);

    topY = island.center.y + static_cast<int>(std::lround(bump * 3.0f * lens));
    bottomY = island.center.y - 1 - static_cast<int>(std::lround(island.thickness * lens));

    if (bottomY < params.floorY) bottomY = params.floorY;
    if (topY >= params.ceilingY) topY = params.ceilingY - 1;
    return topY >= bottomY;
}

bool EndGenerator::isVoid(int gx, int gz) const {
    int topY = 0, bottomY = 0;
    return !columnExtent(gx, gz, topY, bottomY);
}

int EndGenerator::surfaceHeight(int gx, int gz) const {
    int topY = 0, bottomY = 0;
    if (!columnExtent(gx, gz, topY, bottomY)) return params.floorY - 1;
    return topY;
}

bool EndGenerator::isStone(int gx, int gy, int gz) const {
    int topY = 0, bottomY = 0;
    if (!columnExtent(gx, gz, topY, bottomY)) return false;
    return gy >= bottomY && gy <= topY;
}

EndBiome EndGenerator::biomeAt(int gx, int gz) const {
    int topY = 0, bottomY = 0;
    if (!columnExtent(gx, gz, topY, bottomY)) return EndBiome::Void;
    return islandAt(gx, gz).central ? EndBiome::CentralIsland : EndBiome::OuterIsland;
}

std::vector<ObsidianPillar> EndGenerator::pillars() const {
    std::vector<ObsidianPillar> ring;
    ring.reserve(static_cast<std::size_t>(params.pillarCount));

