#include "testHarness.h"
#include "vf/Inventory.h"

using namespace vf;

VF_TEST(hotbarStartsOnFirstSlot) {
    Hotbar hotbar;
    CHECK_EQ(hotbar.selectedIndex(), 0);
    CHECK(hotbar.selectedBlock() == Block::Grass);
}

VF_TEST(hotbarScrollWraps) {
    Hotbar hotbar;
    hotbar.scroll(-1);
    CHECK_EQ(hotbar.selectedIndex(), hotbarSize - 1);
    hotbar.scroll(1);
    CHECK_EQ(hotbar.selectedIndex(), 0);
    hotbar.select(hotbarSize + 1);
    CHECK_EQ(hotbar.selectedIndex(), 1);
    hotbar.select(-1);
    CHECK_EQ(hotbar.selectedIndex(), hotbarSize - 1);
}

VF_TEST(hotbarConsumeEmptiesStack) {
    Hotbar hotbar;
    hotbar.select(2);   // stone, 64
    int drawn = 0;
    while (hotbar.consumeSelected()) ++drawn;
    CHECK_EQ(drawn, 64);
    CHECK(hotbar.slot(2).empty());
    CHECK(!hotbar.consumeSelected());
}

VF_TEST(hotbarAddStacksAndFillsEmpty) {
    Hotbar hotbar;
    hotbar.select(2);
    while (hotbar.consumeSelected()) {}
    CHECK(hotbar.slot(2).empty());

    hotbar.add(Block::Stone, 5);           // matches the drained stone slot
    CHECK_EQ(hotbar.slot(2).count, 5);
    CHECK(hotbar.slot(2).block == Block::Stone);

    hotbar.select(2);
    while (hotbar.consumeSelected()) {}    // empty it fully again
    hotbar.add(Block::CoalOre, 3);         // no matching slot -> first empty one
    bool placed = false;
    for (int i = 0; i < hotbarSize; ++i)
        if (hotbar.slot(i).block == Block::CoalOre && hotbar.slot(i).count == 3) placed = true;
    CHECK(placed);
}
