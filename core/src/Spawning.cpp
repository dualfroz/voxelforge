#include "vf/Spawning.h"

#include <algorithm>
#include <cmath>

#include "vf/Effect.h"

namespace vf {

namespace {

constexpr int nightSkyLightPenalty = 11;

float straightDistance(const Vec3& a, const Vec3& b) {
    Vec3 delta = a - b;
    return length(delta);
}

bool groundAndRoomFit(const SpawnRule& rule, const SpawnEnvironment& env, const Vec3i& cell) {
    Block ground = env.world->blockAt(cell.x, cell.y - 1, cell.z);
    if (!isSolid(ground)) return false;
    if (!rule.allowsGround(ground)) return false;
    return hasSpawnRoom(*env.world, cell, rule.requiredHeadroom);
}

bool lightFits(const SpawnRule& rule, const SpawnEnvironment& env, const Vec3i& cell) {
    int blockLight = env.blockLightAt(cell);
    int skyLight = env.skyLightAt(cell);
    if (blockLight > rule.maxBlockLight) return false;
    if (skyLight > rule.maxSkyLight) return false;
    if (blockLight < rule.minBlockLight) return false;
    if (skyLight < rule.minSkyLight) return false;
    return true;
}

SpawnRule makeHostileRule(Species species, int packMin, int packMax) {
    SpawnRule rule;
    rule.species = species;
    rule.category = SpawnCategory::Hostile;
    rule.dimension = DimensionId::Overworld;
    rule.maxBlockLight = 7;
    rule.maxSkyLight = 7;
    rule.minY = 1;
    rule.maxY = 127;
    rule.requiredHeadroom = 2;
    rule.dayAllowed = false;
    rule.nightAllowed = true;
    rule.packMin = packMin;
    rule.packMax = packMax;
    rule.packSpread = 3;
    rule.weight = 10;
    rule.minPlayerDistance = 24.0f;
    rule.maxPlayerDistance = 128.0f;
    return rule;
}

SpawnRule makePassiveRule(Species species, int packMin, int packMax,
                          std::vector<BiomeKind> biomeList) {
    SpawnRule rule;
    rule.species = species;
    rule.category = SpawnCategory::Passive;
    rule.dimension = DimensionId::Overworld;
    rule.minBlockLight = 0;
    rule.minSkyLight = 9;
    rule.maxBlockLight = 15;
    rule.maxSkyLight = 15;
    rule.minY = 48;
    rule.maxY = 127;
    rule.groundBlocks = {Block::Grass};
    rule.requiredHeadroom = 2;
    rule.biomes = std::move(biomeList);
    rule.dayAllowed = true;
    rule.nightAllowed = true;
    rule.packMin = packMin;
    rule.packMax = packMax;
    rule.packSpread = 4;
    rule.weight = 12;
    rule.minPlayerDistance = 12.0f;
    rule.maxPlayerDistance = 96.0f;
    return rule;
}

} // namespace

const char* spawnCategoryName(SpawnCategory category) {
    switch (category) {
        case SpawnCategory::Hostile: return "hostile";
        case SpawnCategory::Passive: return "passive";
        case SpawnCategory::Water: return "water";
        case SpawnCategory::Ambient: return "ambient";
        case SpawnCategory::Count: break;
    }
    return "unknown";
}

SpawnCategory categoryForSpecies(Species species) {
    return isHostile(species) ? SpawnCategory::Hostile : SpawnCategory::Passive;
}

const char* spawnDenialName(SpawnDenial denial) {
    switch (denial) {
        case SpawnDenial::None: return "none";
        case SpawnDenial::CategoryCapReached: return "categoryCapReached";
        case SpawnDenial::WrongDimension: return "wrongDimension";
        case SpawnDenial::WrongBiome: return "wrongBiome";
        case SpawnDenial::WrongTimeOfDay: return "wrongTimeOfDay";
        case SpawnDenial::HeightOutOfRange: return "heightOutOfRange";
        case SpawnDenial::UnsuitableGround: return "unsuitableGround";
        case SpawnDenial::NoHeadroom: return "noHeadroom";
        case SpawnDenial::BlockLightTooHigh: return "blockLightTooHigh";
        case SpawnDenial::SkyLightTooHigh: return "skyLightTooHigh";
        case SpawnDenial::LightTooLow: return "lightTooLow";
        case SpawnDenial::TooCloseToPlayer: return "tooCloseToPlayer";
        case SpawnDenial::TooFarFromPlayer: return "tooFarFromPlayer";
        case SpawnDenial::NoStandableCell: return "noStandableCell";
        case SpawnDenial::NoRuleMatched: return "noRuleMatched";
        case SpawnDenial::Count: break;
    }
    return "unknown";
}

bool SpawnRule::allowsBiome(BiomeKind biome) const {
    if (biomes.empty()) return true;
    return std::find(biomes.begin(), biomes.end(), biome) != biomes.end();
}

bool SpawnRule::allowsGround(Block block) const {
    if (groundBlocks.empty()) return isSolid(block);
    return std::find(groundBlocks.begin(), groundBlocks.end(), block) != groundBlocks.end();
}

std::vector<SpawnRule> defaultSpawnRules() {
    std::vector<SpawnRule> rules;
    rules.push_back(makeHostileRule(Species::Zombie, 2, 4));
    rules.push_back(makeHostileRule(Species::Skeleton, 1, 3));

    SpawnRule creeper = makeHostileRule(Species::Creeper, 1, 2);
    creeper.biomes = {BiomeKind::Plains,   BiomeKind::Forest,   BiomeKind::Taiga,
                      BiomeKind::Tundra,   BiomeKind::Swamp,    BiomeKind::Jungle,
                      BiomeKind::Savanna,  BiomeKind::Mountains, BiomeKind::Desert};
    rules.push_back(creeper);

    rules.push_back(makePassiveRule(
        Species::Sheep, 2, 4,
        {BiomeKind::Plains, BiomeKind::Forest, BiomeKind::Savanna, BiomeKind::Taiga}));
    rules.push_back(makePassiveRule(
        Species::Cow, 2, 4,
        {BiomeKind::Plains, BiomeKind::Forest, BiomeKind::Savanna, BiomeKind::Taiga}));
    rules.push_back(makePassiveRule(
        Species::Pig, 1, 3, {BiomeKind::Plains, BiomeKind::Forest, BiomeKind::Swamp}));
    return rules;
}

const SpawnRule* findSpawnRule(const std::vector<SpawnRule>& rules, Species species) {
    for (const SpawnRule& rule : rules) {
        if (rule.species == species) return &rule;
    }
    return nullptr;
}

int MobCap::baseCapFor(SpawnCategory category) const {
    return baseCap[static_cast<std::size_t>(category)];
}

int MobCap::capFor(SpawnCategory category, int loadedChunks) const {
    if (loadedChunks <= 0) return 0;
    long long scaled = static_cast<long long>(baseCapFor(category)) * loadedChunks;
    return static_cast<int>(scaled / chunksPerCapUnit);
}

int SpawnEnvironment::populationOf(SpawnCategory category) const {
    return population[static_cast<std::size_t>(category)];
}

void SpawnEnvironment::addPopulation(SpawnCategory category, int amount) {
    population[static_cast<std::size_t>(category)] += amount;
}

BiomeKind SpawnEnvironment::biomeAt(int gx, int gz) const {
    return biomes ? biomes->biomeAt(gx, gz) : fallbackBiome;
}

int SpawnEnvironment::blockLightAt(const Vec3i& cell) const {
    return static_cast<int>(light->blockAtWorld(cell.x, cell.y, cell.z));
}

int SpawnEnvironment::skyLightAt(const Vec3i& cell) const {
    int raw = static_cast<int>(light->sunAtWorld(cell.x, cell.y, cell.z));
    if (daytime) return raw;
    return std::max(0, raw - nightSkyLightPenalty);
}

uint64_t spawnHash(uint64_t seed, int x, int y, int z, int salt) {
    uint64_t mixed = seed;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(x)) * 0x9e3779b97f4a7c15ull;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(y)) * 0xc2b2ae3d27d4eb4full;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(z)) * 0x165667b19e3779f9ull;
    mixed ^= static_cast<uint64_t>(static_cast<uint32_t>(salt)) * 0x27d4eb2f165667c5ull;
    mixed ^= mixed >> 33;
    mixed *= 0xff51afd7ed558ccdull;
    mixed ^= mixed >> 33;
    mixed *= 0xc4ceb9fe1a85ec53ull;
    mixed ^= mixed >> 33;
    return mixed;
}

