#include "testHarness.h"
#include "vf/SlotAction.h"

#include <array>
#include <string>
#include <vector>

using namespace vf;

namespace {

const CraftingRegistry& sharedRegistry() {
    static CraftingRegistry registry;
    return registry;
}

struct Session {
    ContainerMenu menu;
    CursorStack cursor;
    DragState drag;

    explicit Session(MenuKind kind) : menu(kind, &sharedRegistry()) {}

    SlotActionResult click(ClickKind kind, int slotIndex) {
        return applySlotAction(menu, cursor, drag, clickAt(kind, slotIndex));
    }
    SlotActionResult hotbar(int slotIndex, int button) {
        return applySlotAction(menu, cursor, drag, hotbarSwapAt(slotIndex, button));
    }
};

// The seven items the random walk shuffles around. Tools are in the mix so the
// walk also has stacks that refuse to merge.
const std::array<ItemId, 7> walkItems = {ItemId::Stone,     ItemId::Planks, ItemId::Sand,
                                         ItemId::CoalLump,  ItemId::Apple,  ItemId::IronPickaxe,
                                         ItemId::RawBeef};

struct Ledger {
    std::array<int, 7> counts{};

    void snapshot(const ContainerMenu& menu, const CursorStack& cursor) {
        for (std::size_t i = 0; i < walkItems.size(); ++i) {
            counts[i] = totalAcrossMenuAndCursor(menu, cursor, walkItems[i]);
        }
    }
};

void seedMenu(ContainerMenu& menu, uint32_t& rng) {
    for (int i = 0; i < menu.slotCount(); ++i) {
        if (menu.slot(i).takeOnly) continue;
        if (stackRandom(rng) % 3u == 0u) continue;
        ItemId item = walkItems[stackRandom(rng) % walkItems.size()];
        MetaStack stack = makeStack(item, 1 + static_cast<int>(stackRandom(rng) % 40u));
        if (!menu.canPlace(i, stack)) continue;
        menu.slot(i).cell.insert(stack);
    }
}

SlotAction randomAction(const ContainerMenu& menu, uint32_t& rng) {
    ClickKind kind = static_cast<ClickKind>(stackRandom(rng) %
                                            static_cast<uint32_t>(ClickKind::Count));
    int slotIndex = static_cast<int>(stackRandom(rng) % static_cast<uint32_t>(menu.slotCount()));
    // Every eighth action aims outside the window, which is the only way to
    // reach the throw-away path with a full cursor.
    if (stackRandom(rng) % 8u == 0u) slotIndex = -1;
    if (kind == ClickKind::HotbarSwap) {
        return hotbarSwapAt(slotIndex, static_cast<int>(stackRandom(rng) % 9u));
    }
    return clickAt(kind, slotIndex);
}

struct WalkCoverage {
    int applied = 0;
    int dropped = 0;
    int distinctKindsApplied = 0;
};

WalkCoverage runConservationWalk(MenuKind kind, uint32_t seed, int steps) {
    Session session(kind);
    uint32_t rng = seed;
    seedMenu(session.menu, rng);

    Ledger before;
    before.snapshot(session.menu, session.cursor);

    std::array<int, 7> thrown{};
    std::array<int, static_cast<std::size_t>(ClickKind::Count)> perKind{};
    WalkCoverage coverage;
    for (int step = 0; step < steps; ++step) {
        SlotAction action = randomAction(session.menu, rng);
        SlotActionResult outcome =
            applySlotAction(session.menu, session.cursor, session.drag, action);
        if (outcome.applied) {
            ++coverage.applied;
            ++perKind[static_cast<std::size_t>(action.kind)];
        }
        coverage.dropped += outcome.dropped;
        CHECK_EQ(outcome.created, 0);
        CHECK_EQ(outcome.dropped, static_cast<int>([&] {
                     int sum = 0;
                     for (const MetaStack& lost : outcome.droppedStacks) sum += lost.count;
                     return sum;
                 }()));
        for (const MetaStack& lost : outcome.droppedStacks) {
            for (std::size_t i = 0; i < walkItems.size(); ++i) {
                if (lost.item == walkItems[i]) thrown[i] += lost.count;
            }
        }
    }

    Ledger after;
    after.snapshot(session.menu, session.cursor);
    for (std::size_t i = 0; i < walkItems.size(); ++i) {
        CHECK_EQ(after.counts[i] + thrown[i], before.counts[i]);
    }
    for (int hits : perKind) {
        if (hits > 0) ++coverage.distinctKindsApplied;
    }
    return coverage;
}

} // namespace

