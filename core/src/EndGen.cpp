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

    for (int index = 0; index < params.pillarCount; ++index) {
        float angle = 6.2831853f * static_cast<float>(index) /
                      static_cast<float>(params.pillarCount);
        uint32_t roll = mixCoord(params.seed ^ 0x50494c4cu, index, 0, params.pillarRing);

        int spread = params.pillarRing + static_cast<int>(roll % 9u) - 4;
        int gx = static_cast<int>(std::lround(std::cos(angle) * static_cast<float>(spread)));
        int gz = static_cast<int>(std::lround(std::sin(angle) * static_cast<float>(spread)));

        ObsidianPillar pillar;
        pillar.base = {gx, surfaceHeight(gx, gz), gz};
        pillar.radius = 1 + static_cast<int>((roll >> 8) % 3u);
        pillar.height = 22 + static_cast<int>((roll >> 12) % 19u);
        pillar.crowned = pillar.height >= 34;

        int top = pillar.base.y + pillar.height;
        if (top >= params.ceilingY - 2) pillar.height = params.ceilingY - 3 - pillar.base.y;
        ring.push_back(pillar);
    }
    return ring;
}

void EndGenerator::generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const {
    int ox = chunkCoord.x * chunkSize;
    int oy = chunkCoord.y * chunkSize;
    int oz = chunkCoord.z * chunkSize;

    for (int lx = 0; lx < chunkSize; ++lx) {
        for (int lz = 0; lz < chunkSize; ++lz) {
            int topY = 0, bottomY = 0;
            bool solidColumn = columnExtent(ox + lx, oz + lz, topY, bottomY);
            for (int ly = 0; ly < chunkSize; ++ly) {
                int gy = oy + ly;
                bool stone = solidColumn && gy >= bottomY && gy <= topY;
                chunk.set(lx, ly, lz, stone ? Block::EndStone : Block::Air);
            }
        }
    }
}

void EndGenerator::stampPillars(World& world, int minX, int minZ, int maxX, int maxZ) const {
    for (const ObsidianPillar& pillar : pillars()) {
        if (pillar.height <= 0) continue;
        int reach = pillar.radius;
        for (int dx = -reach; dx <= reach; ++dx) {
            for (int dz = -reach; dz <= reach; ++dz) {
                if (dx * dx + dz * dz > reach * reach) continue;
                int gx = pillar.base.x + dx;
                int gz = pillar.base.z + dz;
                if (gx < minX || gx > maxX || gz < minZ || gz > maxZ) continue;

                int bottom = pillar.base.y - 3;
                if (bottom < params.floorY) bottom = params.floorY;
                for (int y = bottom; y <= pillar.base.y + pillar.height; ++y)
                    world.setBlock(gx, y, gz, Block::Obsidian);
            }
        }
        if (!pillar.crowned) continue;
        int crownX = pillar.base.x;
        int crownZ = pillar.base.z;
        if (crownX < minX || crownX > maxX || crownZ < minZ || crownZ > maxZ) continue;
        world.setBlock(crownX, pillar.base.y + pillar.height + 1, crownZ, Block::CryingObsidian);
    }
}

void EndGenerator::placePillars(World& world) const {
    stampPillars(world, -unboundedSpan, -unboundedSpan, unboundedSpan, unboundedSpan);
}

void EndGenerator::stampExitPortal(World& world, int minX, int minZ, int maxX, int maxZ) const {
    int surface = surfaceHeight(0, 0);
    if (surface < params.floorY) return;

    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            if (dx < minX || dx > maxX || dz < minZ || dz > maxZ) continue;

            world.setBlock(dx, surface - 1, dz, Block::Bedrock);
            bool inner = std::abs(dx) <= 1 && std::abs(dz) <= 1;
            world.setBlock(dx, surface, dz, inner ? Block::EndPortal : Block::Bedrock);
            for (int up = 1; up <= 3; ++up) world.setBlock(dx, surface + up, dz, Block::Air);
        }
    }

    const int cornerX[4] = {-2, -2, 2, 2};
    const int cornerZ[4] = {-2, 2, -2, 2};
    for (int corner = 0; corner < 4; ++corner) {
        int gx = cornerX[corner];
        int gz = cornerZ[corner];
        if (gx < minX || gx > maxX || gz < minZ || gz > maxZ) continue;
        for (int up = 1; up <= 3; ++up) world.setBlock(gx, surface + up, gz, Block::Bedrock);
    }
}