bool hasSpawnRoom(const World& world, const Vec3i& cell, int headroom) {
    for (int step = 0; step < headroom; ++step) {
        if (world.solidAt(cell.x, cell.y + step, cell.z)) return false;
    }
    return true;
}

SpawnDenial checkSpawnRule(const SpawnRule& rule, const SpawnEnvironment& env, const Vec3i& cell,
                           const MobCap& cap) {
    if (env.populationOf(rule.category) >= cap.capFor(rule.category, env.loadedChunks))
        return SpawnDenial::CategoryCapReached;
    if (rule.dimension != env.dimension) return SpawnDenial::WrongDimension;
    if (cell.y < rule.minY || cell.y > rule.maxY) return SpawnDenial::HeightOutOfRange;
    if (!rule.allowsBiome(env.biomeAt(cell.x, cell.z))) return SpawnDenial::WrongBiome;
    if (env.daytime && !rule.dayAllowed) return SpawnDenial::WrongTimeOfDay;
    if (!env.daytime && !rule.nightAllowed) return SpawnDenial::WrongTimeOfDay;

    Block ground = env.world->blockAt(cell.x, cell.y - 1, cell.z);
    if (!isSolid(ground) || !rule.allowsGround(ground)) return SpawnDenial::UnsuitableGround;
    if (!hasSpawnRoom(*env.world, cell, rule.requiredHeadroom)) return SpawnDenial::NoHeadroom;

    int blockLight = env.blockLightAt(cell);
    int skyLight = env.skyLightAt(cell);
    if (blockLight > rule.maxBlockLight) return SpawnDenial::BlockLightTooHigh;
    if (skyLight > rule.maxSkyLight) return SpawnDenial::SkyLightTooHigh;
    if (blockLight < rule.minBlockLight) return SpawnDenial::LightTooLow;
    if (skyLight < rule.minSkyLight) return SpawnDenial::LightTooLow;

    if (env.playerPresent) {
        Vec3 spot = behaviorCellCenter(cell);
        float distance = straightDistance(spot, env.playerPosition);
        if (distance < rule.minPlayerDistance) return SpawnDenial::TooCloseToPlayer;
        if (distance > rule.maxPlayerDistance) return SpawnDenial::TooFarFromPlayer;
    }
    return SpawnDenial::None;
}

