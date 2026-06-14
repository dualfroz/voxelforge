#include "testHarness.h"
#include "vf/Container.h"

using namespace vf;

VF_TEST(insertMergesIntoMatchingStackThenSpillsToNewSlot) {
    Container box(3);
    CHECK_EQ(box.insert(ItemId::Stone, 40), 0);
    CHECK_EQ(box.insert(ItemId::Stone, 40), 0);
    CHECK_EQ(box.usedSlots(), 2);
    CHECK_EQ(box.totalItems(), 80);
    CHECK_EQ(box.slot(0).count, 64);
    CHECK_EQ(box.slot(1).count, 16);
}

VF_TEST(insertReturnsLeftoverWhenContainerCannotHoldAll) {
    Container box(1);
    int leftover = box.insert(ItemId::Dirt, 100);
    CHECK_EQ(leftover, 36);
    CHECK_EQ(box.slot(0).count, 64);
    CHECK(box.isFull());
}

VF_TEST(isEmptyAndIsFullReflectContents) {
    Container box(2);
    CHECK(box.isEmpty());
    CHECK(!box.isFull());
    box.insert(ItemId::Stone, 64);
    CHECK(!box.isEmpty());
    CHECK(!box.isFull());
    box.insert(ItemId::Stone, 64);
    CHECK(box.isFull());
}

VF_TEST(extractPullsMatchingItemsAcrossSlots) {
    Container box(4);
    box.insert(ItemId::Cobblestone, 64);
    box.insert(ItemId::Cobblestone, 20);
    ItemSlot pulled = box.extract(ItemId::Cobblestone, 70);
    CHECK_EQ(static_cast<int>(pulled.item), static_cast<int>(ItemId::Cobblestone));
    CHECK_EQ(pulled.count, 70);
    CHECK_EQ(box.totalItems(), 14);
}

VF_TEST(extractReturnsEmptyWhenItemAbsent) {
    Container box(2);
    box.insert(ItemId::Stone, 10);
    ItemSlot pulled = box.extract(ItemId::Dirt, 5);
    CHECK(pulled.empty());
    CHECK_EQ(box.totalItems(), 10);
}

VF_TEST(transferMovesRequestedAmountAndStopsAtDestinationCapacity) {
    Container src(2);
    Container dst(1);
    src.insert(ItemId::Stone, 50);
    int moved = src.transferTo(dst, 30);
    CHECK_EQ(moved, 30);
    CHECK_EQ(dst.totalItems(), 30);
    CHECK_EQ(src.totalItems(), 20);
}

VF_TEST(transferBlockedWhenDestinationFull) {
    Container src(1);
    Container dst(1);
    dst.insert(ItemId::Stone, 64);
    src.insert(ItemId::Stone, 10);
    int moved = src.transferTo(dst, 5);
    CHECK_EQ(moved, 0);
    CHECK_EQ(dst.totalItems(), 64);
    CHECK_EQ(src.totalItems(), 10);
}

VF_TEST(comparatorSignalScalesWithFill) {
    Container empty(1);
    CHECK_EQ(empty.comparatorSignal(), 0);

    Container half(1);
    half.insert(ItemId::Stone, 32);
    CHECK_EQ(half.comparatorSignal(), 8);

    Container full(1);
    full.insert(ItemId::Stone, 64);
    CHECK_EQ(full.comparatorSignal(), 15);
}

VF_TEST(comparatorSignalAveragesAcrossSlots) {
    Container box(4);
    box.insert(ItemId::Stone, 64);
    CHECK(box.comparatorSignal() > 0);
    CHECK(box.comparatorSignal() < 8);
}

VF_TEST(toolsDoNotStackAndKeepDurability) {
    Container box(4);
    ItemSlot pick = makeToolStack(ItemId::WoodPickaxe);
    ItemSlot copyA = pick;
    ItemSlot copyB = pick;
    box.insert(copyA);
    box.insert(copyB);
    CHECK_EQ(box.usedSlots(), 2);
    CHECK(box.slot(0).durability > 0);
}

VF_TEST(extractSlotSplitsExactCount) {
    Container box(2);
    box.insert(ItemId::Sand, 40);
    ItemSlot piece = box.extractSlot(0, 15);
    CHECK_EQ(piece.count, 15);
    CHECK_EQ(box.slot(0).count, 25);
}

VF_TEST(storeCreatesAndAddressesContainersByCell) {
    ContainerStore store;
    CHECK(!store.has({2, 3, 4}));
    Container& box = store.ensure({2, 3, 4}, 5);
    box.insert(ItemId::Stone, 5);
    CHECK(store.has({2, 3, 4}));
    CHECK_EQ(store.at({2, 3, 4})->totalItems(), 5);
    CHECK(store.at({9, 9, 9}) == nullptr);
    store.remove({2, 3, 4});
    CHECK(!store.has({2, 3, 4}));
}
