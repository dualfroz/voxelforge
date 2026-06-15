#include "testHarness.h"
#include "vf/BlockTick.h"

using namespace vf;

namespace {

int countBlock(World& world, Block target, int x0, int x1, int y0, int y1, int z0, int z1) {
    int count = 0;
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (world.blockAt(x, y, z) == target) ++count;
    return count;
}

} // namespace

VF_TEST(scheduledTickFiresAfterExactDelay) {
    World world;
    BlockTickScheduler scheduler(1, 0);
    scheduler.schedule({3, 3, 3}, 4);

    for (int i = 0; i < 3; ++i) CHECK(scheduler.tick(world).empty());

    std::vector<Vec3i> fired = scheduler.tick(world);
    CHECK_EQ(fired.size(), static_cast<std::size_t>(1));
    CHECK(fired[0] == Vec3i(3, 3, 3));
}

VF_TEST(scheduledTicksFireInDueThenInsertionOrder) {
    World world;
    BlockTickScheduler scheduler(1, 0);
    scheduler.schedule({1, 1, 1}, 3);
    scheduler.schedule({2, 2, 2}, 1);
    scheduler.schedule({3, 3, 3}, 1);

    std::vector<Vec3i> firstTick = scheduler.tick(world);
    CHECK_EQ(firstTick.size(), static_cast<std::size_t>(2));
    CHECK(firstTick[0] == Vec3i(2, 2, 2));
    CHECK(firstTick[1] == Vec3i(3, 3, 3));

    CHECK(scheduler.tick(world).empty());

    std::vector<Vec3i> thirdTick = scheduler.tick(world);
    CHECK_EQ(thirdTick.size(), static_cast<std::size_t>(1));
    CHECK(thirdTick[0] == Vec3i(1, 1, 1));
}

VF_TEST(cancelRemovesAPendingTickBeforeItFires) {
    World world;
    BlockTickScheduler scheduler(1, 0);
    scheduler.schedule({5, 5, 5}, 2);
    CHECK(scheduler.isScheduled({5, 5, 5}));

    scheduler.cancel({5, 5, 5});
    CHECK(!scheduler.isScheduled({5, 5, 5}));

    scheduler.tick(world);
    std::vector<Vec3i> fired = scheduler.tick(world);
    CHECK(fired.empty());
}

VF_TEST(sandFallsOneCellPerTickUntilItLands) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    world.setBlock(0, 5, 0, Block::Sand);
    BlockTickScheduler scheduler(1, 0);

    scheduler.tick(world);
    CHECK(world.blockAt(0, 4, 0) == Block::Sand);
    CHECK(world.blockAt(0, 5, 0) == Block::Air);

    scheduler.tick(world);
    CHECK(world.blockAt(0, 3, 0) == Block::Sand);

    scheduler.tick(world);
    CHECK(world.blockAt(0, 2, 0) == Block::Sand);

    scheduler.tick(world);
    CHECK(world.blockAt(0, 1, 0) == Block::Sand);   // landed on the stone

    scheduler.tick(world);
    CHECK(world.blockAt(0, 1, 0) == Block::Sand);   // stays put once supported
    CHECK_EQ(countBlock(world, Block::Sand, -2, 2, 0, 6, -2, 2), 1);
}

VF_TEST(gravelBehavesLikeSandAndSupportedBlocksDoNotFall) {
    World world;
    world.setBlock(1, 0, 1, Block::Stone);
    world.setBlock(1, 1, 1, Block::Gravel);
    BlockTickScheduler scheduler(1, 0);

    scheduler.tick(world);
    scheduler.tick(world);
    CHECK(world.blockAt(1, 1, 1) == Block::Gravel);   // already supported, never moved
}

