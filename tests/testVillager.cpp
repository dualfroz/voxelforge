#include "testHarness.h"
#include "vf/Villager.h"

using namespace vf;

namespace {

constexpr int groundTop = 4;
constexpr int walkY = groundTop + 1;
const Vec3i villageCenter{0, walkY, 0};

World makeGround(int radius) {
    World world;
    for (int x = -radius; x <= radius; ++x)
        for (int z = -radius; z <= radius; ++z)
            for (int y = 0; y <= groundTop; ++y)
                world.setBlock(x, y, z, Block::Stone);
    return world;
}

void placePoi(World& world, int x, int z, Block block) { world.setBlock(x, walkY, z, block); }

World makeVillage() {
    World world = makeGround(20);
    placePoi(world, 0, 0, Block::Bell);
    placePoi(world, 4, 0, Block::Bed);
    placePoi(world, 4, 2, Block::Bed);
    placePoi(world, -4, 0, Block::Lectern);
    placePoi(world, 0, 5, Block::Composter);
    return world;
}

Villager makeWorker(int id, int x, int z, uint32_t seed) {
    Villager villager = makeVillager(id, {static_cast<float>(x) + 0.5f, static_cast<float>(walkY),
                                          static_cast<float>(z) + 0.5f}, seed);
    villager.profession = VillagerProfession::Unemployed;
    return villager;
}

void runTicks(const World& world, Villager& villager, PoiRegistry& pois, const TimeOfDay& clock,
              const VillagerSchedule& schedule, bool threatened, int ticks) {
    for (int i = 0; i < ticks; ++i)
        tickVillager(world, villager, pois, clock, schedule, villageCenter, threatened, 0.05f);
}

TimeOfDay clockAtFraction(double fraction) {
    return TimeOfDay(static_cast<WorldTick>(fraction * 24000.0));
}

float horizontalGap(const Vec3& position, const Vec3i& cell) {
    float dx = position.x - (static_cast<float>(cell.x) + 0.5f);
    float dz = position.z - (static_cast<float>(cell.z) + 0.5f);
    return std::sqrt(dx * dx + dz * dz);
}

}

VF_TEST(poiRegistryIndexesWorkstationsBedsAndTheBellAroundTheVillage) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    CHECK_EQ(pois.size(), 5);
    CHECK_EQ(pois.bedCount(), 2);
    CHECK_EQ(pois.workstationCount(), 2);
    CHECK(pois.bellIndex() >= 0);
    CHECK(pois.at(pois.bellIndex()).isBell());

    int lectern = pois.indexAt({-4, walkY, 0});
    CHECK(lectern >= 0);
    CHECK(pois.at(lectern).profession == VillagerProfession::Librarian);
    CHECK(pois.at(lectern).reachable);
    CHECK_EQ(pois.at(lectern).standCell.y, walkY + 1);

    int bell = pois.bellIndex();
    CHECK_EQ(pois.at(bell).standCell.y, walkY);

    CHECK_EQ(pois.freeBedCount(), 2);
    CHECK_EQ(pois.freeWorkstationCount(), 2);
}

VF_TEST(everyWorkstationBlockMapsBackToTheProfessionItGrants) {
    for (int raw = 2; raw < static_cast<int>(VillagerProfession::Count); ++raw) {
        VillagerProfession profession = static_cast<VillagerProfession>(raw);
        Block station = workstationForProfession(profession);
        CHECK(station != Block::Air);
        CHECK(isWorkstationBlock(station));
        CHECK(professionForWorkstation(station) == profession);
        CHECK(professionEmployable(profession));
    }
    CHECK(workstationForProfession(VillagerProfession::Nitwit) == Block::Air);
    CHECK(professionForWorkstation(Block::Stone) == VillagerProfession::Unemployed);
    CHECK(!isWorkstationBlock(Block::Bed));
    CHECK(!professionEmployable(VillagerProfession::Nitwit));
}

VF_TEST(anUnemployedVillagerWalksToAFreeLecternAndBecomesALibrarian) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 77u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;

    runTicks(world, villager, pois, clock, schedule, false, 1);
    CHECK(villager.activity == VillagerActivity::SeekWorkstation);
    CHECK(villager.state == VillagerState::Travelling);

    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK(villager.profession == VillagerProfession::Librarian);
    CHECK(villager.workstationIndex >= 0);
    CHECK_EQ(pois.at(villager.workstationIndex).ownerId, villager.id);
    CHECK(pois.at(villager.workstationIndex).pos == Vec3i(-4, walkY, 0));
    CHECK(villager.activity == VillagerActivity::Work);
    CHECK(villager.state == VillagerState::AtTarget);
    CHECK(villager.workTicks > 0);
    CHECK(villager.bedIndex >= 0);
}

