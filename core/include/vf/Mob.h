#pragma once

#include <cstdint>
#include <vector>
#include "Entity.h"
#include "Item.h"
#include "Math.h"
#include "Pathfind.h"
#include "World.h"
#include "WorldGen.h"

namespace vf {

enum class Species : uint8_t { Sheep, Cow, Pig, Zombie, Skeleton, Creeper, Count };

bool isHostile(Species species);
inline bool isPassive(Species species) { return !isHostile(species); }

struct SpeciesStats {
    float maxHealth = 10.0f;
    float width = 0.6f;
    float height = 1.8f;
    float walkSpeed = 2.0f;
    float sprintMultiplier = 1.5f;
    float attackDamage = 0.0f;
    float attackRange = 1.4f;
    float attackCooldown = 1.0f;
    float alertRadius = 12.0f;
    float disengageRadius = 16.0f;
    int spawnLightMax = 7;
    int spawnLightMin = 9;
};

const SpeciesStats& speciesStats(Species species);

enum class CreatureState : uint8_t { Idle, Wander, Flee, Chase, Attack };

struct Creature {
    Mob body;
    Species species = Species::Sheep;
    CreatureState state = CreatureState::Idle;
    float health = 10.0f;
    float attackCooldownRemaining = 0.0f;
    float stateTimer = 0.0f;
    uint32_t rng = 0x9e3779b9u;
    bool alive = true;
};

Creature spawnCreature(Species species, const Vec3& position, uint32_t seed);

struct CreatureConfig {
    MobConfig physics;
};

void updateCreature(const World& world, Creature& creature, const Vec3& threatPos, bool hasThreat,
                    const CreatureConfig& config, float dt);

bool damageCreature(Creature& creature, float amount);
bool creatureCanAttack(const Creature& creature, const Vec3& targetPos);
void creatureAttack(Creature& creature);

struct DropEntry {
    ItemId item = ItemId::None;
    int minCount = 0;
    int maxCount = 0;
    float chance = 1.0f;
};

std::vector<DropEntry> dropsFor(Species species);

std::vector<ItemSlot> rollDrops(Species species, uint32_t& rng);

struct SpawnConfig {
    int maxPerArea = 24;
    float spawnAreaRadius = 64.0f;
    float minPlayerDistance = 12.0f;
    float maxPlayerDistance = 40.0f;
};

int mobSpawnLightLevel(const World& world, const Vec3i& cell, int searchRadius = 6);

bool speciesAllowsBiome(Species species, Biome biome);

bool canSpawnAt(const World& world, const Vec3i& cell, Species species, Biome biome,
               const Vec3& playerPos, int currentPopulationInArea, const SpawnConfig& config = {});

struct SpawnAttempt {
    bool spawned = false;
    Species species = Species::Sheep;
    Vec3 position;
};

SpawnAttempt trySpawn(const World& world, const Vec3i& cell, Biome biome, const Vec3& playerPos,
                      int currentPopulationInArea, uint32_t& rng, const SpawnConfig& config = {});

struct SpawnCycle {
    float interval = 1.0f;
    float timer = 0.0f;
};

bool advanceSpawnCycle(SpawnCycle& cycle, float dt);

}
