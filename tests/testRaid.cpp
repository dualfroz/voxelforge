#include "testHarness.h"
#include "vf/Raid.h"

using namespace vf;

namespace {

constexpr int groundTop = 4;
constexpr int walkY = groundTop + 1;
const Vec3i villageCenter{0, walkY, 0};

World makeRaidWorld() {
    World world;
    for (int x = -28; x <= 28; ++x)
        for (int z = -28; z <= 28; ++z)
            for (int y = 0; y <= groundTop; ++y)
                world.setBlock(x, y, z, Block::Stone);
    world.setBlock(0, walkY, 0, Block::Bell);
    world.setBlock(4, walkY, 0, Block::Bed);
    world.setBlock(4, walkY, 2, Block::Bed);
    world.setBlock(4, walkY, 4, Block::Bed);
    world.setBlock(4, walkY, 6, Block::Bed);
    world.setBlock(-4, walkY, 0, Block::Lectern);
    world.setBlock(-4, walkY, 2, Block::Composter);
    return world;
}

std::vector<Villager> makeVillagers(int count) {
    std::vector<Villager> villagers;
    for (int i = 0; i < count; ++i) {
        Villager villager = makeVillager(i + 1,
                                         {static_cast<float>(i) + 0.5f, static_cast<float>(walkY),
                                          1.5f},
                                         100u + static_cast<uint32_t>(i));
        villager.profession = VillagerProfession::Unemployed;
        villagers.push_back(villager);
    }
    return villagers;
}

OmenState omenAt(int level) {
    OmenState omen;
    applyBadOmen(omen, level);
    return omen;
}

float distanceToCentre(const Vec3& position) {
    float dx = position.x - (static_cast<float>(villageCenter.x) + 0.5f);
    float dz = position.z - (static_cast<float>(villageCenter.z) + 0.5f);
    return std::sqrt(dx * dx + dz * dz);
}

int livingVillagerCount(const std::vector<Villager>& villagers) {
    int count = 0;
    for (const Villager& villager : villagers)
        if (villager.alive) ++count;
    return count;
}

} // namespace

