#include "vf/Dimension.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vf {

namespace {

constexpr long long ticksPerDay = 24000;

constexpr int safeSpotVerticalBand = 32;

uint32_t deriveSeed(uint32_t baseSeed, uint32_t salt) {
    uint64_t h = baseSeed;
    h ^= salt + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h *= 1099511628211ull;
    return static_cast<uint32_t>(h ^ (h >> 32));
}

}

DimensionConfig DimensionConfig::forDimension(DimensionId id) {
    DimensionConfig cfg;
    cfg.id = id;
    switch (id) {
        case DimensionId::Nether:
            cfg.floorY = 0;
            cfg.ceilingY = 128;
            cfg.hasSky = false;
            cfg.hasCeiling = true;
            cfg.hasDayCycle = false;
            cfg.waterEvaporates = true;
            cfg.bedsExplode = true;
            cfg.coordinateScale = 8.0f;
            cfg.ambientLight = 0.10f;
            cfg.seedSalt = 0x4e544852u;
            break;
        case DimensionId::End:
            cfg.floorY = 0;
            cfg.ceilingY = 128;
            cfg.hasSky = false;
            cfg.hasCeiling = false;
            cfg.hasDayCycle = false;
            cfg.waterEvaporates = false;
            cfg.bedsExplode = true;
            cfg.coordinateScale = 1.0f;
            cfg.ambientLight = 0.04f;
            cfg.seedSalt = 0x454e4421u;
            break;
        default:
            cfg.floorY = 0;
            cfg.ceilingY = 128;
            cfg.hasSky = true;
            cfg.hasCeiling = false;
            cfg.hasDayCycle = true;
            cfg.waterEvaporates = false;
            cfg.bedsExplode = false;
            cfg.coordinateScale = 1.0f;
            cfg.ambientLight = 0.0f;
            cfg.seedSalt = 0u;
            break;
    }
    return cfg;
}

const char* DimensionConfig::nameOf(DimensionId id) {
    switch (id) {
        case DimensionId::Nether: return "nether";
        case DimensionId::End: return "end";
        default: return "overworld";
    }
}

int DimensionConfig::clampY(int y) const {
    int low = lowestPlaceableY();
    int high = highestPlaceableY();
    if (y < low) return low;
    if (y > high) return high;
    return y;
}

Dimension::Dimension(const DimensionConfig& dimensionConfig, uint32_t worldSeed)
    : cfg(dimensionConfig), derivedSeed(deriveSeed(worldSeed, dimensionConfig.seedSalt)) {
    switch (cfg.id) {
        case DimensionId::Nether: {
            NetherGenerator::Params netherParams;
            netherParams.seed = derivedSeed;
            netherParams.floorY = cfg.floorY;
            netherParams.ceilingY = cfg.ceilingY;
            nether = std::make_unique<NetherGenerator>(netherParams);
            break;
        }
        case DimensionId::End: {
            EndGenerator::Params endParams;
            endParams.seed = derivedSeed;
            endParams.floorY = cfg.floorY;
            endParams.ceilingY = cfg.ceilingY;
            end = std::make_unique<EndGenerator>(endParams);
            break;
        }
        default: {
            WorldGenParams overworldParams;
            overworldParams.seed = derivedSeed;
            overworldParams.maxY = cfg.ceilingY;
            overworld = std::make_unique<WorldGen>(overworldParams);
            break;
        }
    }
}

bool Dimension::columnGenerated(int columnX, int columnZ) const {
    return columns.find(Vec3i{columnX, 0, columnZ}) != columns.end();
}

void Dimension::generateColumn(int columnX, int columnZ) {
    if (!columns.insert(Vec3i{columnX, 0, columnZ}).second) return;

    if (nether) nether->generateColumn(blocks, columnX, columnZ);
    else if (end) end->generateColumn(blocks, columnX, columnZ);
    else overworld->generateColumn(blocks, columnX, columnZ);
}

int Dimension::generateArea(int columnX, int columnZ, int columnRadius) {
    int made = 0;
    for (int dx = -columnRadius; dx <= columnRadius; ++dx) {
        for (int dz = -columnRadius; dz <= columnRadius; ++dz) {
            if (columnGenerated(columnX + dx, columnZ + dz)) continue;
            generateColumn(columnX + dx, columnZ + dz);
            ++made;
        }
    }
    return made;
}

float Dimension::dayFraction() const {
    if (!cfg.hasDayCycle) return 0.0f;
    long long phase = elapsed % ticksPerDay;
    if (phase < 0) phase += ticksPerDay;
    return static_cast<float>(phase) / static_cast<float>(ticksPerDay);
}

int Dimension::highestSolidY(int gx, int gz) const {
    for (int y = cfg.ceilingY - 1; y >= cfg.floorY; --y)
        if (blocks.solidAt(gx, y, gz)) return y;
    return cfg.floorY - 1;
}

bool Dimension::standingSpot(int gx, int gz, int fromY, int clearance, int& outFeetY) const {
    int start = std::max(fromY, cfg.floorY);
    for (int y = start; y < cfg.highestPlaceableY() - clearance; ++y) {
        if (!blocks.solidAt(gx, y, gz)) continue;
        bool roomy = true;
        for (int up = 1; up <= clearance && roomy; ++up)
            roomy = blocks.blockAt(gx, y + up, gz) == Block::Air;
        if (!roomy) continue;
        outFeetY = y + 1;
        return true;
    }
    return false;
}