bool pickSpawnCell(const SpawnEnvironment& env, const Vec3i& chunkColumn, uint64_t seed,
                   int attemptIndex, Vec3i& outCell) {
    uint64_t roll = spawnHash(seed, chunkColumn.x, attemptIndex, chunkColumn.z, 0x5a);
    int localX = static_cast<int>(roll % static_cast<uint64_t>(chunkSize));
    int localZ = static_cast<int>((roll >> 8) % static_cast<uint64_t>(chunkSize));
    int gx = chunkColumn.x * chunkSize + localX;
    int gz = chunkColumn.z * chunkSize + localZ;

    for (int y = env.topY; y >= env.bottomY; --y) {
        Vec3i probe{gx, y, gz};
        if (!isStandable(*env.world, probe, env.path)) continue;
        outCell = probe;
        return true;
    }
    return false;
}

std::vector<Vec3> spreadPack(const SpawnRule& rule, const SpawnEnvironment& env,
                             const Vec3i& origin, int packSize, uint32_t& rng) {
    std::vector<Vec3> members;
    members.push_back(behaviorCellCenter(origin));

    for (int index = 1; index < packSize; ++index) {
        int dx = behaviorRandomRange(rng, -rule.packSpread, rule.packSpread);
        int dz = behaviorRandomRange(rng, -rule.packSpread, rule.packSpread);
        if (dx == 0 && dz == 0) continue;

        bool placed = false;
        for (int dy = 0; dy <= 2 && !placed; ++dy) {
            for (int sign = 1; sign >= -1 && !placed; sign -= 2) {
                Vec3i probe{origin.x + dx, origin.y + dy * sign, origin.z + dz};
                if (!groundAndRoomFit(rule, env, probe)) continue;
                if (!lightFits(rule, env, probe)) continue;
                members.push_back(behaviorCellCenter(probe));
                placed = true;
            }
        }
    }
    return members;
}

DespawnBand despawnBandFor(const DespawnPolicy& policy, const Vec3& position,
                           const Vec3& playerPosition) {
    float distance = straightDistance(position, playerPosition);
    if (distance > policy.instantDistance) return DespawnBand::Immediate;
    if (distance > policy.randomDistance) return DespawnBand::Random;
    return DespawnBand::Protected;
}