VF_TEST(badOmenExpiresOnItsOwnAndCannotArmARaidAfterwards) {
    OmenState omen = omenAt(2);
    CHECK(omen.hasBadOmen());
    CHECK_EQ(omen.level, 2);
    CHECK_EQ(omen.ticksRemaining, badOmenDurationTicks);

    applyBadOmen(omen, 1);
    CHECK_EQ(omen.level, 2);
    applyBadOmen(omen, 4);
    CHECK_EQ(omen.level, 4);
    applyBadOmen(omen, badOmenMaxLevel + 3);
    CHECK_EQ(omen.level, badOmenMaxLevel);

    tickOmen(omen, badOmenDurationTicks - 1);
    CHECK(omen.hasBadOmen());
    tickOmen(omen, 5);
    CHECK(!omen.hasBadOmen());
    CHECK_EQ(omen.level, 0);

    RaidManager raid(villageCenter, 1u);
    CHECK(!raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    CHECK(raid.state() == RaidState::Inactive);
}

VF_TEST(aRaidStartsOnlyForAnOmenBearingPlayerInsideTheVillage) {
    RaidManager raid(villageCenter, 7u);
    OmenState clean;
    CHECK(!raid.tryStart(clean, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));

    OmenState omen = omenAt(1);
    CHECK(!raid.tryStart(omen, {200.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    CHECK(raid.state() == RaidState::Inactive);
    CHECK(omen.hasBadOmen());

    CHECK(raid.tryStart(omen, {6.5f, static_cast<float>(walkY), 3.5f}, 32.0f));
    CHECK(raid.state() == RaidState::Spawning);
    CHECK(raid.active());
    CHECK(raid.villageThreatened());
    CHECK_EQ(raid.badOmenLevel(), 1);
    CHECK(!omen.hasBadOmen());
    CHECK_EQ(raid.waveCount(), raidWaveCount(raidDifficultyNormal));
    CHECK_EQ(raid.waveIndex(), 0);

    OmenState second = omenAt(1);
    CHECK(!raid.tryStart(second, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    CHECK(second.hasBadOmen());
}

VF_TEST(wavesGrowWithTheirIndexTheDifficultyAndTheOmenLevel) {
    int previous = 0;
    for (int index = 0; index < 7; ++index) {
        RaidWave plan = planRaidWave(index, 1, raidDifficultyNormal);
        CHECK_EQ(plan.index, index);
        CHECK(plan.total() > previous);
        previous = plan.total();
        CHECK_EQ(plan.countOf(RaiderKind::Raider), plan.raiders);
        CHECK_EQ(plan.countOf(RaiderKind::Ravager), plan.ravagers);
    }

    CHECK_EQ(planRaidWave(0, 1, raidDifficultyNormal).vindicators, 0);
    CHECK(planRaidWave(1, 1, raidDifficultyNormal).vindicators > 0);
    CHECK_EQ(planRaidWave(1, 1, raidDifficultyNormal).witches, 0);
    CHECK(planRaidWave(2, 1, raidDifficultyNormal).witches > 0);
    CHECK_EQ(planRaidWave(2, 1, raidDifficultyNormal).ravagers, 0);
    CHECK(planRaidWave(3, 1, raidDifficultyNormal).ravagers > 0);

    CHECK(planRaidWave(3, 1, raidDifficultyHard).total() >
          planRaidWave(3, 1, raidDifficultyEasy).total());
    CHECK(planRaidWave(3, 5, raidDifficultyNormal).total() >
          planRaidWave(3, 1, raidDifficultyNormal).total());

    CHECK_EQ(raidWaveCount(raidDifficultyEasy), 3);
    CHECK_EQ(raidWaveCount(raidDifficultyNormal), 5);
    CHECK_EQ(raidWaveCount(raidDifficultyHard), 7);
}

VF_TEST(everySpawnedRaiderStartsOnTheOutskirtsAndWalksTowardsTheBell) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(4);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(1);

    RaidManager raid(villageCenter, 2024u);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    raid.tick(world, villagers, pois, omen, schedule, clock, 1);

    CHECK(raid.state() == RaidState::Fighting);
    CHECK_EQ(static_cast<int>(raid.raiders().size()), raid.wave().total());
    CHECK_EQ(raid.livingRaiders(), raid.wave().total());

    std::vector<float> startDistances;
    for (const Raider& attacker : raid.raiders()) {
        float distance = distanceToCentre(attacker.position);
        CHECK(distance > raidSpawnRadius * 0.5f);
        CHECK(!attacker.path.empty());
        startDistances.push_back(distance);
    }

    raid.tick(world, villagers, pois, omen, schedule, clock, 200);
    for (std::size_t i = 0; i < raid.raiders().size(); ++i) {
        if (!raid.raiders()[i].alive) continue;
        CHECK(distanceToCentre(raid.raiders()[i].position) < startDistances[i]);
    }
}

VF_TEST(theRaidRunsThroughEveryWaveAndEndsInVictory) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(4);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(2);

    RaidManager raid(villageCenter, 555u, raidDifficultyNormal);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));

    int expectedSpawns = 0;
    for (int index = 0; index < raid.waveCount(); ++index)
        expectedSpawns += planRaidWave(index, 2, raidDifficultyNormal).total();

    int guard = 0;
    std::vector<int> wavesSeen;
    while (raid.active() && guard++ < 20000) {
        raid.tick(world, villagers, pois, omen, schedule, clock, 1);
        if (wavesSeen.empty() || wavesSeen.back() != raid.waveIndex())
            wavesSeen.push_back(raid.waveIndex());
        for (int i = 0; i < static_cast<int>(raid.raiders().size()); ++i)
            raid.damageRaider(i, 1000.0f);
    }

    CHECK(raid.state() == RaidState::Victory);
    CHECK_EQ(static_cast<int>(wavesSeen.size()), raid.waveCount());
    CHECK_EQ(raid.waveIndex(), raid.waveCount() - 1);
    CHECK_EQ(raid.totalSpawned(), expectedSpawns);
    CHECK_EQ(raid.livingRaiders(), 0);
    CHECK_EQ(raid.villagersLost(), 0);
    CHECK_EQ(livingVillagerCount(villagers), 4);
    CHECK(!raid.active());
    CHECK(raid.elapsedTicks() > raidWaveDelayTicks * (raid.waveCount() - 1));
}

VF_TEST(victoryLeavesMajorPositiveGossipAndHeroOfTheVillageWithEverySurvivor) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(4);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(3);

    RaidManager raid(villageCenter, 909u, raidDifficultyEasy);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));

    int guard = 0;
    while (raid.active() && guard++ < 20000) {
        raid.tick(world, villagers, pois, omen, schedule, clock, 1);
        for (int i = 0; i < static_cast<int>(raid.raiders().size()); ++i)
            raid.damageRaider(i, 1000.0f);
    }
    CHECK(raid.state() == RaidState::Victory);

    for (const Villager& villager : villagers) {
        CHECK_EQ(villager.gossip.valueOf(GossipKind::MajorPositive), gossipMajorPositiveGain);
        CHECK(villager.gossip.reputation() > 0);
    }
    CHECK(omen.isHero());
    CHECK_EQ(omen.heroLevel, 3);
    CHECK_EQ(omen.heroTicksRemaining, heroDurationTicks);
    CHECK(!omen.hasBadOmen());

    tickOmen(omen, heroDurationTicks);
    CHECK(!omen.isHero());
    CHECK_EQ(omen.heroLevel, 0);
}