VF_TEST(aClaimedWorkstationIsInvisibleToTheNextVillagerLookingForWork) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager first = makeWorker(1, -1, 0, 3u);
    Villager second = makeWorker(2, -2, 0, 9u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;

    runTicks(world, first, pois, clock, schedule, false, 400);
    CHECK(first.profession == VillagerProfession::Librarian);

    runTicks(world, second, pois, clock, schedule, false, 800);
    CHECK(second.profession == VillagerProfession::Farmer);
    CHECK(second.workstationIndex != first.workstationIndex);
    CHECK_EQ(pois.freeWorkstationCount(), 0);
    CHECK_EQ(pois.freeBedCount(), 0);

    Villager third = makeWorker(3, 1, 1, 11u);
    runTicks(world, third, pois, clock, schedule, false, 200);
    CHECK(third.profession == VillagerProfession::Unemployed);
    CHECK_EQ(third.workstationIndex, -1);
    CHECK_EQ(third.bedIndex, -1);
    CHECK(third.activity == VillagerActivity::SeekWorkstation);
    CHECK(third.state == VillagerState::Idle);
}

VF_TEST(villagerWalksTheWholeDayFromBedToWorkToTheBellAndBackToBed) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 5u);
    VillagerSchedule schedule;
    TimeOfDay clock(0);

    std::vector<VillagerActivity> seen;
    Vec3i bellStand = pois.at(pois.bellIndex()).standCell;
    bool reachedBellAtNoon = false;

    for (WorldTick step = 0; step < 24000; step += 20) {
        clock.setTick(step);
        runTicks(world, villager, pois, clock, schedule, false, 20);
        if (seen.empty() || seen.back() != villager.activity) seen.push_back(villager.activity);
        if (villager.activity == VillagerActivity::Meet &&
            villager.state == VillagerState::AtTarget) {
            reachedBellAtNoon = true;
            CHECK(horizontalGap(villager.body.position, bellStand) < 0.01f);
        }
    }

    CHECK(reachedBellAtNoon);
    CHECK(seen.size() >= 7);
    CHECK(seen[0] == VillagerActivity::Sleep);
    CHECK(seen[1] == VillagerActivity::Idle);
    CHECK(seen[2] == VillagerActivity::SeekWorkstation);
    CHECK(seen[3] == VillagerActivity::Work);
    CHECK(seen[4] == VillagerActivity::Meet);
    CHECK(seen[5] == VillagerActivity::Work);
    CHECK(seen.back() == VillagerActivity::Sleep);

    CHECK(villager.profession == VillagerProfession::Librarian);
    CHECK(villager.workTicks > 1000);
    CHECK(villager.bedIndex >= 0);
    CHECK(villager.state == VillagerState::AtTarget);
    CHECK(horizontalGap(villager.body.position, pois.at(villager.bedIndex).standCell) < 0.01f);
}

VF_TEST(breakingTheWorkstationDemotesAFreshVillagerBackToUnemployed) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 21u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;
    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK(villager.profession == VillagerProfession::Librarian);
    CHECK_EQ(villager.level, 1);

    world.setBlock(-4, walkY, 0, Block::Air);
    CHECK_EQ(pois.revalidate(world), 1);

    runTicks(world, villager, pois, clock, schedule, false, 1);
    CHECK(villager.profession == VillagerProfession::Unemployed);
    CHECK_EQ(villager.workstationIndex, -1);
    CHECK_EQ(villager.experience, 0);

    runTicks(world, villager, pois, clock, schedule, false, 800);
    CHECK(villager.profession == VillagerProfession::Farmer);
    CHECK(pois.at(villager.workstationIndex).block == Block::Composter);
}

VF_TEST(aVillagerPastLevelOneKeepsItsTradeWhenTheWorkstationIsBroken) {
    World world = makeVillage();
    placePoi(world, -6, 0, Block::Lectern);
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 31u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;
    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK(villager.profession == VillagerProfession::Librarian);

    CHECK(grantVillagerExperience(villager, 20));
    CHECK_EQ(villager.level, 2);

    world.setBlock(-4, walkY, 0, Block::Air);
    pois.revalidate(world);
    runTicks(world, villager, pois, clock, schedule, false, 1);
    CHECK(villager.profession == VillagerProfession::Librarian);
    CHECK_EQ(villager.workstationIndex, -1);
    CHECK_EQ(villager.experience, 20);

    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK(villager.workstationIndex >= 0);
    CHECK(pois.at(villager.workstationIndex).pos == Vec3i(-6, walkY, 0));
    CHECK(villager.profession == VillagerProfession::Librarian);
}

