#include "Village.h"

#include <algorithm>
#include <cstdlib>

#include "Wildlife.h"
#include "vf/StructureGen.h"

namespace vf {

namespace {

constexpr int villageRadius = 20;
constexpr int villageVerticalRadius = 6;
constexpr float upkeepInterval = 1.0f;
constexpr int maxSlope = 5;

bool suitableSite(const WorldGen& generator, ColumnCoord column) {
    int x = column.x * chunkSize, z = column.z * chunkSize;
    Biome biome = generator.biomeAt(x + chunkSize / 2, z + chunkSize / 2);
    if (biome != Biome::Plains && biome != Biome::Forest && biome != Biome::Desert) return false;
    int low = generator.settings().maxY, high = 0;
    for (int dx = -4; dx <= 18; dx += 11) {
        for (int dz = -6; dz <= 18; dz += 12) {
            int height = generator.surfaceHeight(x + dx, z + dz);
            low = std::min(low, height);
            high = std::max(high, height);
        }
    }
    return low > generator.settings().seaLevel + 1 && high - low <= maxSlope;
}

}

bool Village::settleNear(const WorldGen& generator, ColumnCoord origin, int rings) {
    for (int ring = 0; ring < rings; ++ring) {
        for (int dx = -ring; dx <= ring; ++dx) {
            for (int dz = -ring; dz <= ring; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                ColumnCoord column{origin.x + dx, origin.z + dz};
                if (!suitableSite(generator, column)) continue;
                for (int ox = -2; ox <= 2; ++ox)
                    for (int oz = -2; oz <= 2; ++oz) terrain.generateNow({column.x + ox, column.z + oz});
                if (found(generator, column)) return true;
            }
        }
    }
    return false;
}

Village::Village(World& world, TerrainView& terrain) : world(world), terrain(terrain) {
    pathConfig.maxExpansions = 4000;
}

void Village::flatten(int minX, int maxX, int minZ, int maxZ, int groundY) {
    for (int x = minX; x <= maxX; ++x) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = groundY + 1; y <= groundY + 14; ++y) world.setBlock(x, y, z, Block::Air);
            for (int y = groundY - 4; y < groundY; ++y)
                if (!isSolid(world.blockAt(x, y, z))) world.setBlock(x, y, z, Block::Dirt);
            world.setBlock(x, groundY, z, Block::Grass);
        }
    }
}

bool Village::found(const WorldGen& generator, ColumnCoord column) {
    int baseX = column.x * chunkSize;
    int baseZ = column.z * chunkSize;
    int groundY = generator.surfaceHeight(baseX + 6, baseZ + 6);
    if (groundY <= generator.settings().seaLevel) return false;

    StructureGenParams structureParams;
    structureParams.seed = generator.settings().seed;
    StructureGen structures(structureParams);
    StructurePlan plan =
        structures.buildStructure(StructureGenKind::Village, column.x, column.z, groundY);

    int pathLength = 0;
    for (const StructurePiece& piece : plan.pieces)
        if (piece.block == Block::Gravel) ++pathLength;

    flatten(plan.bounds.min.x - 2, plan.bounds.max.x + 2, plan.bounds.min.z - 4,
            plan.bounds.max.z + 2, groundY);
    structures.stamp(world, plan, generator.settings().maxY);

    int house = 0;
    std::vector<Vec3> beds;
    for (const StructurePiece& piece : plan.pieces) {
        if (piece.block != Block::Glowstone) continue;
        int hx = plan.origin.x + piece.offset.x - 2;
        int hz = plan.origin.z + piece.offset.z - 2;
        int floorY = groundY + 1;
        world.setBlock(hx + 1, floorY, hz + 3, Block::Bed);
        world.setBlock(hx + 3, floorY, hz + 3, Block::Bed);
        auto profession = static_cast<VillagerProfession>(
            static_cast<int>(VillagerProfession::Farmer) +
            house % (static_cast<int>(VillagerProfession::Count) -
                     static_cast<int>(VillagerProfession::Farmer)));
        world.setBlock(hx + 1, floorY, hz + 1, workstationForProfession(profession));
        Vec3 interior{static_cast<float>(hx) + 2.5f, static_cast<float>(floorY),
                      static_cast<float>(hz) + 2.5f};
        beds.push_back(interior);
        beds.push_back(interior);
        ++house;
    }

    bellCell = {plan.origin.x + pathLength / 2, groundY + 1, plan.origin.z - 3};
    world.setBlock(bellCell.x, bellCell.y, bellCell.z, Block::Bell);
    entranceSpot = {static_cast<float>(bellCell.x) + 0.5f, static_cast<float>(groundY + 1),
                    static_cast<float>(bellCell.z) - 1.0f};

    registry.scan(world, bellCell, villageRadius, villageVerticalRadius, pathConfig);
    uint32_t seed = generator.settings().seed ^ 0x51ed27u;
    for (std::size_t i = 0; i < beds.size(); ++i) {
        Vec3 spot = beds[i];
        spot.x += (i % 2 == 0) ? -0.4f : 0.4f;
        residents.push_back(makeVillager(static_cast<int>(i) + 1, spot, seed));
    }

    for (int cx = floorDiv(plan.bounds.min.x - 2, chunkSize);
         cx <= floorDiv(plan.bounds.max.x + 2, chunkSize); ++cx)
        for (int cz = floorDiv(plan.bounds.min.z - 4, chunkSize);
             cz <= floorDiv(plan.bounds.max.z + 2, chunkSize); ++cz)
            terrain.pin({cx, cz});
    built = true;
    return true;
}

void Village::tick(const TimeOfDay& clock, bool threatened, float dt) {
    if (!built) return;
    upkeepTimer += dt;
    if (upkeepTimer >= upkeepInterval) {
        upkeepTimer = 0.0f;
        registry.revalidate(world, pathConfig);
        spreadVillageGossip(residents);
    }
    for (Villager& villager : residents)
        tickVillager(world, villager, registry, clock, schedule, bellCell, threatened, dt, pathConfig);
}

int Village::pick(const Vec3& origin, const Vec3& direction, float maxDistance,
                  float& distance) const {
    int best = -1;
    distance = maxDistance;
    for (std::size_t i = 0; i < residents.size(); ++i) {
        if (!residents[i].alive) continue;
        float hit = rayBoxDistance(origin, direction, mobBox(residents[i].body));
        if (hit < 0.0f || hit >= distance) continue;
        distance = hit;
        best = static_cast<int>(i);
    }
    return best;
}

VillageCensus Village::census() const {
    return takeVillageCensus(residents, registry, bellCell, villageRadius);
}

}
