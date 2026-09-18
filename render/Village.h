#pragma once

#include <vector>

#include "Terrain.h"
#include "vf/Villager.h"

namespace vf {

class Village {
public:
    Village(World& world, TerrainView& terrain);

    bool settleNear(const WorldGen& generator, ColumnCoord origin, int rings);
    bool found(const WorldGen& generator, ColumnCoord column);
    void tick(const TimeOfDay& clock, bool threatened, float dt);

    int pick(const Vec3& origin, const Vec3& direction, float maxDistance, float& distance) const;

    bool founded() const { return built; }
    const Vec3i& centre() const { return bellCell; }
    const Vec3& entrance() const { return entranceSpot; }
    const std::vector<Villager>& villagers() const { return residents; }
    const PoiRegistry& pois() const { return registry; }
    VillageCensus census() const;

private:
    void flatten(int minX, int maxX, int minZ, int maxZ, int groundY);

    World& world;
    TerrainView& terrain;
    PoiRegistry registry;
    VillagerSchedule schedule;
    PathConfig pathConfig;
    std::vector<Villager> residents;
    Vec3i bellCell;
    Vec3 entranceSpot;
    float upkeepTimer = 0.0f;
    bool built = false;
};

}
