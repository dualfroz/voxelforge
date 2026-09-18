#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Behavior.h"
#include "BiomeGen.h"
#include "Block.h"
#include "Dimension.h"
#include "Lighting.h"
#include "Math.h"
#include "Mob.h"
#include "Pathfind.h"
#include "World.h"

namespace vf {

enum class SpawnCategory : uint8_t {
    Hostile = 0,
    Passive = 1,
    Water = 2,
    Ambient = 3,
    Count,
};

constexpr int spawnCategoryCount = static_cast<int>(SpawnCategory::Count);

const char* spawnCategoryName(SpawnCategory category);
SpawnCategory categoryForSpecies(Species species);

enum class SpawnDenial : uint8_t {
    None = 0,
    CategoryCapReached,
    WrongDimension,
    WrongBiome,
    WrongTimeOfDay,
    HeightOutOfRange,
    UnsuitableGround,
    NoHeadroom,
    BlockLightTooHigh,
    SkyLightTooHigh,
    LightTooLow,
    TooCloseToPlayer,
    TooFarFromPlayer,
    NoStandableCell,
    NoRuleMatched,
    Count,
};

constexpr int spawnDenialCount = static_cast<int>(SpawnDenial::Count);

const char* spawnDenialName(SpawnDenial denial);

struct SpawnRule {
    Species species = Species::Zombie;
    SpawnCategory category = SpawnCategory::Hostile;
    DimensionId dimension = DimensionId::Overworld;

    int minBlockLight = 0;
    int maxBlockLight = 15;
    int minSkyLight = 0;
    int maxSkyLight = 15;
    int minY = 0;
    int maxY = 127;

    std::vector<Block> groundBlocks;
    int requiredHeadroom = 2;
    std::vector<BiomeKind> biomes;

    bool dayAllowed = true;
    bool nightAllowed = true;

    int packMin = 1;
    int packMax = 4;
    int packSpread = 3;
    int weight = 10;

    float minPlayerDistance = 8.0f;
    float maxPlayerDistance = 96.0f;

    bool allowsBiome(BiomeKind biome) const;
    bool allowsGround(Block block) const;
};

std::vector<SpawnRule> defaultSpawnRules();
const SpawnRule* findSpawnRule(const std::vector<SpawnRule>& rules, Species species);

struct MobCap {
    std::array<int, spawnCategoryCount> baseCap{{70, 10, 5, 15}};
    int chunksPerCapUnit = 289;

    int capFor(SpawnCategory category, int loadedChunks) const;
    int baseCapFor(SpawnCategory category) const;
};

struct SpawnEnvironment {
    const World* world = nullptr;
    const LightField* light = nullptr;
    const BiomeGen* biomes = nullptr;

    DimensionId dimension = DimensionId::Overworld;
    BiomeKind fallbackBiome = BiomeKind::Plains;
    bool daytime = true;

    Vec3 playerPosition;
    bool playerPresent = true;

    int loadedChunks = 289;
    int topY = 96;
    int bottomY = 1;
    PathConfig path;

    std::array<int, spawnCategoryCount> population{{0, 0, 0, 0}};

    int populationOf(SpawnCategory category) const;
    void addPopulation(SpawnCategory category, int amount);
    BiomeKind biomeAt(int gx, int gz) const;
    int blockLightAt(const Vec3i& cell) const;
    int skyLightAt(const Vec3i& cell) const;
};

struct SpawnTrial {
    Vec3i cell;
    Species species = Species::Zombie;
    SpawnCategory category = SpawnCategory::Hostile;
    int packSize = 0;
    bool ruleFound = false;
    bool cellFound = false;
};

struct SpawnResult {
    bool spawned = false;
    SpawnDenial denial = SpawnDenial::NoRuleMatched;
    Species species = Species::Zombie;
    SpawnCategory category = SpawnCategory::Hostile;
    BehaviorRole role = BehaviorRole::HostileHunter;
    Vec3i origin;
    std::vector<Vec3> positions;

