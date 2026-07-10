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

// Natural spawning: which mob may appear where, how many of them the loaded
// world can hold, how they arrive in packs and when they vanish again. Every
// decision is a pure function of the world, the rule table and a seed - the
// same inputs always produce the same spawn.
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

// Why a spawn did not happen. A refusal always names its reason instead of
// collapsing into a bare false.
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

// One species' conditions. An empty biome or ground list means "anything";
// each light bound is checked on its own channel, so a rule can demand open
// sky and still refuse a torch-lit cell.
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

// The stock table covers every species Mob.h defines: three hostile and three
// passive. Water and Ambient exist as first-class categories with their own
// caps and are filled by whatever rules a caller adds.
std::vector<SpawnRule> defaultSpawnRules();
const SpawnRule* findSpawnRule(const std::vector<SpawnRule>& rules, Species species);

// Population ceiling per category, scaled by how much world is actually
// loaded: the same base numbers give a small cap for a handful of chunks and
// the full cap for a fully loaded view distance.
struct MobCap {
    std::array<int, spawnCategoryCount> baseCap{{70, 10, 5, 15}};
    int chunksPerCapUnit = 289;

    int capFor(SpawnCategory category, int loadedChunks) const;
    int baseCapFor(SpawnCategory category) const;
};

// Everything a spawn check reads besides the rule itself. `light` is the baked
// field from Lighting.h, so block light and sky light come from the same
// source the renderer uses.
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
    // Sky light as the spawn rules see it: the baked value during the day,
    // dimmed by the night penalty after dusk, which is what lets hostiles
    // appear under an open sky only once it is dark.
    int skyLightAt(const Vec3i& cell) const;
};

// The plan for one spawn attempt: which rule won the weighted draw, which cell
// the seed picked and how large the pack would be. Nothing here has been
// validated yet - that is what attemptSpawn does with it.
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

// Deterministic 64-bit mix. Same seed, same cell, same salt, same number.
uint64_t spawnHash(uint64_t seed, int x, int y, int z, int salt);

// Standable, has the headroom the rule wants and nothing solid in the way.
bool hasSpawnRoom(const World& world, const Vec3i& cell, int headroom);

// The full per-cell verdict, checked in a fixed order so a caller can tell
// exactly which condition failed.
SpawnDenial checkSpawnRule(const SpawnRule& rule, const SpawnEnvironment& env, const Vec3i& cell,
                           const MobCap& cap);

// Picks the highest standable cell in one column of the chunk, chosen from the
// seed rather than scanned in order.
bool pickSpawnCell(const SpawnEnvironment& env, const Vec3i& chunkColumn, uint64_t seed,
                   int attemptIndex, Vec3i& outCell);

// Spreads a pack around an accepted origin. Members that land somewhere the
// rule rejects are simply left out; the origin is always member zero.
std::vector<Vec3> spreadPack(const SpawnRule& rule, const SpawnEnvironment& env,
                             const Vec3i& origin, int packSize, uint32_t& rng);

enum class DespawnBand : uint8_t {
    Protected = 0,
    Random = 1,
    Immediate = 2,
};

// Distance bands around the player. Beyond `instantDistance` a mob is gone the
// moment it is checked; between the two distances it rolls every tick; closer
// than `randomDistance` it never despawns on its own.
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

// A monster spawner block: counts down, refuses to fire without a player in
// range or with too many of its own kind already nearby, then picks a fresh
// delay from its range.
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

// Runs spawn cycles over loaded chunk columns and keeps a per-reason tally of
// everything it refused, so a world that never spawns anything can be
// diagnosed instead of guessed at.
class Spawner {
public:
    Spawner();
    explicit Spawner(std::vector<SpawnRule> ruleTable, const MobCap& caps = {});

    const std::vector<SpawnRule>& rules() const { return ruleTable; }
    const MobCap& caps() const { return capTable; }

    void setAttemptsPerColumn(int attempts) { attemptsPerColumnCount = attempts; }
    int attemptsPerColumn() const { return attemptsPerColumnCount; }

    // Weighted pick among the rules of one category that fit the dimension.
    const SpawnRule* pickRule(const SpawnEnvironment& env, SpawnCategory category,
                              uint64_t seed) const;

    // Draws a rule and a cell without touching the world state.
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

// Hands a finished spawn straight to the behaviour layer: every member of the
// pack becomes an agent already carrying the tree role for its species.
std::vector<BehaviorAgent> agentsFromSpawn(const SpawnResult& result, uint32_t seed);

} // namespace vf
