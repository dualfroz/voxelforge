#include "vf/Mob.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace vf {

namespace {

uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float unitFloat(uint32_t& rng) { return static_cast<float>(xorshift(rng)) / 4294967295.0f; }

float horizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Vec3i floorCell(const Vec3& position) {
    return {static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)),
            static_cast<int>(std::floor(position.z))};
}

Vec3 randomHeading(uint32_t& rng) {
    float angle = unitFloat(rng) * 6.2831853f;
    return {std::cos(angle), 0.0f, std::sin(angle)};
}

Vec3 pathTowardWishDir(const World& world, const Mob& body, const Vec3& goalPos,
                       const PathConfig& pathConfig, const Vec3& fallbackDir) {
    Vec3i start = mobFeetCell(body);
    Vec3i goal = floorCell(goalPos);
    std::vector<Vec3i> path = findPath(world, start, goal, pathConfig);
    if (path.size() >= 2) {
        Vec3 center{static_cast<float>(path[1].x) + 0.5f, body.position.y,
                    static_cast<float>(path[1].z) + 0.5f};
        Vec3 toNext{center.x - body.position.x, 0.0f, center.z - body.position.z};
        return normalize(toNext);
    }
    if (path.size() == 1) return {0.0f, 0.0f, 0.0f};
    return fallbackDir;
}

SpeciesStats makeStats(float maxHealth, float width, float height, float walkSpeed,
                       float sprintMultiplier, float attackDamage, float attackRange,
                       float attackCooldown, float alertRadius, float disengageRadius,
                       int spawnLightMax, int spawnLightMin) {
    SpeciesStats stats;
    stats.maxHealth = maxHealth;
    stats.width = width;
    stats.height = height;
    stats.walkSpeed = walkSpeed;
    stats.sprintMultiplier = sprintMultiplier;
    stats.attackDamage = attackDamage;
    stats.attackRange = attackRange;
    stats.attackCooldown = attackCooldown;
    stats.alertRadius = alertRadius;
    stats.disengageRadius = disengageRadius;
    stats.spawnLightMax = spawnLightMax;
    stats.spawnLightMin = spawnLightMin;
    return stats;
}

std::array<SpeciesStats, static_cast<std::size_t>(Species::Count)> buildSpeciesTable() {
    std::array<SpeciesStats, static_cast<std::size_t>(Species::Count)> table{};
    table[static_cast<std::size_t>(Species::Sheep)] =
        makeStats(10.0f, 0.9f, 1.3f, 2.0f, 1.6f, 0.0f, 0.0f, 0.0f, 8.0f, 12.0f, 15, 9);
    table[static_cast<std::size_t>(Species::Cow)] =
        makeStats(10.0f, 0.9f, 1.4f, 2.0f, 1.6f, 0.0f, 0.0f, 0.0f, 8.0f, 12.0f, 15, 9);
    table[static_cast<std::size_t>(Species::Pig)] =
        makeStats(10.0f, 0.9f, 0.9f, 2.0f, 1.6f, 0.0f, 0.0f, 0.0f, 8.0f, 12.0f, 15, 9);
    table[static_cast<std::size_t>(Species::Zombie)] =
        makeStats(20.0f, 0.6f, 1.95f, 2.3f, 1.4f, 3.0f, 1.4f, 1.0f, 16.0f, 20.0f, 7, 0);
    table[static_cast<std::size_t>(Species::Skeleton)] =
        makeStats(20.0f, 0.6f, 1.95f, 2.3f, 1.4f, 2.0f, 8.0f, 1.4f, 16.0f, 20.0f, 7, 0);
    table[static_cast<std::size_t>(Species::Creeper)] =
        makeStats(20.0f, 0.6f, 1.7f, 2.5f, 1.3f, 6.0f, 1.6f, 1.5f, 16.0f, 20.0f, 7, 0);
    return table;
}

constexpr int fullSkyLight = 15;

}

bool isHostile(Species species) {
    switch (species) {
        case Species::Zombie:
        case Species::Skeleton:
        case Species::Creeper:
            return true;
        default:
            return false;
    }
}

const SpeciesStats& speciesStats(Species species) {
    static const std::array<SpeciesStats, static_cast<std::size_t>(Species::Count)> table =
        buildSpeciesTable();
    return table[static_cast<std::size_t>(species)];
}

