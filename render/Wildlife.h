#pragma once

#include <cstdint>
#include <vector>

#include "Terrain.h"
#include "vf/Item.h"
#include "vf/Mob.h"
#include "vf/Spawning.h"

namespace vf {

struct WildCreature {
    Creature creature;
    float panicSeconds = 0.0f;
    float hurtSeconds = 0.0f;
    float corpseSeconds = 0.0f;
};

struct WildlifeImpact {
    float damage = 0.0f;
    bool exploded = false;
    Vec3 knockback;
};

class Wildlife {
public:
    Wildlife(World& world, TerrainView& terrain, const WorldGen& generator, uint32_t seed);

    void populate(const Vec3& playerPosition, bool daytime);
    WildlifeImpact tick(const Vec3& playerPosition, bool daytime, float dt);

    int pick(const Vec3& origin, const Vec3& direction, float maxDistance, float& distance) const;
    std::vector<ItemSlot> strike(int index, const Vec3& attacker, float damage);

    const std::vector<WildCreature>& creatures() const { return herd; }
    int livingIn(SpawnCategory category) const;
    int spawnedTotal() const { return spawner.spawnedCount(); }

private:
    void runSpawnCycle(const Vec3& playerPosition, bool daytime, int columnsPerCategory);
    void spawnFrom(const SpawnResult& result);
    WildlifeImpact detonate(WildCreature& creeper, const Vec3& playerPosition);

    World& world;
    TerrainView& terrain;
    const WorldGen& generator;
    Spawner spawner;
    CreatureConfig creatureConfig;
    DespawnPolicy despawnPolicy;
    std::vector<WildCreature> herd;
    uint32_t rng;
    uint64_t cycle = 0;
    float spawnTimer = 0.0f;
};

float rayBoxDistance(const Vec3& origin, const Vec3& direction, const Aabb& box);
BiomeKind spawnBiomeFor(Biome biome);
const char* biomeDisplayName(Biome biome);

}