DimensionSet::DimensionSet(uint32_t worldSeed) : baseSeed(worldSeed) {
    worlds.reserve(dimensionCount);
    for (int index = 0; index < dimensionCount; ++index) {
        DimensionId id = static_cast<DimensionId>(index);
        worlds.push_back(std::make_unique<Dimension>(DimensionConfig::forDimension(id), worldSeed));
    }
}

void DimensionSet::tickAll(int steps) {
    for (auto& dimension : worlds) dimension->tick(steps);
}

std::size_t DimensionSet::totalChunks() const {
    std::size_t total = 0;
    for (const auto& dimension : worlds) total += dimension->world().chunkCount();
    return total;
}

Vec3i DimensionTravel::scale(const Vec3i& pos, const DimensionConfig& from,
                             const DimensionConfig& to) {
    double ratio = static_cast<double>(from.coordinateScale) /
                   static_cast<double>(to.coordinateScale);
    return {static_cast<int>(std::floor(pos.x * ratio)), pos.y,
            static_cast<int>(std::floor(pos.z * ratio))};
}

int DimensionTravel::searchRadiusFor(DimensionId to) {
    return to == DimensionId::Nether ? 128 : 16;
}

int DimensionTravel::buildRadiusFor(DimensionId to) {
    return std::min(searchRadiusFor(to), 16);
}

TravelResult DimensionTravel::plan(DimensionId from, DimensionId to, const Vec3i& pos) const {
    return plan(from, to, pos, {});
}

TravelResult DimensionTravel::plan(DimensionId from, DimensionId to, const Vec3i& pos,
                                   const std::vector<Vec3i>& knownPortals) const {
    TravelResult result;
    result.from = from;
    result.to = to;
    result.source = pos;

    const DimensionConfig& origin = worlds.dimension(from).config();
    const DimensionConfig& destination = worlds.dimension(to).config();

    result.scaledTarget = scale(pos, origin, destination);
    result.target = result.scaledTarget;
    if (result.target.y < destination.lowestPlaceableY()) {
        result.target.y = destination.lowestPlaceableY();
        result.clampedToFloor = true;
    } else if (result.target.y > destination.highestPlaceableY()) {
        result.target.y = destination.highestPlaceableY();
        result.clampedToCeiling = true;
    }

    result.searchRadius = searchRadiusFor(to);
    long long best = -1;
    for (std::size_t index = 0; index < knownPortals.size(); ++index) {
        const Vec3i& candidate = knownPortals[index];
        long long dx = candidate.x - result.target.x;
        long long dy = candidate.y - result.target.y;
        long long dz = candidate.z - result.target.z;
        if (std::llabs(dx) > result.searchRadius || std::llabs(dz) > result.searchRadius) continue;
        long long distance = dx * dx + dy * dy + dz * dz;
        if (best >= 0 && distance >= best) continue;
        best = distance;
        result.nearestIndex = static_cast<int>(index);
    }

    if (result.nearestIndex >= 0) {
        result.needsPortal = false;
        result.nearestDistanceSquared = static_cast<int>(best);
    }
    return result;
}

bool DimensionTravel::findSafeSpot(DimensionId to, const Vec3i& near, int radius, int clearance,
                                   Vec3i& outSpot) const {
    const Dimension& destination = worlds.dimension(to);
    int floorY = destination.config().floorY;

    for (int ring = 0; ring <= radius; ++ring) {
        int bestDrop = -1;
        Vec3i best;
        for (int dx = -ring; dx <= ring; ++dx) {
            for (int dz = -ring; dz <= ring; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;

                int gx = near.x + dx;
                int gz = near.z + dz;
                int scan = floorY;
                int feetY = 0;
                while (destination.standingSpot(gx, gz, scan, clearance, feetY)) {
                    int drop = std::abs(feetY - near.y);
                    if (drop <= safeSpotVerticalBand && (bestDrop < 0 || drop < bestDrop)) {
                        bestDrop = drop;
                        best = {gx, feetY, gz};
                    }
                    scan = feetY + 1;
                }
            }
        }
        if (bestDrop >= 0) {
            outSpot = best;
            return true;
        }
    }
    return false;
}

void DimensionTravel::carvePlatform(DimensionId to, const Vec3i& spot, int halfWidth,
                                    int clearance) {
    Dimension& destination = worlds.dimension(to);
    const DimensionConfig& cfg = destination.config();
    World& world = destination.world();

    int deck = std::max(spot.y - 1, cfg.floorY);
    for (int dx = -halfWidth; dx <= halfWidth; ++dx) {
        for (int dz = -halfWidth; dz <= halfWidth; ++dz) {
            world.setBlock(spot.x + dx, deck, spot.z + dz, Block::Obsidian);
            for (int up = 1; up <= clearance; ++up) {
                int y = deck + up;
                if (y >= cfg.ceilingY - 1) break;
                world.setBlock(spot.x + dx, y, spot.z + dz, Block::Air);
            }
        }
    }
}

}
