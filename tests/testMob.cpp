#include "testHarness.h"
#include "vf/Mob.h"

#include <cmath>

using namespace vf;

namespace {

World openField(int x0 = -60, int z0 = -60, int x1 = 60, int z1 = 60) {
    World world;
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z)
            world.setBlock(x, 0, z, Block::Stone);
    return world;
}

World sealedRoom() {
    World world;
    for (int x = 0; x <= 4; ++x) {
        for (int z = 0; z <= 4; ++z) {
            for (int y = 0; y <= 4; ++y) {
                bool shell = x == 0 || x == 4 || z == 0 || z == 4 || y == 0 || y == 4;
                if (shell) world.setBlock(x, y, z, Block::Stone);
            }
        }
    }
    return world;
}

float horizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

VF_TEST(isHostileClassifiesEachSpecies) {
    CHECK(!isHostile(Species::Sheep));
    CHECK(!isHostile(Species::Cow));
    CHECK(!isHostile(Species::Pig));
    CHECK(isHostile(Species::Zombie));
    CHECK(isHostile(Species::Skeleton));
    CHECK(isHostile(Species::Creeper));
}

VF_TEST(spawnCreatureAdoptsSpeciesStats) {
    Creature creature = spawnCreature(Species::Cow, {3.0f, 1.0f, 3.0f}, 42);
    const SpeciesStats& stats = speciesStats(Species::Cow);
    CHECK_NEAR(creature.health, stats.maxHealth, 1e-6f);
    CHECK_NEAR(creature.body.width, stats.width, 1e-6f);
    CHECK_NEAR(creature.body.height, stats.height, 1e-6f);
    CHECK(creature.alive);
    CHECK(creature.state == CreatureState::Idle);
}

VF_TEST(passiveWandersWhenNoThreatIsNearby) {
    World world = openField();
    Creature creature = spawnCreature(Species::Sheep, {5.0f, 1.0f, 5.0f}, 7);
    CreatureConfig config;
    Vec3 farThreat{200.0f, 1.0f, 200.0f};

    updateCreature(world, creature, farThreat, false, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Wander);
}

VF_TEST(passiveFleesAndGainsDistanceFromNearbyThreat) {
    World world = openField();
    Creature creature = spawnCreature(Species::Sheep, {5.0f, 1.0f, 5.0f}, 11);
    CreatureConfig config;
    Vec3 threat{7.0f, 1.0f, 5.0f};   // 2 blocks away, inside the 8-block alert radius

    float startDist = horizontalDistance(creature.body.position, threat);
    for (int i = 0; i < 120; ++i) updateCreature(world, creature, threat, true, config, 1.0f / 20.0f);
    float endDist = horizontalDistance(creature.body.position, threat);

    CHECK(creature.state == CreatureState::Flee);
    CHECK(endDist > startDist);
}

VF_TEST(hostileWandersWhenThreatIsFar) {
    World world = openField();
    Creature creature = spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 3);
    CreatureConfig config;
    Vec3 farThreat{500.0f, 1.0f, 500.0f};

    updateCreature(world, creature, farThreat, true, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Wander);
}

VF_TEST(hostileChasesAndClosesDistanceOnReachableThreat) {
    World world = openField();
    Creature creature = spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 5);
    CreatureConfig config;
    Vec3 threat{11.0f, 1.0f, 5.0f};   // within the 16-block alert radius, open ground

    float startDist = horizontalDistance(creature.body.position, threat);
    bool sawChase = false;
    for (int i = 0; i < 150; ++i) {
        updateCreature(world, creature, threat, true, config, 1.0f / 20.0f);
        if (creature.state == CreatureState::Chase) sawChase = true;
    }
    float endDist = horizontalDistance(creature.body.position, threat);

    CHECK(sawChase);
    CHECK(endDist < startDist);
}

VF_TEST(hostileEntersAttackStateWithinRange) {
    World world = openField();
    Creature creature = spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 9);
    CreatureConfig config;
    Vec3 threat{5.5f, 1.0f, 5.0f};   // 0.5 blocks away, inside the 1.4 attack range

    updateCreature(world, creature, threat, true, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Attack);
}

