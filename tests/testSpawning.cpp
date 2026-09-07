#include "testHarness.h"
#include "vf/Spawning.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

constexpr int floorY = 64;
constexpr int spawnY = 65;
constexpr int fieldSpan = 48;

// One flat plate of `surface` with the sky wide open above it, big enough for
// the light box below to see nothing but that plate.
World plateWorld(Block surface) {
    World world;
    for (int x = -fieldSpan; x <= fieldSpan; ++x) {
        for (int z = -fieldSpan; z <= fieldSpan; ++z) world.setBlock(x, floorY, z, surface);
    }
    return world;
}

LightField bakeLight(const World& world) {
    return computeLight(world, Vec3i{-fieldSpan, floorY - 2, -fieldSpan},
                        Vec3i{fieldSpan * 2 + 1, 10, fieldSpan * 2 + 1});
}

SpawnEnvironment nightEnvironment(const World& world, const LightField& light) {
    SpawnEnvironment env;
    env.world = &world;
    env.light = &light;
    env.dimension = DimensionId::Overworld;
    env.fallbackBiome = BiomeKind::Plains;
    env.daytime = false;
    env.playerPresent = true;
    env.playerPosition = {40.5f, static_cast<float>(spawnY), 0.5f};
    env.loadedChunks = 289;
    env.topY = 70;
    env.bottomY = 61;
    return env;
}

SpawnRule zombieRule() {
    std::vector<SpawnRule> rules = defaultSpawnRules();
    return *findSpawnRule(rules, Species::Zombie);
}

SpawnRule sheepRule() {
    std::vector<SpawnRule> rules = defaultSpawnRules();
    return *findSpawnRule(rules, Species::Sheep);
}

Vec3i originCell() { return Vec3i{0, spawnY, 0}; }

} // namespace

VF_TEST(theStockRuleTableCoversEverySpeciesWithACoherentCategory) {
    std::vector<SpawnRule> rules = defaultSpawnRules();
    CHECK_EQ(rules.size(), 6u);

    for (int index = 0; index < static_cast<int>(Species::Count); ++index) {
        Species species = static_cast<Species>(index);
        const SpawnRule* rule = findSpawnRule(rules, species);
        CHECK(rule != nullptr);
        if (rule == nullptr) continue;
        CHECK(rule->category == categoryForSpecies(species));
        CHECK(rule->packMin >= 1);
        CHECK(rule->packMax >= rule->packMin);
        CHECK(rule->weight > 0);
    }

    CHECK(findSpawnRule(rules, Species::Zombie)->dayAllowed == false);
    CHECK(findSpawnRule(rules, Species::Sheep)->dayAllowed);
    CHECK(std::string(spawnCategoryName(SpawnCategory::Water)) == std::string("water"));
    CHECK(std::string(spawnCategoryName(SpawnCategory::Ambient)) == std::string("ambient"));

    for (int index = 0; index < spawnDenialCount; ++index) {
        SpawnDenial denial = static_cast<SpawnDenial>(index);
        CHECK(std::string(spawnDenialName(denial)) != std::string("unknown"));
    }
}

VF_TEST(aCellThatSatisfiesEveryConditionIsAccepted) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    MobCap cap;

    CHECK_EQ(env.skyLightAt(originCell()), 4);
    CHECK_EQ(env.blockLightAt(originCell()), 0);
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), cap) == SpawnDenial::None);
}

VF_TEST(aFullCategoryCapRefusesTheSpawnBeforeAnythingElseIsChecked) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    MobCap cap;

    env.population[static_cast<std::size_t>(SpawnCategory::Hostile)] =
        cap.capFor(SpawnCategory::Hostile, env.loadedChunks);
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), cap) ==
          SpawnDenial::CategoryCapReached);

    env.population[static_cast<std::size_t>(SpawnCategory::Hostile)] -= 1;
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), cap) == SpawnDenial::None);
}

VF_TEST(aRuleFromAnotherDimensionIsRefused) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.dimension = DimensionId::Nether;
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) ==
          SpawnDenial::WrongDimension);
}

VF_TEST(aCellOutsideTheRulesHeightBandIsRefused) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    SpawnRule rule = zombieRule();
    rule.minY = spawnY + 4;
    CHECK(checkSpawnRule(rule, env, originCell(), MobCap{}) == SpawnDenial::HeightOutOfRange);

    rule.minY = 1;
    rule.maxY = spawnY - 1;
    CHECK(checkSpawnRule(rule, env, originCell(), MobCap{}) == SpawnDenial::HeightOutOfRange);
}