VF_TEST(everyClickKindHasAName) {
    for (int i = 0; i < static_cast<int>(ClickKind::Count); ++i) {
        CHECK(std::string(clickKindName(static_cast<ClickKind>(i))) != std::string("unknown"));
    }
}

VF_TEST(aLeftClickPicksUpTheWholeStackAndPutsItDownAgain) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 37);

    SlotActionResult pickup = session.click(ClickKind::Left, 0);
    CHECK(pickup.applied);
    CHECK_EQ(pickup.moved, 37);
    CHECK(pickup.cursorChanged);
    CHECK_EQ(session.cursor.held.count, 37);
    CHECK(session.menu.stackAt(0).empty());

    SlotActionResult drop = session.click(ClickKind::Left, 4);
    CHECK(drop.applied);
    CHECK_EQ(drop.moved, 37);
    CHECK(session.cursor.empty());
    CHECK_EQ(session.menu.stackAt(4).count, 37);

    CHECK(!session.click(ClickKind::Left, 9).applied);
    CHECK(!session.click(ClickKind::Left, 999).applied);
}

VF_TEST(aLeftClickMergesIntoAMatchingStackAndSwapsWithAForeignOne) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 50);
    session.menu.slot(1).cell.stack = makeStack(ItemId::Sand, 60);
    session.menu.slot(2).cell.stack = makeStack(ItemId::Planks, 8);

    session.click(ClickKind::Left, 0);
    SlotActionResult merge = session.click(ClickKind::Left, 1);
    CHECK(merge.applied);
    CHECK_EQ(merge.moved, 4);
    CHECK_EQ(session.menu.stackAt(1).count, 64);
    CHECK_EQ(session.cursor.held.count, 46);

    SlotActionResult swap = session.click(ClickKind::Left, 2);
    CHECK(swap.applied);
    CHECK_EQ(swap.moved, 54);
    CHECK(session.cursor.held.item == ItemId::Planks);
    CHECK_EQ(session.cursor.held.count, 8);
    CHECK(session.menu.stackAt(2).item == ItemId::Sand);
    CHECK_EQ(session.menu.stackAt(2).count, 46);

    // A full matching stack accepts nothing and is not swapped either.
    session.menu.slot(3).cell.stack = makeStack(ItemId::Planks, 64);
    CHECK(!session.click(ClickKind::Left, 3).applied);
    CHECK_EQ(session.cursor.held.count, 8);
}

VF_TEST(aRightClickTakesTheLargerHalfAndThenPlacesOneAtATime) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::CoalLump, 9);

    SlotActionResult half = session.click(ClickKind::Right, 0);
    CHECK(half.applied);
    CHECK_EQ(half.moved, 5);
    CHECK_EQ(session.cursor.held.count, 5);
    CHECK_EQ(session.menu.stackAt(0).count, 4);

    SlotActionResult one = session.click(ClickKind::Right, 6);
    CHECK(one.applied);
    CHECK_EQ(one.moved, 1);
    CHECK_EQ(session.menu.stackAt(6).count, 1);
    CHECK_EQ(session.cursor.held.count, 4);

    session.click(ClickKind::Right, 6);
    CHECK_EQ(session.menu.stackAt(6).count, 2);
    CHECK_EQ(session.cursor.held.count, 3);

    // A right click over an incompatible stack does nothing at all.
    session.menu.slot(7).cell.stack = makeStack(ItemId::Stone, 1);
    CHECK(!session.click(ClickKind::Right, 7).applied);
    CHECK_EQ(session.cursor.held.count, 3);
    CHECK_EQ(session.menu.stackAt(7).count, 1);
}