VF_TEST(experienceThresholdsPromoteTheVillagerThroughAllFiveLevels) {
    CHECK_EQ(villagerExperienceForLevel(1), 0);
    CHECK_EQ(villagerLevelForExperience(0), 1);
    CHECK_EQ(villagerLevelForExperience(villagerExperienceForLevel(5)), villagerMaxLevel);

    Villager villager = makeWorker(1, 0, 0, 4u);
    int reachedLevels = 1;
    for (int step = 0; step < 60; ++step) {
        if (grantVillagerExperience(villager, 5)) ++reachedLevels;
        CHECK(villager.level <= villagerMaxLevel);
    }
    CHECK_EQ(reachedLevels, villagerMaxLevel);
    CHECK_EQ(villager.level, villagerMaxLevel);
    CHECK(!grantVillagerExperience(villager, 0));

    for (int level = 2; level <= villagerMaxLevel; ++level)
        CHECK(villagerExperienceForLevel(level) > villagerExperienceForLevel(level - 1));
}

VF_TEST(panicOverridesTheScheduleAndSendsTheVillagerBackToItsBed) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 12u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;
    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK(villager.activity == VillagerActivity::Work);
    CHECK(villager.profession == VillagerProfession::Librarian);

    runTicks(world, villager, pois, clock, schedule, true, 1);
    CHECK(villager.activity == VillagerActivity::Panic);

    runTicks(world, villager, pois, clock, schedule, true, 600);
    CHECK(villager.state == VillagerState::AtTarget);
    CHECK(horizontalGap(villager.body.position, pois.at(villager.bedIndex).standCell) < 0.01f);
    int workTicksUnderThreat = villager.workTicks;

    runTicks(world, villager, pois, clock, schedule, true, 100);
    CHECK_EQ(villager.workTicks, workTicksUnderThreat);

    runTicks(world, villager, pois, clock, schedule, false, 1);
    CHECK(villager.activity == VillagerActivity::Work);
}

VF_TEST(deathReleasesTheBedAndWorkstationForSomebodyElse) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager villager = makeWorker(1, -1, 0, 55u);
    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;
    runTicks(world, villager, pois, clock, schedule, false, 400);
    CHECK_EQ(pois.freeWorkstationCount(), 1);
    CHECK_EQ(pois.freeBedCount(), 1);

    CHECK(!damageVillager(villager, pois, 10.0f));
    CHECK(villager.alive);
    CHECK(damageVillager(villager, pois, 10.0f));
    CHECK(!villager.alive);
    CHECK(villager.state == VillagerState::Dead);
    CHECK_EQ(villager.workstationIndex, -1);
    CHECK_EQ(pois.freeWorkstationCount(), 2);
    CHECK_EQ(pois.freeBedCount(), 2);

    CHECK(!damageVillager(villager, pois, 10.0f));

    Villager heir = makeWorker(2, -1, 0, 56u);
    runTicks(world, heir, pois, clock, schedule, false, 400);
    CHECK(heir.profession == VillagerProfession::Librarian);
}

VF_TEST(neighboursStandingTogetherTradeGossipBothWays) {
    Villager first = makeWorker(1, 0, 0, 61u);
    Villager second = makeWorker(2, 1, 0, 62u);
    Villager distant = makeWorker(3, 15, 0, 63u);

    first.gossip.add(GossipKind::MinorNegative, gossipMinorNegativeGain * 4);
    second.gossip.add(GossipKind::Trading, gossipTradingGain * 6);

    CHECK(exchangeVillagerGossip(first, second));
    CHECK(second.gossip.valueOf(GossipKind::MinorNegative) > 0);
    CHECK(first.gossip.valueOf(GossipKind::Trading) > 0);

    CHECK(!exchangeVillagerGossip(first, distant));
    CHECK_EQ(distant.gossip.valueOf(GossipKind::MinorNegative), 0);

    std::vector<Villager> crowd{first, second, distant};
    CHECK_EQ(spreadVillageGossip(crowd), 1);
    CHECK_EQ(crowd[2].gossip.valueOf(GossipKind::MinorNegative), 0);

    crowd[2].body.position = {1.5f, static_cast<float>(walkY), 0.5f};
    CHECK(spreadVillageGossip(crowd) >= 2);
    CHECK(crowd[2].gossip.valueOf(GossipKind::MinorNegative) > 0);
    CHECK(crowd[2].gossip.valueOf(GossipKind::MinorNegative) <=
          gossipTuning(GossipKind::MinorNegative).shareCap);
}

