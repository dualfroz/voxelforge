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

// A small village: a bell in the middle, two beds, a lectern and a composter.
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

} // namespace

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