VF_TEST(shiftClickTopsUpExistingStacksBeforeItTouchesAnEmptySlot) {
    Session session(MenuKind::Chest);
    std::vector<int> player = session.menu.playerSectionSlots();
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 30);
    session.menu.slot(player[0]).cell.stack = makeStack(ItemId::Stone, 64);
    session.menu.slot(player[1]).cell.stack = makeStack(ItemId::Sand, 50);

    SlotActionResult moved = session.click(ClickKind::ShiftLeft, 0);
    CHECK(moved.applied);
    CHECK_EQ(moved.moved, 30);
    CHECK(session.menu.stackAt(0).empty());
    CHECK_EQ(session.menu.stackAt(player[1]).count, 64);
    CHECK_EQ(session.menu.stackAt(player[2]).count, 16);
    CHECK(session.menu.stackAt(player[2]).item == ItemId::Sand);
}

VF_TEST(shiftClickPutsWhatDoesNotFitStraightBackWhereItCameFrom) {
    Session session(MenuKind::Hopper);
    for (int index : session.menu.playerSectionSlots()) {
        session.menu.slot(index).cell.stack = makeStack(ItemId::Obsidian, 64);
    }
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 12);

    SlotActionResult blocked = session.click(ClickKind::ShiftLeft, 0);
    CHECK(!blocked.applied);
    CHECK_EQ(session.menu.stackAt(0).count, 12);
    CHECK_EQ(session.menu.totalOf(ItemId::Sand), 12);
}

VF_TEST(shiftClickInAPlayerInventoryMovesBetweenTheRowsAndTheHotbar) {
    Session session(MenuKind::PlayerInventory);
    const MenuLayout& layout = session.menu.layout();
    int firstRow = layout.inventoryFirst;
    int firstHotbar = layout.hotbarFirst;

    session.menu.slot(firstRow).cell.stack = makeStack(ItemId::Bread, 10);
    SlotActionResult down = session.click(ClickKind::ShiftLeft, firstRow);
    CHECK(down.applied);
    CHECK_EQ(down.moved, 10);
    CHECK(session.menu.stackAt(firstRow).empty());
    CHECK_EQ(session.menu.stackAt(firstHotbar).count, 10);

    SlotActionResult up = session.click(ClickKind::ShiftRight, firstHotbar);
    CHECK(up.applied);
    CHECK_EQ(session.menu.stackAt(firstRow).count, 10);
    CHECK(session.menu.stackAt(firstHotbar).empty());

    // The crafting grid is not a shift-click destination.
    CHECK_EQ(session.menu.totalOf(ItemId::Bread), 10);
    for (int cell = 0; cell < layout.craftCells(); ++cell) {
        CHECK(session.menu.stackAt(layout.craftFirst + cell).empty());
    }
}

VF_TEST(shiftClickHonoursTheDestinationFilters) {
    Session session(MenuKind::Furnace);
    int hotbar = session.menu.layout().hotbarFirst;
    session.menu.slot(hotbar).cell.stack = makeStack(ItemId::Stone, 5);

    SlotActionResult refused = session.click(ClickKind::ShiftLeft, hotbar);
    CHECK(!refused.applied);
    CHECK_EQ(session.menu.stackAt(hotbar).count, 5);

    session.menu.slot(hotbar + 1).cell.stack = makeStack(ItemId::CoalLump, 5);
    SlotActionResult accepted = session.click(ClickKind::ShiftLeft, hotbar + 1);
    CHECK(accepted.applied);
    CHECK_EQ(session.menu.stackAt(session.menu.slotsWithRole(MenuSlot::Role::FuelInput)[0]).count,
             5);
}