VF_TEST(aBiomeOutsideTheRulesListIsRefused) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    std::vector<SpawnRule> rules = defaultSpawnRules();
    SpawnRule creeper = *findSpawnRule(rules, Species::Creeper);
    CHECK(!creeper.biomes.empty());

    env.fallbackBiome = BiomeKind::Ocean;
    CHECK(checkSpawnRule(creeper, env, originCell(), MobCap{}) == SpawnDenial::WrongBiome);

    env.fallbackBiome = BiomeKind::Forest;
    CHECK(checkSpawnRule(creeper, env, originCell(), MobCap{}) == SpawnDenial::None);
}

VF_TEST(aNocturnalRuleIsRefusedInBroadDaylight) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.daytime = true;
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) ==
          SpawnDenial::WrongTimeOfDay);
}

VF_TEST(theWrongBlockUnderfootIsRefused) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    SpawnRule rule = zombieRule();
    rule.groundBlocks = {Block::Sand};
    CHECK(checkSpawnRule(rule, env, originCell(), MobCap{}) == SpawnDenial::UnsuitableGround);

    rule.groundBlocks = {Block::Sand, Block::Grass};
    CHECK(checkSpawnRule(rule, env, originCell(), MobCap{}) == SpawnDenial::None);
}

VF_TEST(aCeilingPressedOntoTheSpawnCellIsRefusedForNoHeadroom) {
    World world = plateWorld(Block::Grass);
    world.setBlock(0, spawnY + 1, 0, Block::Stone);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) == SpawnDenial::NoHeadroom);

    SpawnRule crawler = zombieRule();
    crawler.requiredHeadroom = 1;
    CHECK(checkSpawnRule(crawler, env, originCell(), MobCap{}) == SpawnDenial::None);
}

VF_TEST(aTorchLitCellIsRefusedForTooMuchBlockLight) {
    World world = plateWorld(Block::Grass);
    world.setBlock(1, spawnY, 1, Block::Glowstone);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    CHECK(env.blockLightAt(originCell()) > zombieRule().maxBlockLight);
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) ==
          SpawnDenial::BlockLightTooHigh);

    // The same cell far from the emitter is dark enough again.
    Vec3i faraway{30, spawnY, 30};
    CHECK_EQ(env.blockLightAt(faraway), 0);
    CHECK(checkSpawnRule(zombieRule(), env, faraway, MobCap{}) == SpawnDenial::None);
}

VF_TEST(anOpenSkyAtNoonIsRefusedForTooMuchSkyLight) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.daytime = true;

    SpawnRule daywalker = zombieRule();
    daywalker.dayAllowed = true;
    CHECK_EQ(env.skyLightAt(originCell()), 15);
    CHECK(checkSpawnRule(daywalker, env, originCell(), MobCap{}) ==
          SpawnDenial::SkyLightTooHigh);
}

VF_TEST(aSunlessCellIsRefusedForTooLittleLight) {
    World world = plateWorld(Block::Grass);
    for (int x = -fieldSpan; x <= fieldSpan; ++x) {
        for (int z = -fieldSpan; z <= fieldSpan; ++z) world.setBlock(x, floorY + 4, z, Block::Stone);
    }
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.daytime = true;
    env.playerPosition = {20.5f, static_cast<float>(spawnY), 0.5f};

    CHECK_EQ(env.skyLightAt(originCell()), 0);
    CHECK(checkSpawnRule(sheepRule(), env, originCell(), MobCap{}) == SpawnDenial::LightTooLow);
}

VF_TEST(theSpawnRingAroundThePlayerIsRefusedOnBothSides) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    env.playerPosition = {0.5f, static_cast<float>(spawnY), 0.5f};
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) ==
          SpawnDenial::TooCloseToPlayer);

    env.playerPosition = {400.5f, static_cast<float>(spawnY), 0.5f};
    CHECK(checkSpawnRule(zombieRule(), env, originCell(), MobCap{}) ==
          SpawnDenial::TooFarFromPlayer);
}

VF_TEST(aColumnWithNoFloorReportsThatNoCellWasStandable) {
    World world;
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    Vec3i cell;
    CHECK(!pickSpawnCell(env, Vec3i{0, 0, 0}, 99u, 0, cell));

    Spawner spawner;
    SpawnResult result = spawner.runColumn(env, SpawnCategory::Hostile, Vec3i{0, 0, 0}, 99u, 0);
    CHECK(result.denial == SpawnDenial::NoStandableCell);
    CHECK(!result.spawned);
    CHECK_EQ(spawner.denialCount(SpawnDenial::NoStandableCell), 1);
}

