#include "testHarness.h"
#include "vf/Dispenser.h"

using namespace vf;

VF_TEST(dispenserFiresOnlyOnRisingEdge) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::Stone, 5);
    store.ensure(pos + Vec3i{1, 0, 0}, dispenserSlots);   // container in front

    DispenserState state;
    CHECK(tickDropper(world, pos, state, store, 0).action == DispenseAction::None);
    CHECK(tickDropper(world, pos, state, store, 15).action == DispenseAction::InsertedToContainer);
    CHECK(tickDropper(world, pos, state, store, 15).action == DispenseAction::None);
    CHECK(tickDropper(world, pos, state, store, 0).action == DispenseAction::None);
    CHECK(tickDropper(world, pos, state, store, 15).action == DispenseAction::InsertedToContainer);

    CHECK_EQ(store.at(pos + Vec3i{1, 0, 0})->totalItems(), 2);
    CHECK_EQ(store.at(pos)->totalItems(), 3);
}

VF_TEST(dispenserPlacesBlockItemIntoAirInFront) {
    World world;
    Vec3i pos{5, 5, 5};
    Vec3i front{6, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::Stone, 1);

    DispenserState state;
    DispenseResult res = tickDispenser(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::PlacedBlock);
    CHECK(world.blockAt(front.x, front.y, front.z) == Block::Stone);
    CHECK_EQ(store.at(pos)->totalItems(), 0);
}

VF_TEST(dispenserEjectsNonBlockItemIntoFrontContainer) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::IronIngot, 2);
    store.ensure(pos + Vec3i{1, 0, 0}, dispenserSlots);

    DispenserState state;
    DispenseResult res = tickDispenser(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::InsertedToContainer);
    CHECK_EQ(store.at(pos + Vec3i{1, 0, 0})->totalItems(), 1);
    CHECK_EQ(store.at(pos)->totalItems(), 1);
}

VF_TEST(dispenserEjectsToWorldWhenItCannotPlaceOrDeposit) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::IronIngot, 1);

    DispenserState state;
    DispenseResult res = tickDispenser(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::EjectedToWorld);
    CHECK(static_cast<int>(res.item.item) == static_cast<int>(ItemId::IronIngot));
    CHECK(world.blockAt(6, 5, 5) == Block::Air);
}

VF_TEST(dispenserThatCannotPlaceIntoOccupiedCellEjectsInstead) {
    World world;
    Vec3i pos{5, 5, 5};
    world.setBlock(6, 5, 5, Block::Stone);   // front already occupied
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::Dirt, 1);

    DispenserState state;
    DispenseResult res = tickDispenser(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::EjectedToWorld);
    CHECK(world.blockAt(6, 5, 5) == Block::Stone);   // unchanged
}

VF_TEST(dropperNeverPlacesBlocksEvenForBlockItems) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::Stone, 1);

    DispenserState state;
    DispenseResult res = tickDropper(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::EjectedToWorld);
    CHECK(world.blockAt(6, 5, 5) == Block::Air);
}

VF_TEST(dropperDepositsBlockItemIntoFrontContainer) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots).insert(ItemId::Stone, 1);
    store.ensure(pos + Vec3i{1, 0, 0}, dispenserSlots);

    DispenserState state;
    DispenseResult res = tickDropper(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::InsertedToContainer);
    CHECK_EQ(store.at(pos + Vec3i{1, 0, 0})->totalItems(), 1);
}

VF_TEST(emptyDispenserDoesNothingOnEdge) {
    World world;
    Vec3i pos{5, 5, 5};
    ContainerStore store;
    store.ensure(pos, dispenserSlots);

    DispenserState state;
    DispenseResult res = tickDispenser(world, pos, state, store, 15);
    CHECK(res.action == DispenseAction::None);
}

VF_TEST(slotSelectionIsDeterministicAndAlwaysHitsAFilledSlot) {
    Container inventory(9);
    inventory.setSlot(0, ItemSlot{ItemId::Stone, 10, 0});
    inventory.setSlot(3, ItemSlot{ItemId::Dirt, 10, 0});
    inventory.setSlot(7, ItemSlot{ItemId::Sand, 10, 0});

    std::uint32_t rngA = 0x1234abcdu;
    std::uint32_t rngB = 0x1234abcdu;
    for (int i = 0; i < 8; ++i) {
        int a = chooseNonEmptySlot(inventory, rngA);
        int b = chooseNonEmptySlot(inventory, rngB);
        CHECK_EQ(a, b);
        CHECK(!inventory.slot(a).empty());
    }
}