Creature spawnCreature(Species species, const Vec3& position, uint32_t seed) {
    const SpeciesStats& stats = speciesStats(species);
    Creature creature;
    creature.species = species;
    creature.body.position = position;
    creature.body.width = stats.width;
    creature.body.height = stats.height;
    creature.health = stats.maxHealth;
    creature.rng = seed != 0 ? seed : 0x9e3779b9u;
    creature.state = CreatureState::Idle;
    return creature;
}

void updateCreature(const World& world, Creature& creature, const Vec3& threatPos, bool hasThreat,
                    const CreatureConfig& config, float dt) {
    if (!creature.alive) return;

    if (creature.attackCooldownRemaining > 0.0f)
        creature.attackCooldownRemaining = std::max(0.0f, creature.attackCooldownRemaining - dt);

    const SpeciesStats& stats = speciesStats(creature.species);
    float distToThreat = hasThreat ? horizontalDistance(creature.body.position, threatPos) : 0.0f;
    bool wasEngaged = creature.state == CreatureState::Chase || creature.state == CreatureState::Attack ||
                      creature.state == CreatureState::Flee;
    float triggerRadius = wasEngaged ? stats.disengageRadius : stats.alertRadius;
    bool engaged = hasThreat && distToThreat <= triggerRadius;

    if (isHostile(creature.species)) {
        creature.state = !engaged ? CreatureState::Wander
                         : (distToThreat <= stats.attackRange ? CreatureState::Attack : CreatureState::Chase);
    } else {
        creature.state = engaged ? CreatureState::Flee : CreatureState::Wander;
    }

    Vec3 wishDir{0.0f, 0.0f, 0.0f};
    float speed = 0.0f;

    switch (creature.state) {
        case CreatureState::Idle:
            break;
        case CreatureState::Wander: {
            creature.stateTimer -= dt;
            if (creature.stateTimer <= 0.0f || length(creature.body.wanderDir) < 0.1f) {
                creature.body.wanderDir = randomHeading(creature.rng);
                creature.stateTimer = 1.5f + unitFloat(creature.rng) * 2.5f;
            }
            wishDir = creature.body.wanderDir;
            speed = stats.walkSpeed;
            break;
        }
        case CreatureState::Chase: {
            wishDir = pathTowardWishDir(world, creature.body, threatPos, config.physics.path,
                                       creature.body.wanderDir);
            creature.body.wanderDir = wishDir;
            speed = stats.walkSpeed * stats.sprintMultiplier;
            break;
        }
        case CreatureState::Flee: {
            Vec3 away{creature.body.position.x - threatPos.x, 0.0f,
                     creature.body.position.z - threatPos.z};
            away = normalize(away);
            if (length(away) < 0.1f) away = randomHeading(creature.rng);
            Vec3 fleeTarget = creature.body.position + away * 8.0f;
            wishDir = pathTowardWishDir(world, creature.body, fleeTarget, config.physics.path, away);
            creature.body.wanderDir = wishDir;
            speed = stats.walkSpeed * stats.sprintMultiplier;
            break;
        }
        case CreatureState::Attack:
            wishDir = {0.0f, 0.0f, 0.0f};
            speed = 0.0f;
            break;
    }

    MobConfig physics = config.physics;
    physics.moveSpeed = speed;
    stepMob(world, creature.body, wishDir, physics, dt);
}

bool damageCreature(Creature& creature, float amount) {
    if (!creature.alive) return false;
    creature.health -= amount;
    if (creature.health <= 0.0f) {
        creature.health = 0.0f;
        creature.alive = false;
        return true;
    }
    return false;
}

bool creatureCanAttack(const Creature& creature, const Vec3& targetPos) {
    if (!creature.alive || creature.attackCooldownRemaining > 0.0f) return false;
    const SpeciesStats& stats = speciesStats(creature.species);
    return horizontalDistance(creature.body.position, targetPos) <= stats.attackRange;
}

void creatureAttack(Creature& creature) {
    creature.attackCooldownRemaining = speciesStats(creature.species).attackCooldown;
}

std::vector<DropEntry> dropsFor(Species species) {
    switch (species) {
        case Species::Sheep:
            return {{ItemId::RawMutton, 1, 2, 1.0f}};
        case Species::Cow:
            return {{ItemId::RawBeef, 1, 3, 1.0f}};
        case Species::Pig:
            return {{ItemId::RawPorkchop, 1, 3, 1.0f}};
        case Species::Zombie:
            return {{ItemId::RottenFlesh, 0, 2, 0.8f}};
        case Species::Skeleton:
            return {{ItemId::Stick, 0, 2, 0.7f}};
        case Species::Creeper:
            return {{ItemId::Gunpowder, 0, 2, 1.0f}};
        default:
            return {};
    }
}