VF_TEST(aRaidThatKillsEveryVillagerEndsInDefeat) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(2);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(1);

    RaidManager raid(villageCenter, 31u, raidDifficultyEasy);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    CHECK_EQ(golemCapFor(2), 0);

    int guard = 0;
    while (raid.active() && guard++ < 20000)
        raid.tick(world, villagers, pois, omen, schedule, clock, 1);

    CHECK(raid.state() == RaidState::Defeat);
    CHECK_EQ(livingVillagerCount(villagers), 0);
    CHECK_EQ(raid.villagersLost(), 2);
    CHECK_EQ(raid.golems().spawnedTotal, 0);
    CHECK(!omen.isHero());
    for (const Villager& villager : villagers) {
        CHECK_EQ(villager.gossip.valueOf(GossipKind::MajorPositive), 0);
        CHECK_EQ(villager.workstationIndex, -1);
        CHECK_EQ(villager.bedIndex, -1);
    }
    CHECK_EQ(pois.freeBedCount(), pois.bedCount());
}

VF_TEST(golemsSpawnInProportionToThePopulationAndStopAtTheCap) {
    CHECK_EQ(golemCapFor(0), 0);
    CHECK_EQ(golemCapFor(golemMinimumVillagers - 1), 0);
    CHECK_EQ(golemCapFor(golemMinimumVillagers), 1);
    CHECK_EQ(golemCapFor(8), 2);
    CHECK_EQ(golemCapFor(12), 3);

    GolemSpawner spawner;
    int spawns = 0;
    for (int tick = 0; tick < 2000; ++tick)
        if (tickGolemSpawner(spawner, 12, 1)) ++spawns;
    CHECK_EQ(spawns, 3);
    CHECK_EQ(spawner.golemsAlive, 3);
    CHECK_EQ(spawner.spawnedTotal, 3);

    spawner.golemsAlive = 0;
    CHECK(tickGolemSpawner(spawner, 12, spawner.intervalTicks + 1));
    CHECK_EQ(spawner.spawnedTotal, 4);

    GolemSpawner hamlet;
    for (int tick = 0; tick < 2000; ++tick) tickGolemSpawner(hamlet, 2, 1);
    CHECK_EQ(hamlet.spawnedTotal, 0);
    CHECK(!tickGolemSpawner(hamlet, 12, 0));
}

VF_TEST(aDefendedVillageFieldsGolemsWhileTheRaidIsRunning) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(4);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(1);

    RaidManager raid(villageCenter, 77u, raidDifficultyEasy);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    raid.tick(world, villagers, pois, omen, schedule, clock, 400);

    CHECK(raid.golems().spawnedTotal >= 1);
    CHECK(raid.golems().golemsAlive >= 1);
    CHECK(raid.golems().golemsAlive <= golemCapFor(4));
    CHECK(raid.livingRaiders() < raid.wave().total());
}

VF_TEST(golemsTurnOnThePlayerOnceTheVillageReputationCollapses) {
    GolemSpawner spawner;
    GossipLedger calm;
    calm.add(GossipKind::Trading, gossipTradingGain * 4);
    updateGolemHostility(spawner, buildReputation(calm, {}));
    CHECK(!spawner.hostileToPlayer);

    GossipLedger angry;
    angry.add(GossipKind::MajorNegative, gossipMajorNegativeGain);
    std::vector<const GossipLedger*> village{&angry, &angry};
    updateGolemHostility(spawner, buildReputation(calm, village));
    CHECK(spawner.hostileToPlayer);

    std::vector<const GossipLedger*> mixed{&angry, &calm, &calm, &calm, &calm, &calm};
    updateGolemHostility(spawner, buildReputation(calm, mixed));
    CHECK(!spawner.hostileToPlayer);
}

