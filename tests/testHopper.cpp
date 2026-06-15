#include "testHarness.h"
#include "vf/Hopper.h"

using namespace vf;

VF_TEST(hopperPullsFromContainerAboveOneItemPerCycle) {
    World world;
    Vec3i pos{5, 4, 5};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 3);

    HopperState state;
    CHECK(tickHopper(world, pos, state, store));
    CHECK_EQ(store.at(pos)->totalItems(), 1);
    CHECK_EQ(store.at(pos + Vec3i{0, 1, 0})->totalItems(), 2);
}

VF_TEST(hopperPushesIntoTargetItFaces) {
    World world;
    Vec3i pos{5, 4, 5};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos, hopperSlots).insert(ItemId::Dirt, 4);
    store.ensure(pos + Vec3i{0, -1, 0}, 5);

    HopperState state;
    CHECK(tickHopper(world, pos, state, store));
    CHECK_EQ(store.at(pos + Vec3i{0, -1, 0})->totalItems(), 1);
    CHECK_EQ(store.at(pos)->totalItems(), 3);
}

VF_TEST(hopperRespectsCooldownBetweenTransfers) {
    World world;
    Vec3i pos{0, 6, 0};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 64);

    HopperState state;
    CHECK(tickHopper(world, pos, state, store));   // transfer, arms cooldown 8
    for (int i = 0; i < hopperCooldownTicks; ++i)
        CHECK(!tickHopper(world, pos, state, store));   // idle while cooldown drains
    CHECK(tickHopper(world, pos, state, store));   // fires again
    CHECK_EQ(store.at(pos)->totalItems(), 2);
}

VF_TEST(hopperDoesNothingWhenSourceEmptyAndNoPushTarget) {
    World world;
    Vec3i pos{1, 1, 1};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos, hopperSlots);
    store.ensure(pos + Vec3i{0, 1, 0}, 5);   // empty source

    HopperState state;
    CHECK(!tickHopper(world, pos, state, store));
    CHECK_EQ(store.at(pos)->totalItems(), 0);
}

VF_TEST(hopperBlockedWhenTargetIsFull) {
    World world;
    Vec3i pos{2, 2, 2};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos, hopperSlots).insert(ItemId::Stone, 5);
    Container& target = store.ensure(pos + Vec3i{0, -1, 0}, 1);
    target.insert(ItemId::Stone, 64);   // exactly full

    HopperState state;
    CHECK(!tickHopper(world, pos, state, store));
    CHECK_EQ(target.totalItems(), 64);
    CHECK_EQ(store.at(pos)->totalItems(), 5);
}

VF_TEST(hopperIgnoredWhenWorldBlockIsNotAHopper) {
    World world;
    Vec3i pos{3, 3, 3};
    world.setBlock(pos.x, pos.y, pos.z, Block::Air);

    ContainerStore store;
    store.ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 5);

    HopperState state;
    CHECK(!tickHopper(world, pos, state, store));
    CHECK(store.at(pos) == nullptr);
}

VF_TEST(hopperCanTransferReportsReadiness) {
    World world;
    Vec3i pos{7, 7, 7};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos, hopperSlots);
    HopperState state;
    CHECK(!hopperCanTransfer(pos, state, store));

    store.ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 1);
    CHECK(hopperCanTransfer(pos, state, store));
}

VF_TEST(hopperChainMovesEntireStackAcrossManyCycles) {
    World world;
    Vec3i pos{4, 4, 4};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);

    ContainerStore store;
    store.ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 5);
    store.ensure(pos + Vec3i{0, -1, 0}, 5);

    HopperState state;
    for (int tick = 0; tick < 100; ++tick)
        tickHopper(world, pos, state, store);

    CHECK_EQ(store.at(pos + Vec3i{0, 1, 0})->totalItems(), 0);
    CHECK_EQ(store.at(pos + Vec3i{0, -1, 0})->totalItems(), 5);
}