VF_TEST(shiftClickingTheResultSlotCraftsEverythingTheGridCanMake) {
    Session session(MenuKind::CraftingTable);
    CraftingView grid = session.menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Planks, 4);
    grid.input(0, 1) = makeStack(ItemId::Planks, 4);
    grid.refresh();

    SlotActionResult crafted = session.click(ClickKind::ShiftLeft, session.menu.layout().resultSlot);
    CHECK(crafted.applied);
    CHECK(crafted.craftedResult);
    CHECK_EQ(crafted.craftedCount, 4);
    CHECK_EQ(session.menu.totalOf(ItemId::Stick), 16);
    CHECK_EQ(session.menu.totalOf(ItemId::Planks), 0);
}

VF_TEST(aNumberKeyExchangesTheHoveredSlotWithThatHotbarSlot) {
    Session session(MenuKind::Chest);
    int hotbar = session.menu.layout().hotbarFirst;
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 20);
    session.menu.slot(hotbar + 3).cell.stack = makeStack(ItemId::Stone, 7);

    SlotActionResult swap = session.hotbar(0, 3);
    CHECK(swap.applied);
    CHECK_EQ(swap.moved, 27);
    CHECK(session.menu.stackAt(0).item == ItemId::Stone);
    CHECK_EQ(session.menu.stackAt(0).count, 7);
    CHECK(session.menu.stackAt(hotbar + 3).item == ItemId::Sand);
    CHECK_EQ(session.menu.stackAt(hotbar + 3).count, 20);

    CHECK(!session.hotbar(0, -1).applied);
    CHECK(!session.hotbar(0, 9).applied);
    CHECK(!session.hotbar(hotbar + 3, 3).applied);
}

VF_TEST(aNumberKeyRespectsTheTargetSlotsFilter) {
    Session session(MenuKind::Furnace);
    int hotbar = session.menu.layout().hotbarFirst;
    int fuel = session.menu.slotsWithRole(MenuSlot::Role::FuelInput)[0];

    session.menu.slot(hotbar).cell.stack = makeStack(ItemId::Stone, 4);
    CHECK(!session.hotbar(fuel, 0).applied);
    CHECK_EQ(session.menu.stackAt(hotbar).count, 4);

    session.menu.slot(hotbar + 1).cell.stack = makeStack(ItemId::Planks, 4);
    CHECK(session.hotbar(fuel, 1).applied);
    CHECK_EQ(session.menu.stackAt(fuel).count, 4);
}

VF_TEST(aNumberKeyOnTheResultSlotCraftsIntoAnEmptyHotbarSlot) {
    Session session(MenuKind::CraftingTable);
    CraftingView grid = session.menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Wood, 3);
    grid.refresh();

    int hotbar = session.menu.layout().hotbarFirst;
    SlotActionResult crafted = session.hotbar(session.menu.layout().resultSlot, 0);
    CHECK(crafted.applied);
    CHECK(crafted.craftedResult);
    CHECK_EQ(session.menu.stackAt(hotbar).count, 4);
    CHECK(session.menu.stackAt(hotbar).item == ItemId::Planks);
    CHECK_EQ(grid.input(0, 0).count, 2);

    // The second attempt finds the hotbar slot occupied and refuses.
    CHECK(!session.hotbar(session.menu.layout().resultSlot, 0).applied);
    CHECK_EQ(grid.input(0, 0).count, 2);
}

