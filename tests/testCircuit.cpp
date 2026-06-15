#include "testHarness.h"
#include "vf/Circuit.h"

using namespace vf;

namespace {

void wireLine(World& world, int x0, int x1, int y, int z) {
    for (int x = x0; x <= x1; ++x) world.setBlock(x, y, z, Block::RedstoneWire);
}

} // namespace

VF_TEST(wirePropagatesWithFalloffToZeroAfterFifteenBlocks) {
    World world;
    world.setBlock(0, 0, 0, Block::RedstoneBlock);
    wireLine(world, 1, 16, 0, 0);
    RedstoneCircuit circuit(world, {0, 0, 0}, {17, 1, 1});
    circuit.tick();

    for (int x = 1; x <= 15; ++x) CHECK_EQ(circuit.powerAt(x, 0, 0), redstoneMaxPower - (x - 1));
    CHECK_EQ(circuit.powerAt(16, 0, 0), 0);
}

VF_TEST(wireDoesNotCrossThroughSolidBlocks) {
    World world;
    world.setBlock(0, 0, 0, Block::RedstoneBlock);
    wireLine(world, 1, 3, 0, 0);
    world.setBlock(4, 0, 0, Block::Stone);
    wireLine(world, 5, 7, 0, 0);
    RedstoneCircuit circuit(world, {0, 0, 0}, {8, 1, 1});
    circuit.tick();

    CHECK(circuit.powerAt(3, 0, 0) > 0);
    CHECK_EQ(circuit.powerAt(5, 0, 0), 0);
    CHECK_EQ(circuit.powerAt(6, 0, 0), 0);
}

VF_TEST(leverTogglesWireOnAndOff) {
    World world;
    world.setBlock(5, 0, 5, Block::Lever);
    world.setBlock(6, 0, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});

    circuit.tick();
    CHECK_EQ(circuit.powerAt(6, 0, 5), 0);
    CHECK(!circuit.leverOn({5, 0, 5}));

    circuit.setLever({5, 0, 5}, true);
    circuit.tick();
    CHECK(circuit.leverOn({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), redstoneMaxPower);

    circuit.setLever({5, 0, 5}, false);
    circuit.tick();
    CHECK_EQ(circuit.powerAt(6, 0, 5), 0);
}