VF_TEST(twoRaidManagersOnTheSameSeedProduceTheSameRaid) {
    World world = makeRaidWorld();
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));

    PoiRegistry firstPois;
    firstPois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> firstVillagers = makeVillagers(4);
    OmenState firstOmen = omenAt(2);
    RaidManager first(villageCenter, 4242u, raidDifficultyHard);
    CHECK(first.tryStart(firstOmen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    first.tick(world, firstVillagers, firstPois, firstOmen, schedule, clock, 300);

    PoiRegistry secondPois;
    secondPois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> secondVillagers = makeVillagers(4);
    OmenState secondOmen = omenAt(2);
    RaidManager second(villageCenter, 4242u, raidDifficultyHard);
    CHECK(second.tryStart(secondOmen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    second.tick(world, secondVillagers, secondPois, secondOmen, schedule, clock, 300);

    CHECK_EQ(first.totalSpawned(), second.totalSpawned());
    CHECK_EQ(first.livingRaiders(), second.livingRaiders());
    CHECK_EQ(first.golems().spawnedTotal, second.golems().spawnedTotal);
    CHECK(first.state() == second.state());

    bool identical = first.raiders().size() == second.raiders().size();
    for (std::size_t i = 0; i < first.raiders().size() && identical; ++i) {
        const Raider& a = first.raiders()[i];
        const Raider& b = second.raiders()[i];
        if (a.kind != b.kind || a.alive != b.alive) identical = false;
        if (std::fabs(a.position.x - b.position.x) > 1e-5f) identical = false;
        if (std::fabs(a.position.z - b.position.z) > 1e-5f) identical = false;
        if (std::fabs(a.health - b.health) > 1e-5f) identical = false;
    }
    CHECK(identical);

    RaidManager other(villageCenter, 99u, raidDifficultyHard);
    OmenState otherOmen = omenAt(2);
    std::vector<Villager> otherVillagers = makeVillagers(4);
    PoiRegistry otherPois;
    otherPois.scan(world, villageCenter, 12, 4);
    CHECK(other.tryStart(otherOmen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    other.tick(world, otherVillagers, otherPois, otherOmen, schedule, clock, 1);
    CHECK_EQ(other.wave().total(), first.wave().total());
}

VF_TEST(villagersPanicHomeInsteadOfWorkingWhileTheRaidIsRunning) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(2);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));

    for (int tick = 0; tick < 400; ++tick)
        for (Villager& villager : villagers)
            tickVillager(world, villager, pois, clock, schedule, villageCenter, false, 0.05f);
    CHECK(villagers[0].activity == VillagerActivity::Work);
    CHECK(villagers[0].employed());
    int workBefore = villagers[0].workTicks;

    OmenState omen = omenAt(1);
    RaidManager raid(villageCenter, 8u, raidDifficultyEasy);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));
    raid.tick(world, villagers, pois, omen, schedule, clock, 120);

    CHECK(villagers[0].activity == VillagerActivity::Panic);
    CHECK_EQ(villagers[0].workTicks, workBefore);
    CHECK(villagers[0].bedIndex >= 0);
    CHECK(villagers[0].alive);
    CHECK(pois.at(villagers[0].bedIndex).ownerId == villagers[0].id);
}

VF_TEST(aWaveAdvancesOnTimeoutWhenTheRaidersRefuseToDie) {
    World world = makeRaidWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    std::vector<Villager> villagers = makeVillagers(2);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));
    OmenState omen = omenAt(1);

    RaidManager raid(villageCenter, 606u, raidDifficultyNormal);
    CHECK(raid.tryStart(omen, {0.5f, static_cast<float>(walkY), 0.5f}, 32.0f));

    int firstWaveSize = 0;
    for (int tick = 0; tick < raidWaveTimeoutTicks - 5; ++tick) {
        raid.tick(world, villagers, pois, omen, schedule, clock, 1);
        for (Villager& villager : villagers)
            if (villager.alive) villager.health = 20.0f;
        if (firstWaveSize == 0) firstWaveSize = raid.totalSpawned();
    }
    CHECK_EQ(raid.waveIndex(), 0);
    CHECK_EQ(raid.totalSpawned(), firstWaveSize);
    CHECK(raid.livingRaiders() > 0);

    for (int tick = 0; tick < 20; ++tick) {
        raid.tick(world, villagers, pois, omen, schedule, clock, 1);
        for (Villager& villager : villagers)
            if (villager.alive) villager.health = 20.0f;
    }
    CHECK_EQ(raid.waveIndex(), 1);
    CHECK(raid.totalSpawned() > firstWaveSize);
    CHECK(raid.state() == RaidState::Fighting);
    CHECK_EQ(livingVillagerCount(villagers), 2);
}

VF_TEST(raiderKindsCarryDistinctStatsAndReadableNames) {
    const Raider::Stats& footSoldier = raiderStats(RaiderKind::Raider);
    const Raider::Stats& brute = raiderStats(RaiderKind::Ravager);
    CHECK(brute.maxHealth > footSoldier.maxHealth);
    CHECK(brute.damage > footSoldier.damage);
    CHECK(brute.speed > footSoldier.speed);
    CHECK(raiderStats(RaiderKind::Witch).attackRange > footSoldier.attackRange);

    CHECK_EQ(std::string(raiderKindName(RaiderKind::Vindicator)), std::string("vindicator"));
    CHECK_EQ(std::string(raidStateName(RaidState::Victory)), std::string("victory"));
    CHECK_EQ(std::string(raidStateName(RaidState::Defeat)), std::string("defeat"));

    RaidManager raid(villageCenter, 1u);
    CHECK(raid.center() == villageCenter);
    CHECK_EQ(raid.livingRaiders(), 0);
    CHECK(!raid.damageRaider(0, 5.0f));
    CHECK(raid.state() == RaidState::Inactive);
}