    int packSize() const { return static_cast<int>(positions.size()); }
};

uint64_t spawnHash(uint64_t seed, int x, int y, int z, int salt);

bool hasSpawnRoom(const World& world, const Vec3i& cell, int headroom);

SpawnDenial checkSpawnRule(const SpawnRule& rule, const SpawnEnvironment& env, const Vec3i& cell,
                           const MobCap& cap);

bool pickSpawnCell(const SpawnEnvironment& env, const Vec3i& chunkColumn, uint64_t seed,
                   int attemptIndex, Vec3i& outCell);

std::vector<Vec3> spreadPack(const SpawnRule& rule, const SpawnEnvironment& env,
                             const Vec3i& origin, int packSize, uint32_t& rng);

enum class DespawnBand : uint8_t {
    Protected = 0,
    Random = 1,
    Immediate = 2,
};

struct DespawnPolicy {
    float instantDistance = 128.0f;
    float randomDistance = 32.0f;
    int randomChanceDenominator = 800;
    bool despawnNamed = false;
    bool despawnLeashed = false;
};

struct DespawnCandidate {
    Vec3 position;
    bool named = false;
    bool leashed = false;
    bool persistent = false;
};

DespawnBand despawnBandFor(const DespawnPolicy& policy, const Vec3& position,
                           const Vec3& playerPosition);
bool despawnCandidateIsProtected(const DespawnPolicy& policy, const DespawnCandidate& candidate);
bool shouldDespawn(const DespawnPolicy& policy, const DespawnCandidate& candidate,
                   const Vec3& playerPosition, uint32_t& rng);

struct SpawnerBlockState {
    Species species = Species::Zombie;
    Vec3i position;
    int spawnCount = 4;
    int spawnRange = 4;
    int nearbyLimit = 6;
    int requiredHeadroom = 2;
    float activationRange = 16.0f;
    int minDelay = 200;
    int maxDelay = 800;
    int delay = 20;
    int totalSpawned = 0;
    int cyclesFired = 0;
    uint32_t rng = 0x1f123bb5u;
};

struct SpawnerTickResult {
    bool playerInRange = false;
    bool fired = false;
    SpawnDenial denial = SpawnDenial::None;
    std::vector<Vec3> positions;
    int delayAfter = 0;
};

SpawnerTickResult tickSpawnerBlock(SpawnerBlockState& spawner, const SpawnEnvironment& env,
                                   int nearbyOfKind);

class Spawner {
public:
    Spawner();
    explicit Spawner(std::vector<SpawnRule> ruleTable, const MobCap& caps = {});

    const std::vector<SpawnRule>& rules() const { return ruleTable; }
    const MobCap& caps() const { return capTable; }

    void setAttemptsPerColumn(int attempts) { attemptsPerColumnCount = attempts; }
    int attemptsPerColumn() const { return attemptsPerColumnCount; }

    const SpawnRule* pickRule(const SpawnEnvironment& env, SpawnCategory category,
                              uint64_t seed) const;

    SpawnTrial planSpawn(const SpawnEnvironment& env, SpawnCategory category,
                         const Vec3i& chunkColumn, uint64_t seed, int attemptIndex) const;

    SpawnResult attemptSpawn(const SpawnEnvironment& env, const SpawnRule& rule,
                             const SpawnTrial& trial, uint64_t seed);

    SpawnResult runColumn(SpawnEnvironment& env, SpawnCategory category, const Vec3i& chunkColumn,
                          uint64_t seed, int attemptIndex);

    std::vector<SpawnResult> runCycle(SpawnEnvironment& env, SpawnCategory category,
                                      const std::vector<Vec3i>& chunkColumns, uint64_t seed);

    int denialCount(SpawnDenial denial) const;
    int spawnedCount() const { return spawned; }
    void resetStats();

private:
    std::vector<SpawnRule> ruleTable;
    MobCap capTable;
    int attemptsPerColumnCount = 3;
    int spawned = 0;
    std::array<int, spawnDenialCount> denials{};
};

std::vector<BehaviorAgent> agentsFromSpawn(const SpawnResult& result, uint32_t seed);

}