VF_TEST(stackedFallingBlocksMaintainRelativeOrderWithoutGaps) {
    World world;
    world.setBlock(2, 0, 2, Block::Stone);
    world.setBlock(2, 5, 2, Block::Sand);
    world.setBlock(2, 6, 2, Block::Gravel);
    BlockTickScheduler scheduler(1, 0);

    for (int i = 0; i < 5; ++i) scheduler.tick(world);

    CHECK(world.blockAt(2, 1, 2) == Block::Sand);
    CHECK(world.blockAt(2, 2, 2) == Block::Gravel);
    CHECK(world.blockAt(2, 3, 2) == Block::Air);
}

VF_TEST(randomTicksSampleConfiguredCountPerLoadedChunk) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    world.setBlock(20, 0, 0, Block::Stone);   // second chunk along x
    BlockTickScheduler scheduler(42, 5);

    scheduler.tick(world);
    CHECK_EQ(scheduler.lastRandomTicks().size(), static_cast<std::size_t>(10));
    for (const Vec3i& pos : scheduler.lastRandomTicks()) {
        CHECK(pos.x >= -16 && pos.x < 32);
        CHECK(pos.y >= 0 && pos.y < 16);
        CHECK(pos.z >= 0 && pos.z < 16);
    }
}

VF_TEST(randomTickSamplingIsDeterministicForAFixedSeed) {
    World worldA, worldB;
    worldA.setBlock(0, 0, 0, Block::Stone);
    worldB.setBlock(0, 0, 0, Block::Stone);
    BlockTickScheduler a(99, 4);
    BlockTickScheduler b(99, 4);

    for (int i = 0; i < 3; ++i) {
        a.tick(worldA);
        b.tick(worldB);
        CHECK(a.lastRandomTicks() == b.lastRandomTicks());
    }
}

VF_TEST(differentSeedsProduceDifferentRandomTickSequences) {
    World worldA, worldB;
    worldA.setBlock(0, 0, 0, Block::Stone);
    worldB.setBlock(0, 0, 0, Block::Stone);
    BlockTickScheduler a(1, 6);
    BlockTickScheduler b(2, 6);

    a.tick(worldA);
    b.tick(worldB);
    CHECK(a.lastRandomTicks() != b.lastRandomTicks());
}

VF_TEST(growthHookFiresOnRandomTicksAndGrowsExposedDirtToGrass) {
    World world;
    world.setBlock(3, 3, 3, Block::Dirt);
    world.setBlock(3, 3, 4, Block::Dirt);
    world.setBlock(3, 4, 4, Block::Stone);   // covered dirt must not grow

    BlockTickScheduler scheduler(7, 200);
    scheduler.setRandomTickHook([](World& w, const Vec3i& pos, Block b) {
        if (b == Block::Dirt) tryGrowGrass(w, pos);
    });

    bool grewExposed = false;
    for (int i = 0; i < 100 && !grewExposed; ++i) {
        scheduler.tick(world);
        if (world.blockAt(3, 3, 3) == Block::Grass) grewExposed = true;
    }
    CHECK(grewExposed);
    CHECK(world.blockAt(3, 3, 4) == Block::Dirt);   // stays dirt, blocked by the stone above
}

VF_TEST(tryGrowGrassRejectsNonDirtAndCoveredDirt) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    CHECK(!tryGrowGrass(world, {0, 0, 0}));

    world.setBlock(0, 1, 0, Block::Dirt);
    world.setBlock(0, 2, 0, Block::Stone);
    CHECK(!tryGrowGrass(world, {0, 1, 0}));
    CHECK(world.blockAt(0, 1, 0) == Block::Dirt);

    world.setBlock(0, 2, 0, Block::Air);
    CHECK(tryGrowGrass(world, {0, 1, 0}));
    CHECK(world.blockAt(0, 1, 0) == Block::Grass);
}

VF_TEST(currentTickAdvancesByOnePerCall) {
    World world;
    BlockTickScheduler scheduler(1, 0);
    CHECK_EQ(scheduler.currentTick(), 0);
    scheduler.tick(world);
    scheduler.tick(world);
    scheduler.tick(world);
    CHECK_EQ(scheduler.currentTick(), 3);
}
