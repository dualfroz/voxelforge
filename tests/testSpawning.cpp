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