VF_TEST(theMiddleClickOnlyClonesInCreativeMode) {
    Session survival(MenuKind::Chest);
    survival.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 3);
    SlotActionResult nothing = survival.click(ClickKind::Middle, 0);
    CHECK(!nothing.applied);
    CHECK_EQ(nothing.created, 0);
    CHECK(survival.cursor.empty());

    Session creative(MenuKind::Chest);
    creative.menu.setCreativeMode(true);
    creative.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 3);
    SlotActionResult cloned = creative.click(ClickKind::Middle, 0);
    CHECK(cloned.applied);
    CHECK_EQ(cloned.created, 64);
    CHECK_EQ(creative.cursor.held.count, 64);
    CHECK_EQ(creative.menu.stackAt(0).count, 3);

    // A full cursor blocks the clone.
    CHECK(!creative.click(ClickKind::Middle, 0).applied);
}

VF_TEST(aDoubleClickGathersEveryMatchingStackOntoTheCursor) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 10);
    session.menu.slot(1).cell.stack = makeStack(ItemId::Sand, 64);
    session.menu.slot(2).cell.stack = makeStack(ItemId::Sand, 5);
    session.menu.slot(3).cell.stack = makeStack(ItemId::Stone, 20);

    session.click(ClickKind::Left, 0);
    CHECK_EQ(session.cursor.held.count, 10);

    SlotActionResult gathered = session.click(ClickKind::Double, 0);
    CHECK(gathered.applied);
    CHECK_EQ(session.cursor.held.count, 64);
    // The partial stack goes first, the full one only tops up the rest.
    CHECK(session.menu.stackAt(2).empty());
    CHECK_EQ(session.menu.stackAt(1).count, 15);
    CHECK_EQ(session.menu.stackAt(3).count, 20);

    CHECK(!session.click(ClickKind::Double, 0).applied);
}

VF_TEST(aDoubleClickWithAnEmptyCursorDoesNothing) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Sand, 10);
    CHECK(!session.click(ClickKind::Double, 0).applied);
    CHECK_EQ(session.menu.stackAt(0).count, 10);
}

VF_TEST(aDoubleClickNeverPullsOutOfAResultSlot) {
    Session session(MenuKind::CraftingTable);
    CraftingView grid = session.menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Wood, 1);
    grid.refresh();

    int hotbar = session.menu.layout().hotbarFirst;
    session.menu.slot(hotbar).cell.stack = makeStack(ItemId::Planks, 2);
    session.click(ClickKind::Left, hotbar);
    CHECK_EQ(session.cursor.held.count, 2);

    session.click(ClickKind::Double, hotbar);
    CHECK_EQ(session.cursor.held.count, 2);
    CHECK_EQ(session.menu.stackAt(session.menu.layout().resultSlot).count, 4);
}

VF_TEST(aLeftDragSplitsEvenlyAndKeepsTheRemainderOnTheCursor) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 10);
    session.click(ClickKind::Left, 0);

    CHECK(session.click(ClickKind::DragLeftStart, 1).applied);
    CHECK(session.click(ClickKind::DragLeftAdd, 2).applied);
    CHECK(session.click(ClickKind::DragLeftAdd, 3).applied);
    CHECK(!session.click(ClickKind::DragLeftAdd, 3).applied);

    SlotActionResult released = session.click(ClickKind::DragLeftEnd, -1);
    CHECK(released.applied);
    CHECK_EQ(released.moved, 9);
    CHECK_EQ(session.menu.stackAt(1).count, 3);
    CHECK_EQ(session.menu.stackAt(2).count, 3);
    CHECK_EQ(session.menu.stackAt(3).count, 3);
    CHECK_EQ(session.cursor.held.count, 1);
    CHECK(!session.drag.active);
}

VF_TEST(aRightDragLeavesExactlyOneItemPerPaintedSlot) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Apple, 8);
    session.click(ClickKind::Left, 0);

    session.click(ClickKind::DragRightStart, 1);
    session.click(ClickKind::DragRightAdd, 2);
    session.click(ClickKind::DragRightAdd, 3);
    SlotActionResult released = session.click(ClickKind::DragRightEnd, -1);

    CHECK(released.applied);
    CHECK_EQ(released.moved, 3);
    CHECK_EQ(session.menu.stackAt(1).count, 1);
    CHECK_EQ(session.menu.stackAt(2).count, 1);
    CHECK_EQ(session.menu.stackAt(3).count, 1);
    CHECK_EQ(session.cursor.held.count, 5);
}