bool despawnCandidateIsProtected(const DespawnPolicy& policy, const DespawnCandidate& candidate) {
    if (candidate.persistent) return true;
    if (candidate.named && !policy.despawnNamed) return true;
    if (candidate.leashed && !policy.despawnLeashed) return true;
    return false;
}

bool shouldDespawn(const DespawnPolicy& policy, const DespawnCandidate& candidate,
                   const Vec3& playerPosition, uint32_t& rng) {
    if (despawnCandidateIsProtected(policy, candidate)) return false;
    DespawnBand band = despawnBandFor(policy, candidate.position, playerPosition);
    if (band == DespawnBand::Immediate) return true;
    if (band == DespawnBand::Protected) return false;
    uint32_t denominator = static_cast<uint32_t>(std::max(1, policy.randomChanceDenominator));
    return behaviorNextRandom(rng) % denominator == 0u;
}

SpawnerTickResult tickSpawnerBlock(SpawnerBlockState& spawner, const SpawnEnvironment& env,
                                   int nearbyOfKind) {
    SpawnerTickResult outcome;
    outcome.delayAfter = spawner.delay;

    Vec3 blockCenter = behaviorCellCenter(spawner.position);
    outcome.playerInRange =
        env.playerPresent && straightDistance(blockCenter, env.playerPosition) <=
                                 spawner.activationRange;
    if (!outcome.playerInRange) {
        outcome.denial = SpawnDenial::TooFarFromPlayer;
        return outcome;
    }

    if (spawner.delay > 0) {
        --spawner.delay;
        outcome.delayAfter = spawner.delay;
        return outcome;
    }

    if (nearbyOfKind >= spawner.nearbyLimit) {
        outcome.denial = SpawnDenial::CategoryCapReached;
        spawner.delay = behaviorRandomRange(spawner.rng, spawner.minDelay, spawner.maxDelay);
        outcome.delayAfter = spawner.delay;
        return outcome;
    }

    for (int index = 0; index < spawner.spawnCount; ++index) {
        int dx = behaviorRandomRange(spawner.rng, -spawner.spawnRange, spawner.spawnRange);
        int dy = behaviorRandomRange(spawner.rng, -1, 1);
        int dz = behaviorRandomRange(spawner.rng, -spawner.spawnRange, spawner.spawnRange);
        Vec3i probe{spawner.position.x + dx, spawner.position.y + dy, spawner.position.z + dz};
        if (!env.world->solidAt(probe.x, probe.y - 1, probe.z)) continue;
        if (!hasSpawnRoom(*env.world, probe, spawner.requiredHeadroom)) continue;
        outcome.positions.push_back(behaviorCellCenter(probe));
    }

    if (outcome.positions.empty()) {
        outcome.denial = SpawnDenial::NoHeadroom;
    } else {
        outcome.fired = true;
        spawner.totalSpawned += static_cast<int>(outcome.positions.size());
        ++spawner.cyclesFired;
    }

    spawner.delay = behaviorRandomRange(spawner.rng, spawner.minDelay, spawner.maxDelay);
    outcome.delayAfter = spawner.delay;
    return outcome;
}

Spawner::Spawner() : ruleTable(defaultSpawnRules()) {}

Spawner::Spawner(std::vector<SpawnRule> rules, const MobCap& caps)
    : ruleTable(std::move(rules)), capTable(caps) {}

const SpawnRule* Spawner::pickRule(const SpawnEnvironment& env, SpawnCategory category,
                                   uint64_t seed) const {
    int totalWeight = 0;
    for (const SpawnRule& rule : ruleTable) {
        if (rule.category != category) continue;
        if (rule.dimension != env.dimension) continue;
        if (rule.weight <= 0) continue;
        totalWeight += rule.weight;
    }
    if (totalWeight == 0) return nullptr;

    int pick = static_cast<int>(seed % static_cast<uint64_t>(totalWeight));
    for (const SpawnRule& rule : ruleTable) {
        if (rule.category != category) continue;
        if (rule.dimension != env.dimension) continue;
        if (rule.weight <= 0) continue;
        pick -= rule.weight;
        if (pick < 0) return &rule;
    }
    return nullptr;
}