VF_TEST(hysteresisKeepsChasingInsideDisengageBandThenGivesUp) {
    World world = openField();
    Creature creature = spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 13);
    creature.state = CreatureState::Chase;
    CreatureConfig config;

    Vec3 inBand{5.0f + 18.0f, 1.0f, 5.0f};   // between alert (16) and disengage (20)
    updateCreature(world, creature, inBand, true, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Chase);

    creature.state = CreatureState::Chase;
    Vec3 farOut{5.0f + 30.0f, 1.0f, 5.0f};   // beyond disengage radius
    updateCreature(world, creature, farOut, true, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Wander);
}

VF_TEST(damageCreatureTracksHealthAndDeath) {
    Creature creature = spawnCreature(Species::Pig, {0.0f, 1.0f, 0.0f}, 21);
    float maxHealth = creature.health;

    bool died = damageCreature(creature, maxHealth * 0.5f);
    CHECK(!died);
    CHECK(creature.alive);
    CHECK_NEAR(creature.health, maxHealth * 0.5f, 1e-3f);

    died = damageCreature(creature, maxHealth);
    CHECK(died);
    CHECK(!creature.alive);
    CHECK_EQ(creature.health, 0.0f);
}

VF_TEST(creatureAttackEntersCooldownThenRecovers) {
    World world = openField();
    Creature creature = spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 17);
    Vec3 threat{5.5f, 1.0f, 5.0f};
    CreatureConfig config;
    updateCreature(world, creature, threat, true, config, 1.0f / 20.0f);
    CHECK(creature.state == CreatureState::Attack);

    CHECK(creatureCanAttack(creature, threat));
    creatureAttack(creature);
    CHECK(!creatureCanAttack(creature, threat));

    float cooldown = speciesStats(Species::Zombie).attackCooldown;
    for (float t = 0.0f; t < cooldown + 0.5f; t += 1.0f / 20.0f)
        updateCreature(world, creature, threat, true, config, 1.0f / 20.0f);
    CHECK(creatureCanAttack(creature, threat));
}

VF_TEST(dropsForMatchesSpeciesTheme) {
    auto sheepDrops = dropsFor(Species::Sheep);
    CHECK_EQ(sheepDrops.size(), static_cast<std::size_t>(1));
    CHECK(sheepDrops[0].item == ItemId::RawMutton);

    auto creeperDrops = dropsFor(Species::Creeper);
    CHECK(creeperDrops[0].item == ItemId::Gunpowder);
}

VF_TEST(rollDropsIsDeterministicForTheSameSeed) {
    uint32_t seedA = 999u;
    uint32_t seedB = 999u;
    std::vector<ItemSlot> a = rollDrops(Species::Cow, seedA);
    std::vector<ItemSlot> b = rollDrops(Species::Cow, seedB);
    CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].item == b[i].item);
        CHECK_EQ(a[i].count, b[i].count);
    }
    CHECK_EQ(seedA, seedB);   // rng advanced identically
}

VF_TEST(mobSpawnLightLevelIsHighOutsideAndDarkInsideASealedRoom) {
    World open = openField();
    int openLight = mobSpawnLightLevel(open, Vec3i{5, 1, 5});
    CHECK_EQ(openLight, 15);

    World room = sealedRoom();
    int roomLight = mobSpawnLightLevel(room, Vec3i{2, 1, 2});
    CHECK_EQ(roomLight, 0);
}

VF_TEST(canSpawnAtRespectsPopulationCap) {
    World world = openField();
    Vec3i cell{5, 1, 5};
    Vec3 player{5.0f, 1.0f, 25.0f};   // 20 blocks away, inside default range
    SpawnConfig config;
    config.maxPerArea = 3;

    CHECK(canSpawnAt(world, cell, Species::Sheep, Biome::Plains, player, 2, config));
    CHECK(!canSpawnAt(world, cell, Species::Sheep, Biome::Plains, player, 3, config));
}

