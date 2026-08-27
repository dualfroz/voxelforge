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