VF_TEST(aDragSkipsSlotsThatWouldRefuseTheStackAnyway) {
    Session session(MenuKind::Furnace);
    int hotbar = session.menu.layout().hotbarFirst;
    int fuel = session.menu.slotsWithRole(MenuSlot::Role::FuelInput)[0];
    int input = session.menu.slotsWithRole(MenuSlot::Role::SmeltInput)[0];

    session.menu.slot(hotbar).cell.stack = makeStack(ItemId::Stone, 6);
    session.click(ClickKind::Left, hotbar);

    session.click(ClickKind::DragLeftStart, hotbar + 1);
    CHECK(!session.click(ClickKind::DragLeftAdd, fuel).applied);
    CHECK(!session.click(ClickKind::DragLeftAdd, input).applied);
    CHECK(session.click(ClickKind::DragLeftAdd, hotbar + 2).applied);
    session.click(ClickKind::DragLeftEnd, -1);

    CHECK_EQ(session.menu.stackAt(hotbar + 1).count, 3);
    CHECK_EQ(session.menu.stackAt(hotbar + 2).count, 3);
    CHECK(session.menu.stackAt(fuel).empty());
    CHECK(session.cursor.empty());
}

VF_TEST(anAbandonedDragGivesEverythingBackToTheCursor) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 12);
    session.click(ClickKind::Left, 0);

    session.click(ClickKind::DragLeftStart, -1);
    CHECK(session.drag.active);
    SlotActionResult released = session.click(ClickKind::DragLeftEnd, -1);
    CHECK(released.applied);
    CHECK_EQ(released.moved, 0);
    CHECK_EQ(session.cursor.held.count, 12);
    CHECK(!session.drag.active);

    // A drag end for the other button is not this drag's business.
    session.click(ClickKind::DragLeftStart, 1);
    CHECK(!session.click(ClickKind::DragRightEnd, -1).applied);
    CHECK(session.drag.active);
}

VF_TEST(aDragNeverStartsWithAnEmptyCursor) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 12);
    CHECK(!session.click(ClickKind::DragLeftStart, 0).applied);
    CHECK(!session.drag.active);
    CHECK(!session.click(ClickKind::DragLeftAdd, 1).applied);
    CHECK_EQ(session.menu.stackAt(0).count, 12);
}

VF_TEST(throwingFromTheCursorReportsExactlyWhatLeftTheMenu) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Gravel, 6);
    session.click(ClickKind::Left, 0);

    SlotActionResult one = session.click(ClickKind::DropOne, -1);
    CHECK(one.applied);
    CHECK_EQ(one.dropped, 1);
    CHECK_EQ(one.droppedStacks.size(), 1u);
    CHECK_EQ(one.droppedStacks[0].count, 1);
    CHECK_EQ(session.cursor.held.count, 5);

    SlotActionResult all = session.click(ClickKind::DropStack, -1);
    CHECK_EQ(all.dropped, 5);
    CHECK(session.cursor.empty());
    CHECK(!session.click(ClickKind::DropStack, -1).applied);
}

VF_TEST(throwingFromASlotOnlyWorksWithAnEmptyHand) {
    Session session(MenuKind::Chest);
    session.menu.slot(0).cell.stack = makeStack(ItemId::Gravel, 6);

    SlotActionResult one = session.click(ClickKind::DropOne, 0);
    CHECK(one.applied);
    CHECK_EQ(one.dropped, 1);
    CHECK_EQ(session.menu.stackAt(0).count, 5);

    SlotActionResult rest = session.click(ClickKind::DropStack, 0);
    CHECK_EQ(rest.dropped, 5);
    CHECK(session.menu.stackAt(0).empty());
    CHECK(!session.click(ClickKind::DropOne, 0).applied);

    session.menu.slot(1).cell.stack = makeStack(ItemId::Stone, 4);
    session.click(ClickKind::Left, 1);
    session.menu.slot(2).cell.stack = makeStack(ItemId::Sand, 4);
    CHECK(!session.click(ClickKind::DropOne, 2).applied);
    CHECK_EQ(session.menu.stackAt(2).count, 4);
}