VF_TEST(aCategoryWithNoRulesReportsThatNoRuleMatched) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    Spawner spawner;
    SpawnResult result = spawner.runColumn(env, SpawnCategory::Water, Vec3i{0, 0, 0}, 7u, 0);
    CHECK(result.denial == SpawnDenial::NoRuleMatched);
    CHECK_EQ(spawner.denialCount(SpawnDenial::NoRuleMatched), 1);

    // Adding one water rule is all it takes for the category to come alive.
    std::vector<SpawnRule> rules = defaultSpawnRules();
    SpawnRule diver = sheepRule();
    diver.category = SpawnCategory::Water;
    rules.push_back(diver);
    Spawner wet(rules);
    CHECK(wet.pickRule(env, SpawnCategory::Water, 3u) != nullptr);
}

VF_TEST(theMobCapGrowsWithTheNumberOfLoadedChunks) {
    MobCap cap;
    CHECK_EQ(cap.capFor(SpawnCategory::Hostile, 289), cap.baseCapFor(SpawnCategory::Hostile));
    CHECK_EQ(cap.capFor(SpawnCategory::Hostile, 578), cap.baseCapFor(SpawnCategory::Hostile) * 2);
    CHECK_EQ(cap.capFor(SpawnCategory::Hostile, 0), 0);

    int previous = -1;
    for (int chunks = 0; chunks <= 1156; chunks += 17) {
        int current = cap.capFor(SpawnCategory::Hostile, chunks);
        CHECK(current >= previous);
        previous = current;
    }
    CHECK(cap.capFor(SpawnCategory::Hostile, 1156) > cap.capFor(SpawnCategory::Hostile, 289));
    CHECK(cap.capFor(SpawnCategory::Passive, 289) < cap.capFor(SpawnCategory::Hostile, 289));

    // A quarter of the world loaded holds roughly a quarter of the mobs.
    CHECK_EQ(cap.capFor(SpawnCategory::Hostile, 72), 17);
}

VF_TEST(aRunningCycleStopsSpawningOnceTheCapIsReached) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.loadedChunks = 17;
    env.playerPosition = {0.5f, static_cast<float>(spawnY), 0.5f};

    MobCap cap;
    int limit = cap.capFor(SpawnCategory::Hostile, env.loadedChunks);
    CHECK(limit > 0);

    std::vector<Vec3i> columns;
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) columns.push_back(Vec3i{cx, 0, cz});
    }

    Spawner spawner;
    spawner.setAttemptsPerColumn(6);
    for (int cycle = 0; cycle < 12; ++cycle) {
        spawner.runCycle(env, SpawnCategory::Hostile, columns, 1234u + static_cast<uint64_t>(cycle));
    }

    CHECK(env.populationOf(SpawnCategory::Hostile) > 0);
    CHECK(env.populationOf(SpawnCategory::Hostile) <= limit + 4);
    CHECK(spawner.denialCount(SpawnDenial::CategoryCapReached) > 0);
}

VF_TEST(aPackArrivesSpreadAroundItsOriginCell) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    SpawnRule rule = zombieRule();
    rule.packMin = 4;
    rule.packMax = 4;
    rule.packSpread = 3;

    Spawner spawner({rule});
    SpawnTrial trial;
    trial.cell = originCell();
    trial.species = Species::Zombie;
    trial.category = SpawnCategory::Hostile;
    trial.ruleFound = true;
    trial.cellFound = true;
    trial.packSize = 4;

    SpawnResult result = spawner.attemptSpawn(env, rule, trial, 4242u);
    CHECK(result.spawned);
    CHECK(result.denial == SpawnDenial::None);
    CHECK(result.packSize() >= 2);
    CHECK(result.packSize() <= 4);
    CHECK(result.role == BehaviorRole::HostileHunter);

    for (const Vec3& spot : result.positions) {
        CHECK(std::fabs(spot.x - 0.5f) <= static_cast<float>(rule.packSpread));
        CHECK(std::fabs(spot.z - 0.5f) <= static_cast<float>(rule.packSpread));
        CHECK(world.solidAt(static_cast<int>(std::floor(spot.x)), floorY,
                            static_cast<int>(std::floor(spot.z))));
    }
}