void EndGenerator::placeExitPortal(World& world) const {
    stampExitPortal(world, -unboundedSpan, -unboundedSpan, unboundedSpan, unboundedSpan);
}

int EndGenerator::growBranch(World& world, Vec3i pos, int depth, int maxDepth,
                             uint32_t& rngState) const {
    if (pos.y >= params.ceilingY - 2 || depth >= maxDepth) {
        if (world.blockAt(pos.x, pos.y, pos.z) != Block::Air) return 0;
        world.setBlock(pos.x, pos.y, pos.z, Block::ChorusFlower);
        return 1;
    }

    int written = 0;
    int segments = 1 + static_cast<int>(nextState(rngState) % 3u);
    for (int step = 0; step < segments && pos.y < params.ceilingY - 2; ++step) {
        if (world.blockAt(pos.x, pos.y, pos.z) != Block::Air) break;
        world.setBlock(pos.x, pos.y, pos.z, Block::ChorusPlant);
        ++written;
        pos.y += 1;
    }

    int branches = static_cast<int>(nextState(rngState) % 3u);
    for (int branch = 0; branch < branches; ++branch) {
        const Vec3i& step = horizontalSteps[nextState(rngState) % 4u];
        Vec3i side{pos.x + step.x, pos.y - 1, pos.z + step.z};
        if (world.blockAt(side.x, side.y, side.z) != Block::Air) continue;
        world.setBlock(side.x, side.y, side.z, Block::ChorusPlant);
        ++written;
        written += growBranch(world, {side.x, side.y + 1, side.z}, depth + 1, maxDepth, rngState);
    }

    written += growBranch(world, pos, depth + 1, maxDepth, rngState);
    return written;
}

int EndGenerator::growChorus(World& world, const Vec3i& base, uint32_t roll, int maxDepth) const {
    if (world.blockAt(base.x, base.y - 1, base.z) != Block::EndStone) return 0;
    if (world.blockAt(base.x, base.y, base.z) != Block::Air) return 0;

    uint32_t state = mixCoord(params.seed ^ 0x43484f52u, base.x, base.y, base.z) ^ roll;
    return growBranch(world, base, 0, maxDepth, state);
}

void EndGenerator::raisePurpurSpire(World& world, int gx, int gz, int top, uint32_t roll) const {
    int tall = 2 + static_cast<int>((roll >> 18) % 4u);
    for (int step = 0; step < tall; ++step) {
        int y = top + 1 + step;
        if (y >= params.ceilingY - 1) return;
        if (world.blockAt(gx, y, gz) != Block::Air) return;
        world.setBlock(gx, y, gz, Block::Purpur);
    }
}

void EndGenerator::decorateColumn(World& world, int columnX, int columnZ) const {
    int ox = columnX * chunkSize;
    int oz = columnZ * chunkSize;

    for (int lx = 4; lx < chunkSize - 4; ++lx) {
        for (int lz = 4; lz < chunkSize - 4; ++lz) {
            int gx = ox + lx;
            int gz = oz + lz;
            if (biomeAt(gx, gz) != EndBiome::OuterIsland) continue;

            int top = surfaceHeight(gx, gz);
            if (top < params.floorY) continue;
            uint32_t roll = mixCoord(params.seed ^ 0x44454330u, gx, 0, gz);

            if (params.chorus && roll % 19u == 0u) growChorus(world, {gx, top + 1, gz}, roll, 3);
            else if (roll % 53u == 7u) raisePurpurSpire(world, gx, gz, top, roll);
        }
    }
}

void EndGenerator::generateColumn(World& world, int columnX, int columnZ) const {
    for (int cy = 0; cy < stackChunkCount(); ++cy) {
        Chunk& chunk = world.ensureChunk({columnX, cy, columnZ});
        generateChunk(chunk, {columnX, cy, columnZ});
    }

    int minX = columnX * chunkSize;
    int minZ = columnZ * chunkSize;
    int maxX = minX + chunkSize - 1;
    int maxZ = minZ + chunkSize - 1;

    stampPillars(world, minX, minZ, maxX, maxZ);
    if (params.exitPortal) stampExitPortal(world, minX, minZ, maxX, maxZ);
    decorateColumn(world, columnX, columnZ);
}

} // namespace vf
