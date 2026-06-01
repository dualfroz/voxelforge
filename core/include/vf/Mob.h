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
    float sprintMultiplier = 1.5f;   // chase (hostile) or flee (passive) speed factor
    float attackDamage = 0.0f;
    float attackRange = 1.4f;
    float attackCooldown = 1.0f;
    float alertRadius = 12.0f;       // hostile: aggro distance; passive: flee-trigger distance
    float disengageRadius = 16.0f;   // hysteresis give-up radius once engaged
    int spawnLightMax = 7;           // hostile: spawns only at or below this light
    int spawnLightMin = 9;           // passive: spawns only at or above this light
};

const SpeciesStats& speciesStats(Species species);

enum class CreatureState : uint8_t { Idle, Wander, Flee, Chase, Attack };

// A gameplay mob: species identity, health and an AI state machine layered on
// top of the physics body from Entity.h. `body.wanderDir` is reused as the
// creature's persistent wander/flee heading between ticks.
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

// One AI+physics tick. `hasThreat` false means no relevant target this tick
// (out of range, dead, or none nearby); the creature then wanders or idles.
// Hostile species chase and attack a present threat; passive species flee it.
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

// Rolls the drop table for `species`, advancing `rng`. Deterministic given the
// same rng state.
std::vector<ItemSlot> rollDrops(Species species, uint32_t& rng);

struct SpawnConfig {
    int maxPerArea = 24;
    float spawnAreaRadius = 64.0f;
    float minPlayerDistance = 12.0f;
    float maxPlayerDistance = 40.0f;
};

// Approximate ambient light at a cell for spawn checks: full skylight when the
// column above is unobstructed, combined with the strongest nearby block
// emitter attenuated per Manhattan step.
int mobSpawnLightLevel(const World& world, const Vec3i& cell, int searchRadius = 6);

bool speciesAllowsBiome(Species species, Biome biome);

bool canSpawnAt(const World& world, const Vec3i& cell, Species species, Biome biome,
               const Vec3& playerPos, int currentPopulationInArea, const SpawnConfig& config = {});

struct SpawnAttempt {
    bool spawned = false;
    Species species = Species::Sheep;
    Vec3 position;
};

// Tests every species against canSpawnAt and, if at least one qualifies, picks
// one deterministically from `rng`.
SpawnAttempt trySpawn(const World& world, const Vec3i& cell, Biome biome, const Vec3& playerPos,
                      int currentPopulationInArea, uint32_t& rng, const SpawnConfig& config = {});

struct SpawnCycle {
    float interval = 1.0f;
    float timer = 0.0f;
};

// Advances the cycle timer; returns true on ticks where a spawn attempt should
// run. Catches up at most once per call even after a large dt.
bool advanceSpawnCycle(SpawnCycle& cycle, float dt);

} // namespace vf