VF_TEST(censusCountsBedsWorkstationsAndFoodAndGatesVillageGrowth) {
    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    std::vector<Villager> villagers{makeWorker(1, 1, 1, 71u), makeWorker(2, 2, 1, 72u)};
    VillageCensus empty = takeVillageCensus(villagers, pois, villageCenter, 12);
    CHECK_EQ(empty.villagers, 2);
    CHECK_EQ(empty.living, 2);
    CHECK_EQ(empty.beds, 2);
    CHECK_EQ(empty.freeBeds, 2);
    CHECK_EQ(empty.workstations, 2);
    CHECK_EQ(empty.foodStock, 0);
    CHECK(!empty.canGrow());

    villagers[0].food = villagerBreedingFoodCost;
    villagers[1].food = villagerBreedingFoodCost;
    VillageCensus fed = takeVillageCensus(villagers, pois, villageCenter, 12);
    CHECK_EQ(fed.willingBreeders, 2);
    CHECK_EQ(fed.foodStock, villagerBreedingFoodCost * 2);
    CHECK(fed.canGrow());

    CHECK(assignVillagerBed(villagers[0], pois));
    CHECK(assignVillagerBed(villagers[1], pois));
    VillageCensus full = takeVillageCensus(villagers, pois, villageCenter, 12);
    CHECK_EQ(full.freeBeds, 0);
    CHECK(!full.canGrow());
    CHECK_EQ(full.capacity(), 2);
}

VF_TEST(breedingSpendsFoodAndFillsTheFreeBeds) {
    World world = makeVillage();
    placePoi(world, 6, 0, Block::Bed);
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);
    CHECK_EQ(pois.bedCount(), 3);

    std::vector<Villager> villagers{makeWorker(1, 1, 1, 81u), makeWorker(2, 2, 1, 82u)};
    villagers[0].food = villagerBreedingFoodCost * 2;
    villagers[1].food = villagerBreedingFoodCost * 2;

    uint32_t rng = 4242u;
    int born = breedVillagers(villagers, pois, villageCenter, 12, rng);
    CHECK_EQ(born, 2);
    CHECK_EQ(static_cast<int>(villagers.size()), 4);
    CHECK_EQ(villagers[0].food, 0);
    CHECK_EQ(villagers[1].food, 0);
    CHECK_EQ(pois.freeBedCount(), 1);
    CHECK_EQ(villagers[2].id, 3);
    CHECK_EQ(villagers[3].id, 4);
    CHECK(villagers[2].bedIndex >= 0);
    CHECK(villagers[3].bedIndex >= 0);
    CHECK(villagers[2].bedIndex != villagers[3].bedIndex);

    CHECK_EQ(breedVillagers(villagers, pois, villageCenter, 12, rng), 0);
}

VF_TEST(nitwitsNeverTakeAWorkstationEvenWhenOneIsFree) {
    int nitwits = 0;
    int found = -1;
    for (int id = 1; id <= 40; ++id) {
        Villager candidate = makeVillager(id, {0.5f, static_cast<float>(walkY), 0.5f}, 900u);
        if (candidate.profession == VillagerProfession::Nitwit) {
            ++nitwits;
            if (found < 0) found = id;
        }
    }
    CHECK(nitwits > 0);
    CHECK(nitwits < 40);

    World world = makeVillage();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 12, 4);

    Villager nitwit = makeVillager(found, {-1.5f, static_cast<float>(walkY), 0.5f}, 900u);
    CHECK(nitwit.profession == VillagerProfession::Nitwit);
    CHECK(!nitwit.employed());

    TimeOfDay clock = clockAtFraction(0.35);
    VillagerSchedule schedule;
    runTicks(world, nitwit, pois, clock, schedule, false, 400);
    CHECK(nitwit.profession == VillagerProfession::Nitwit);
    CHECK_EQ(nitwit.workstationIndex, -1);
    CHECK(nitwit.activity == VillagerActivity::Idle);
    CHECK_EQ(pois.freeWorkstationCount(), 2);
    CHECK(nitwit.bedIndex >= 0);
    CHECK(!assignVillagerWorkstation(nitwit, pois));
}