VF_TEST(buttonPowersThenExpires) {
    World world;
    world.setBlock(5, 0, 5, Block::Button);
    world.setBlock(6, 0, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.pressButton({5, 0, 5}, 3);

    circuit.tick();
    CHECK(circuit.buttonActive({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), redstoneMaxPower);

    circuit.tick();
    CHECK(circuit.buttonActive({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), redstoneMaxPower);

    circuit.tick();
    CHECK(!circuit.buttonActive({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), 0);

    circuit.tick();
    CHECK(!circuit.buttonActive({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), 0);
}

VF_TEST(torchInvertsThePowerStateOfItsMountBlock) {
    World world;
    world.setBlock(4, 0, 5, Block::RedstoneBlock);
    world.setBlock(5, 0, 5, Block::Stone);
    world.setBlock(5, 1, 5, Block::RedstoneTorch);
    world.setBlock(6, 1, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});

    circuit.tick();
    CHECK_EQ(circuit.powerAt(6, 1, 5), redstoneMaxPower);   // mount not yet observed as powered

    circuit.tick();
    CHECK_EQ(circuit.powerAt(6, 1, 5), 0);   // mount observed powered, torch inverted off
}

VF_TEST(torchWithUnpoweredMountStaysLitIndefinitely) {
    World world;
    world.setBlock(5, 0, 5, Block::Stone);
    world.setBlock(5, 1, 5, Block::RedstoneTorch);
    world.setBlock(6, 1, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});

    for (int i = 0; i < 5; ++i) {
        circuit.tick();
        CHECK_EQ(circuit.powerAt(6, 1, 5), redstoneMaxPower);
    }
}

VF_TEST(repeaterDelaysOutputByConfiguredTicksAndRefreshesToFull) {
    World world;
    world.setBlock(5, 0, 5, Block::Lever);
    world.setBlock(6, 0, 5, Block::Repeater);
    wireLine(world, 7, 20, 0, 5);
    RedstoneCircuit circuit(world, {0, 0, 0}, {21, 1, 10});
    circuit.setRepeater({6, 0, 5}, Face::PosX, 4);
    circuit.setLever({5, 0, 5}, true);

    circuit.tick();   // edge not visible yet (fresh field)
    CHECK(!circuit.repeaterOutputOn({6, 0, 5}));
    circuit.tick();   // edge observed, countdown starts at 4 and ticks to 3
    CHECK(!circuit.repeaterOutputOn({6, 0, 5}));
    circuit.tick();
    CHECK(!circuit.repeaterOutputOn({6, 0, 5}));
    circuit.tick();
    CHECK(!circuit.repeaterOutputOn({6, 0, 5}));
    circuit.tick();
    CHECK(circuit.repeaterOutputOn({6, 0, 5}));
    CHECK_EQ(circuit.powerAt(7, 0, 5), redstoneMaxPower);   // output refreshed to full strength
}

VF_TEST(repeaterWithMinimumDelayRespondsOneTickAfterTheEdge) {
    World world;
    world.setBlock(5, 0, 5, Block::Lever);
    world.setBlock(6, 0, 5, Block::Repeater);
    world.setBlock(7, 0, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.setRepeater({6, 0, 5}, Face::PosX, 1);
    circuit.setLever({5, 0, 5}, true);

    circuit.tick();
    CHECK(!circuit.repeaterOutputOn({6, 0, 5}));
    circuit.tick();
    CHECK(circuit.repeaterOutputOn({6, 0, 5}));
}

VF_TEST(repeaterIgnoresInputPerpendicularToItsFacing) {
    World world;
    world.setBlock(5, 0, 6, Block::Lever);          // powers from the side, not behind
    world.setBlock(5, 0, 5, Block::Repeater);
    world.setBlock(6, 0, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.setRepeater({5, 0, 5}, Face::PosX, 1);
    circuit.setLever({5, 0, 6}, true);

    for (int i = 0; i < 4; ++i) circuit.tick();
    CHECK(!circuit.repeaterOutputOn({5, 0, 5}));
    CHECK_EQ(circuit.powerAt(6, 0, 5), 0);
}

VF_TEST(comparatorCompareModePassesBackWhenSideIsLower) {
    World world;
    world.setBlock(5, 0, 5, Block::RedstoneBlock);
    world.setBlock(6, 0, 5, Block::Comparator);
    world.setBlock(7, 0, 5, Block::RedstoneWire);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.setComparator({6, 0, 5}, Face::PosX, ComparatorMode::Compare);

    circuit.tick();
    circuit.tick();
    CHECK_EQ(circuit.powerAt(7, 0, 5), redstoneMaxPower);
}

VF_TEST(comparatorCompareModeBlocksWhenSideExceedsBack) {
    World world;
    world.setBlock(3, 0, 5, Block::RedstoneBlock);
    wireLine(world, 4, 5, 0, 5);                     // back input falls off to 14 by the comparator
    world.setBlock(6, 0, 5, Block::Comparator);
    world.setBlock(7, 0, 5, Block::RedstoneWire);
    world.setBlock(6, 0, 6, Block::RedstoneBlock);   // full-strength side input
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.setComparator({6, 0, 5}, Face::PosX, ComparatorMode::Compare);

    circuit.tick();
    circuit.tick();
    CHECK_EQ(circuit.powerAt(7, 0, 5), 0);
}

VF_TEST(comparatorSubtractModeSubtractsSideFromBack) {
    World world;
    world.setBlock(4, 0, 5, Block::RedstoneBlock);
    wireLine(world, 5, 5, 0, 5);
    world.setBlock(6, 0, 5, Block::Comparator);
    world.setBlock(7, 0, 5, Block::RedstoneWire);
    world.setBlock(6, 0, 6, Block::Lever);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});
    circuit.setComparator({6, 0, 5}, Face::PosX, ComparatorMode::Subtract);
    circuit.setLever({6, 0, 6}, true);

    circuit.tick();
    circuit.tick();
    CHECK_EQ(circuit.powerAt(7, 0, 5), 0);   // full back (15) minus full side (15) = 0
}

VF_TEST(redstoneBlockIsAConstantSourceRegardlessOfTicks) {
    World world;
    world.setBlock(5, 0, 5, Block::RedstoneBlock);
    wireLine(world, 6, 8, 0, 5);
    RedstoneCircuit circuit(world, {0, 0, 0}, {9, 1, 10});

    for (int i = 0; i < 4; ++i) {
        circuit.tick();
        CHECK_EQ(circuit.powerAt(6, 0, 5), redstoneMaxPower);
        CHECK_EQ(circuit.powerAt(7, 0, 5), redstoneMaxPower - 1);
    }
}

VF_TEST(lampLitsWhenAdjacentToPowerAndDarkensWhenNot) {
    World world;
    world.setBlock(5, 0, 5, Block::Lever);
    world.setBlock(6, 0, 5, Block::RedstoneLamp);
    RedstoneCircuit circuit(world, {0, 0, 0}, {10, 3, 10});

    circuit.tick();
    circuit.applyLamps();
    CHECK(world.blockAt(6, 0, 5) == Block::RedstoneLamp);

    circuit.setLever({5, 0, 5}, true);
    circuit.tick();
    circuit.applyLamps();
    CHECK(world.blockAt(6, 0, 5) == Block::RedstoneLampLit);

    circuit.setLever({5, 0, 5}, false);
    circuit.tick();
    circuit.applyLamps();
    CHECK(world.blockAt(6, 0, 5) == Block::RedstoneLamp);
}

VF_TEST(circuitIsDeterministicAcrossIdenticalRuns) {
    World worldA, worldB;
    for (World* w : {&worldA, &worldB}) {
        w->setBlock(4, 0, 5, Block::RedstoneBlock);
        w->setBlock(5, 0, 5, Block::Stone);
        w->setBlock(5, 1, 5, Block::RedstoneTorch);
        wireLine(*w, 6, 12, 1, 5);
        w->setBlock(6, 1, 6, Block::Lever);
    }
    RedstoneCircuit a(worldA, {0, 0, 0}, {14, 3, 10});
    RedstoneCircuit b(worldB, {0, 0, 0}, {14, 3, 10});
    a.setLever({6, 1, 6}, true);
    b.setLever({6, 1, 6}, true);

    for (int i = 0; i < 6; ++i) {
        a.tick();
        b.tick();
        for (int x = 6; x <= 12; ++x) CHECK_EQ(a.powerAt(x, 1, 5), b.powerAt(x, 1, 5));
    }
}