VF_TEST(canSpawnAtRespectsBiomeRestrictions) {
    World world = openField();
    Vec3i cell{5, 1, 5};
    Vec3 player{5.0f, 1.0f, 25.0f};

    CHECK(canSpawnAt(world, cell, Species::Sheep, Biome::Plains, player, 0));
    CHECK(!canSpawnAt(world, cell, Species::Sheep, Biome::Ocean, player, 0));

    CHECK(speciesAllowsBiome(Species::Creeper, Biome::Forest));
    CHECK(!speciesAllowsBiome(Species::Creeper, Biome::Ocean));
    CHECK(!speciesAllowsBiome(Species::Creeper, Biome::Beach));
    CHECK(speciesAllowsBiome(Species::Zombie, Biome::Desert));
    CHECK(!speciesAllowsBiome(Species::Zombie, Biome::Ocean));
}

VF_TEST(canSpawnAtRespectsLightThresholds) {
    World room = sealedRoom();
    Vec3i darkCell{2, 1, 2};
    Vec3 farPlayer{2.0f, 1.0f, 22.0f};   // horizontal distance ~20, inside range

    CHECK(canSpawnAt(room, darkCell, Species::Zombie, Biome::Forest, farPlayer, 0));

    World lit = openField();
    Vec3i brightCell{5, 1, 5};
    CHECK(!canSpawnAt(lit, brightCell, Species::Zombie, Biome::Forest, farPlayer, 0));
}

VF_TEST(canSpawnAtRespectsPlayerDistanceWindow) {
    World world = openField();
    Vec3i cell{5, 1, 5};
    SpawnConfig config;

    Vec3 tooClose{5.0f, 1.0f, 6.0f};
    CHECK(!canSpawnAt(world, cell, Species::Sheep, Biome::Plains, tooClose, 0, config));

    Vec3 tooFar{5.0f, 1.0f, 200.0f};
    CHECK(!canSpawnAt(world, cell, Species::Sheep, Biome::Plains, tooFar, 0, config));

    Vec3 justRight{5.0f, 1.0f, 25.0f};
    CHECK(canSpawnAt(world, cell, Species::Sheep, Biome::Plains, justRight, 0, config));
}

VF_TEST(trySpawnPicksOnlyAmongValidCandidatesDeterministically) {
    World world = openField();
    Vec3i cell{5, 1, 5};
    Vec3 player{5.0f, 1.0f, 25.0f};   // daylight rules out hostiles (light 15 > their max of 7)

    uint32_t rngA = 2026u;
    uint32_t rngB = 2026u;
    SpawnAttempt a = trySpawn(world, cell, Biome::Plains, player, 0, rngA);
    SpawnAttempt b = trySpawn(world, cell, Biome::Plains, player, 0, rngB);

    CHECK(a.spawned);
    CHECK(a.species == b.species);
    CHECK(!isHostile(a.species));
}

VF_TEST(trySpawnFindsNoCandidateWhenAreaIsFull) {
    World world = openField();
    Vec3i cell{5, 1, 5};
    Vec3 player{5.0f, 1.0f, 25.0f};
    SpawnConfig config;
    config.maxPerArea = 0;

    uint32_t rng = 1u;
    SpawnAttempt attempt = trySpawn(world, cell, Biome::Plains, player, 0, rng, config);
    CHECK(!attempt.spawned);
}

VF_TEST(advanceSpawnCycleFiresOncePerIntervalAndCatchesUpOnce) {
    SpawnCycle cycle;
    cycle.interval = 1.0f;

    int fires = 0;
    for (int i = 0; i < 25; ++i)
        if (advanceSpawnCycle(cycle, 0.1f)) ++fires;
    CHECK_EQ(fires, 2);   // fires at t=1.0 and t=2.0 across 2.5s of ticking

    SpawnCycle burst;
    burst.interval = 1.0f;
    CHECK(advanceSpawnCycle(burst, 10.0f));   // huge dt: only one attempt fires this call
    CHECK_NEAR(burst.timer, 9.0f, 1e-4f);      // the rest of the backlog is kept, not dropped

    int extraFires = 0;
    for (int i = 0; i < 15; ++i)
        if (advanceSpawnCycle(burst, 0.0f)) ++extraFires;
    CHECK_EQ(extraFires, 9);   // backlog drains at one interval per subsequent call
}