std::vector<ItemSlot> rollDrops(Species species, uint32_t& rng) {
    std::vector<ItemSlot> drops;
    for (const DropEntry& entry : dropsFor(species)) {
        if (unitFloat(rng) > entry.chance) continue;
        int spread = entry.maxCount > entry.minCount
            ? static_cast<int>(xorshift(rng) % static_cast<uint32_t>(entry.maxCount - entry.minCount + 1))
            : 0;
        int count = entry.minCount + spread;
        if (count > 0) drops.push_back(ItemSlot{entry.item, count, 0});
    }
    return drops;
}

int mobSpawnLightLevel(const World& world, const Vec3i& cell, int searchRadius) {
    bool skyOpen = true;
    for (int y = cell.y + 1; y <= cell.y + 64; ++y) {
        if (world.solidAt(cell.x, y, cell.z)) {
            skyOpen = false;
            break;
        }
    }
    int sky = skyOpen ? fullSkyLight : 0;

    int emitterBest = 0;
    for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
        for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
            for (int dz = -searchRadius; dz <= searchRadius; ++dz) {
                int manhattan = std::abs(dx) + std::abs(dy) + std::abs(dz);
                if (manhattan > searchRadius) continue;
                Block block = world.blockAt(cell.x + dx, cell.y + dy, cell.z + dz);
                int emission = blockLightEmission(block);
                if (emission <= 0) continue;
                int level = emission - manhattan;
                if (level > emitterBest) emitterBest = level;
            }
        }
    }

    return std::max(sky, emitterBest);
}

bool speciesAllowsBiome(Species species, Biome biome) {
    switch (species) {
        case Species::Sheep:
        case Species::Cow:
        case Species::Pig:
            return biome == Biome::Plains || biome == Biome::Forest;
        case Species::Zombie:
        case Species::Skeleton:
            return biome != Biome::Ocean;
        case Species::Creeper:
            return biome != Biome::Ocean && biome != Biome::Beach;
        default:
            return false;
    }
}

bool canSpawnAt(const World& world, const Vec3i& cell, Species species, Biome biome,
               const Vec3& playerPos, int currentPopulationInArea, const SpawnConfig& config) {
    if (currentPopulationInArea >= config.maxPerArea) return false;
    if (!speciesAllowsBiome(species, biome)) return false;
    if (!isStandable(world, cell)) return false;

    float dist = horizontalDistance(
        {static_cast<float>(cell.x) + 0.5f, 0.0f, static_cast<float>(cell.z) + 0.5f}, playerPos);
    if (dist < config.minPlayerDistance || dist > config.maxPlayerDistance) return false;

    int light = mobSpawnLightLevel(world, cell);
    const SpeciesStats& stats = speciesStats(species);
    if (isHostile(species)) {
        if (light > stats.spawnLightMax) return false;
    } else {
        if (light < stats.spawnLightMin) return false;
    }
    return true;
}

SpawnAttempt trySpawn(const World& world, const Vec3i& cell, Biome biome, const Vec3& playerPos,
                      int currentPopulationInArea, uint32_t& rng, const SpawnConfig& config) {
    std::array<Species, static_cast<std::size_t>(Species::Count)> candidates{};
    int candidateCount = 0;
    for (int i = 0; i < static_cast<int>(Species::Count); ++i) {
        Species species = static_cast<Species>(i);
        if (canSpawnAt(world, cell, species, biome, playerPos, currentPopulationInArea, config))
            candidates[static_cast<std::size_t>(candidateCount++)] = species;
    }
    if (candidateCount == 0) return {};

    uint32_t pick = xorshift(rng) % static_cast<uint32_t>(candidateCount);
    SpawnAttempt attempt;
    attempt.spawned = true;
    attempt.species = candidates[pick];
    attempt.position = {static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y),
                        static_cast<float>(cell.z) + 0.5f};
    return attempt;
}

bool advanceSpawnCycle(SpawnCycle& cycle, float dt) {
    cycle.timer += dt;
    if (cycle.timer < cycle.interval) return false;
    cycle.timer -= cycle.interval;
    return true;
}

}