VF_TEST(theResultSlotCannotBeUsedAsStorageByAnyClick) {
    Session session(MenuKind::CraftingTable);
    int result = session.menu.layout().resultSlot;
    session.menu.slot(0).cell.stack = makeStack(ItemId::Stone, 10);
    session.click(ClickKind::Left, 0);

    CHECK(!session.click(ClickKind::Left, result).applied);
    CHECK(!session.click(ClickKind::Right, result).applied);
    CHECK(!session.click(ClickKind::DragLeftStart, result).applied ||
          session.drag.paintedSlots.empty());
    session.drag.reset();
    CHECK(session.menu.stackAt(result).empty());
    CHECK_EQ(session.cursor.held.count, 10);
}

VF_TEST(takingAResultOntoAMatchingCursorKeepsCrafting) {
    Session session(MenuKind::CraftingTable);
    CraftingView grid = session.menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Wood, 4);
    grid.refresh();

    session.click(ClickKind::Left, session.menu.layout().resultSlot);
    CHECK_EQ(session.cursor.held.count, 4);
    CHECK(session.cursor.held.item == ItemId::Planks);

    SlotActionResult second = session.click(ClickKind::Left, session.menu.layout().resultSlot);
    CHECK(second.applied);
    CHECK(second.craftedResult);
    CHECK_EQ(session.cursor.held.count, 8);
    CHECK_EQ(grid.input(0, 0).count, 2);

    // A cursor holding something else cannot take the result.
    session.cursor.held = makeStack(ItemId::Stone, 1);
    CHECK(!session.click(ClickKind::Left, session.menu.layout().resultSlot).applied);
    CHECK_EQ(grid.input(0, 0).count, 2);
}

VF_TEST(noSequenceOfChestClicksEverCreatesOrDestroysAnItem) {
    WalkCoverage walk = runConservationWalk(MenuKind::Chest, 20260904u, 500);
    // A walk that never actually did anything would pass the invariant for the
    // wrong reason, so the coverage itself is asserted: most click kinds have
    // to have landed, and items have to have really left through the drop path.
    CHECK(walk.applied > 100);
    CHECK(walk.dropped > 0);
    CHECK(walk.distinctKindsApplied >= 12);
}

VF_TEST(theSameInvariantHoldsOnAFilteredFurnaceScreen) {
    WalkCoverage walk = runConservationWalk(MenuKind::Furnace, 77u, 500);
    CHECK(walk.applied > 100);
    CHECK(walk.dropped > 0);
    CHECK(walk.distinctKindsApplied >= 12);
}

VF_TEST(theSameInvariantHoldsOnAHopperAndOnALargeChest) {
    WalkCoverage hopper = runConservationWalk(MenuKind::Hopper, 4242u, 500);
    WalkCoverage large = runConservationWalk(MenuKind::LargeChest, 991u, 500);
    CHECK(hopper.applied > 100);
    CHECK(large.applied > 100);
    CHECK(hopper.dropped > 0);
    CHECK(large.dropped > 0);
}

VF_TEST(theConservationWalkIsReproducibleFromItsSeed) {
    CHECK_EQ(runConservationWalk(MenuKind::Chest, 555u, 500).applied,
             runConservationWalk(MenuKind::Chest, 555u, 500).applied);
    CHECK(runConservationWalk(MenuKind::Chest, 555u, 500).applied !=
          runConservationWalk(MenuKind::Chest, 556u, 500).applied);
}