VF_TEST(aSpawnedPackBecomesAgentsCarryingTheRightBehaviourRole) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);

    SpawnRule rule = zombieRule();
    rule.packMin = 3;
    rule.packMax = 3;
    Spawner spawner({rule});

    SpawnTrial trial;
    trial.cell = originCell();
    trial.species = Species::Zombie;
    trial.ruleFound = true;
    trial.cellFound = true;
    trial.packSize = 3;

    SpawnResult result = spawner.attemptSpawn(env, rule, trial, 77u);
    CHECK(result.spawned);

    std::vector<BehaviorAgent> agents = agentsFromSpawn(result, 5u);
    CHECK_EQ(agents.size(), result.positions.size());
    const SpeciesStats& stats = speciesStats(Species::Zombie);
    for (const BehaviorAgent& agent : agents) {
        CHECK(agent.species == Species::Zombie);
        CHECK_NEAR(agent.health, stats.maxHealth, 1e-6f);
        CHECK_NEAR(agent.maxHealth, stats.maxHealth, 1e-6f);
        CHECK(agent.moveStep > 0.0f);
        CHECK(agent.rng != 0u);
    }
    CHECK(makeTreeForRole(result.role).structureValid());

    SpawnResult refused;
    CHECK(agentsFromSpawn(refused, 5u).empty());
}

VF_TEST(despawnNeverTouchesAMobStandingNextToThePlayer) {
    DespawnPolicy policy;
    DespawnCandidate mob;
    mob.position = {10.0f, 64.0f, 0.0f};
    Vec3 player{0.0f, 64.0f, 0.0f};

    CHECK(despawnBandFor(policy, mob.position, player) == DespawnBand::Protected);
    uint32_t rng = 12345u;
    for (int tick = 0; tick < 2000; ++tick) {
        CHECK(!shouldDespawn(policy, mob, player, rng));
    }
}

VF_TEST(despawnRemovesAMobBeyondTheHardDistanceImmediately) {
    DespawnPolicy policy;
    DespawnCandidate mob;
    mob.position = {200.0f, 64.0f, 0.0f};
    Vec3 player{0.0f, 64.0f, 0.0f};

    CHECK(despawnBandFor(policy, mob.position, player) == DespawnBand::Immediate);
    uint32_t rng = 1u;
    CHECK(shouldDespawn(policy, mob, player, rng));
    CHECK_EQ(rng, 1u);
}

VF_TEST(despawnInTheMiddleBandIsARollThatMatchesItsStatedOdds) {
    DespawnPolicy policy;
    policy.randomChanceDenominator = 50;
    DespawnCandidate mob;
    mob.position = {64.0f, 64.0f, 0.0f};
    Vec3 player{0.0f, 64.0f, 0.0f};
    CHECK(despawnBandFor(policy, mob.position, player) == DespawnBand::Random);

    int hits = 0;
    uint32_t rng = 0x1337u;
    for (int tick = 0; tick < 5000; ++tick) {
        if (shouldDespawn(policy, mob, player, rng)) ++hits;
    }
    CHECK(hits > 40);
    CHECK(hits < 180);

    // Same seed, same sequence of rolls.
    int replay = 0;
    uint32_t again = 0x1337u;
    for (int tick = 0; tick < 5000; ++tick) {
        if (shouldDespawn(policy, mob, player, again)) ++replay;
    }
    CHECK_EQ(replay, hits);
}

VF_TEST(namedAndLeashedMobsSurviveEvenAtTheEdgeOfTheWorld) {
    DespawnPolicy policy;
    Vec3 player{0.0f, 64.0f, 0.0f};
    uint32_t rng = 9u;

    DespawnCandidate named;
    named.position = {500.0f, 64.0f, 0.0f};
    named.named = true;
    CHECK(despawnCandidateIsProtected(policy, named));
    CHECK(!shouldDespawn(policy, named, player, rng));

    DespawnCandidate leashed;
    leashed.position = {500.0f, 64.0f, 0.0f};
    leashed.leashed = true;
    CHECK(!shouldDespawn(policy, leashed, player, rng));

    DespawnCandidate persistent;
    persistent.position = {500.0f, 64.0f, 0.0f};
    persistent.persistent = true;
    CHECK(!shouldDespawn(policy, persistent, player, rng));

    DespawnPolicy ruthless;
    ruthless.despawnNamed = true;
    CHECK(!despawnCandidateIsProtected(ruthless, named));
    CHECK(shouldDespawn(ruthless, named, player, rng));
}

VF_TEST(theSpawnerBlockCountsDownActivatesAndRespectsItsNearbyLimit) {
    World world = plateWorld(Block::Grass);
    LightField light = bakeLight(world);
    SpawnEnvironment env = nightEnvironment(world, light);
    env.playerPosition = {0.5f, static_cast<float>(spawnY), 0.5f};