SpawnTrial Spawner::planSpawn(const SpawnEnvironment& env, SpawnCategory category,
                              const Vec3i& chunkColumn, uint64_t seed, int attemptIndex) const {
    SpawnTrial trial;
    trial.category = category;

    uint64_t ruleSeed = spawnHash(seed, chunkColumn.x, attemptIndex, chunkColumn.z, 0x11);
    const SpawnRule* rule = pickRule(env, category, ruleSeed);
    if (rule == nullptr) return trial;

    trial.ruleFound = true;
    trial.species = rule->species;
    trial.category = rule->category;

    if (!pickSpawnCell(env, chunkColumn, seed, attemptIndex, trial.cell)) return trial;
    trial.cellFound = true;

    uint32_t packRng =
        static_cast<uint32_t>(spawnHash(seed, trial.cell.x, trial.cell.y, trial.cell.z, 0x9c));
    trial.packSize = behaviorRandomRange(packRng, rule->packMin, rule->packMax);
    return trial;
}

SpawnResult Spawner::attemptSpawn(const SpawnEnvironment& env, const SpawnRule& rule,
                                  const SpawnTrial& trial, uint64_t seed) {
    SpawnResult result;
    result.species = rule.species;
    result.category = rule.category;
    result.role = roleForSpecies(rule.species);
    result.origin = trial.cell;
    result.denial = checkSpawnRule(rule, env, trial.cell, capTable);

    if (result.denial != SpawnDenial::None) {
        ++denials[static_cast<std::size_t>(result.denial)];
        return result;
    }

    uint32_t rng = static_cast<uint32_t>(
        spawnHash(seed, trial.cell.x, trial.cell.y, trial.cell.z, 0x9c));
    result.positions = spreadPack(rule, env, trial.cell, trial.packSize, rng);
    result.spawned = true;
    ++spawned;
    return result;
}

SpawnResult Spawner::runColumn(SpawnEnvironment& env, SpawnCategory category,
                               const Vec3i& chunkColumn, uint64_t seed, int attemptIndex) {
    SpawnTrial trial = planSpawn(env, category, chunkColumn, seed, attemptIndex);

    SpawnResult result;
    result.category = category;
    result.species = trial.species;

    if (!trial.ruleFound) {
        result.denial = SpawnDenial::NoRuleMatched;
        ++denials[static_cast<std::size_t>(result.denial)];
        return result;
    }
    if (!trial.cellFound) {
        result.denial = SpawnDenial::NoStandableCell;
        ++denials[static_cast<std::size_t>(result.denial)];
        return result;
    }

    uint64_t ruleSeed = spawnHash(seed, chunkColumn.x, attemptIndex, chunkColumn.z, 0x11);
    result = attemptSpawn(env, *findSpawnRule(ruleTable, trial.species), trial, ruleSeed);
    if (result.spawned) env.addPopulation(category, result.packSize());
    return result;
}

std::vector<SpawnResult> Spawner::runCycle(SpawnEnvironment& env, SpawnCategory category,
                                           const std::vector<Vec3i>& chunkColumns, uint64_t seed) {
    std::vector<SpawnResult> results;
    results.reserve(chunkColumns.size() * static_cast<std::size_t>(attemptsPerColumnCount));
    for (const Vec3i& column : chunkColumns) {
        for (int attempt = 0; attempt < attemptsPerColumnCount; ++attempt) {
            results.push_back(runColumn(env, category, column, seed, attempt));
        }
    }
    return results;
}

int Spawner::denialCount(SpawnDenial denial) const {
    return denials[static_cast<std::size_t>(denial)];
}

void Spawner::resetStats() {
    denials.fill(0);
    spawned = 0;
}

std::vector<BehaviorAgent> agentsFromSpawn(const SpawnResult& result, uint32_t seed) {
    std::vector<BehaviorAgent> agents;
    if (!result.spawned) return agents;

    const SpeciesStats& stats = speciesStats(result.species);
    agents.reserve(result.positions.size());
    uint32_t stream = seed != 0u ? seed : 0x9e3779b9u;
    for (const Vec3& spot : result.positions) {
        BehaviorAgent agent;
        agent.species = result.species;
        agent.position = spot;
        agent.homePosition = spot;
        agent.health = stats.maxHealth;
        agent.maxHealth = stats.maxHealth;
        agent.moveStep = stats.walkSpeed / static_cast<float>(ticksPerSecond);
        agent.rng = behaviorNextRandom(stream);
        agents.push_back(agent);
    }
    return agents;
}

} // namespace vf
