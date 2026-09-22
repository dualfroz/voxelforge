#include "testHarness.h"
#include "vf/Fluid.h"

using namespace vf;

namespace {

void solidLayer(World& world, int y, Vec3i lo, Vec3i hi) {
    for (int x = lo.x; x <= hi.x; ++x)
        for (int z = lo.z; z <= hi.z; ++z)
            world.setBlock(x, y, z, Block::Stone);
}

uint8_t maxWater(const FluidSim& sim) {
    uint8_t hi = 0;
    for (uint8_t v : sim.levels()) hi = v > hi ? v : hi;
    return hi;
}

}

VF_TEST(fluidSpreadsWithDecreasingLevelsSymmetrically) {
    World world;
    solidLayer(world, 0, {0, 0, 0}, {12, 0, 12});
    FluidSim sim(world, {0, 0, 0}, {13, 3, 13});
    sim.addSource(6, 1, 6);
    int steps = sim.settle(50);
    CHECK(steps < 50);

    CHECK_EQ(static_cast<int>(sim.level(6, 1, 6)), fluidMax);
    CHECK_EQ(static_cast<int>(sim.level(7, 1, 6)), fluidMax - 1);
    CHECK_EQ(static_cast<int>(sim.level(5, 1, 6)), fluidMax - 1);
    CHECK_EQ(static_cast<int>(sim.level(6, 1, 7)), fluidMax - 1);
    CHECK_EQ(static_cast<int>(sim.level(9, 1, 6)), fluidMax - 3);
    CHECK_EQ(static_cast<int>(sim.level(10, 1, 10)), 0);
    CHECK_EQ(static_cast<int>(sim.level(6, 2, 6)), 0);
}

VF_TEST(fluidFallsThroughHoleAndSpreadsBelow) {
    World world;
    solidLayer(world, 0, {0, 0, 0}, {12, 0, 12});
    solidLayer(world, 4, {0, 4, 0}, {12, 4, 12});
    world.setBlock(6, 4, 6, Block::Air);
    FluidSim sim(world, {0, 0, 0}, {13, 6, 13});
    sim.addSource(3, 5, 6);
    sim.settle(80);

    CHECK_EQ(static_cast<int>(sim.level(6, 2, 6)), fluidMax);
    CHECK_EQ(static_cast<int>(sim.level(6, 1, 6)), fluidMax);
    CHECK(sim.level(7, 1, 6) > 0);
    CHECK(sim.level(8, 1, 6) > 0);
}

VF_TEST(fluidDrainsAfterSourceRemoved) {
    World world;
    solidLayer(world, 0, {0, 0, 0}, {12, 0, 12});
    FluidSim sim(world, {0, 0, 0}, {13, 3, 13});
    sim.addSource(6, 1, 6);
    sim.settle(50);
    CHECK(maxWater(sim) == fluidMax);

    sim.removeSource(6, 1, 6);
    int steps = sim.settle(50);
    CHECK(steps < 50);
    CHECK_EQ(static_cast<int>(maxWater(sim)), 0);
}

VF_TEST(fluidIsDeterministic) {
    World worldA, worldB;
    solidLayer(worldA, 0, {0, 0, 0}, {12, 0, 12});
    solidLayer(worldB, 0, {0, 0, 0}, {12, 0, 12});
    FluidSim a(worldA, {0, 0, 0}, {13, 3, 13});
    FluidSim b(worldB, {0, 0, 0}, {13, 3, 13});
    a.addSource(6, 1, 6);
    b.addSource(6, 1, 6);
    a.settle(50);
    b.settle(50);
    CHECK(a.levels() == b.levels());
}

VF_TEST(fluidBlockedBySolidWall) {
    World world;
    solidLayer(world, 0, {0, 0, 0}, {12, 0, 12});
    for (int y = 1; y <= 2; ++y)
        for (int z = 0; z <= 12; ++z)
            world.setBlock(8, y, z, Block::Stone);
    FluidSim sim(world, {0, 0, 0}, {13, 3, 13});
    sim.addSource(6, 1, 6);
    sim.settle(50);
    CHECK(sim.level(7, 1, 6) > 0);
    CHECK_EQ(static_cast<int>(sim.level(9, 1, 6)), 0);
}
