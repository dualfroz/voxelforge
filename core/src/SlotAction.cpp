#include "vf/SlotAction.h"

#include <algorithm>

namespace vf {

namespace {

// The half of a menu a shift-click aims at. Craft inputs and result slots are
// deliberately not part of it: shift-clicking in a player inventory moves items
// between the rows and the hotbar, it does not stuff the 2x2 grid.
bool isTransferSection(MenuSlot::Role role) {
    switch (role) {
        case MenuSlot::Role::Storage:
        case MenuSlot::Role::FuelInput:
        case MenuSlot::Role::SmeltInput:
        case MenuSlot::Role::SmeltOutput:
        case MenuSlot::Role::BrewBottle:
        case MenuSlot::Role::BrewIngredient:
        case MenuSlot::Role::EnchantItem:
        case MenuSlot::Role::EnchantLapis:
        case MenuSlot::Role::AnvilInput:
        case MenuSlot::Role::AnvilResult:
        case MenuSlot::Role::TradeInput:
        case MenuSlot::Role::TradeResult:
            return true;
        case MenuSlot::Role::Inventory:
        case MenuSlot::Role::Hotbar:
        case MenuSlot::Role::OffHand:
        case MenuSlot::Role::CraftInput:
        case MenuSlot::Role::CraftResult:
        case MenuSlot::Role::Count:
            return false;
    }
    return false;
}

std::vector<int> collectRoles(const ContainerMenu& menu, bool wantTransferSection) {
    std::vector<int> found;
    for (int i = 0; i < menu.slotCount(); ++i) {
        if (isTransferSection(menu.slot(i).role) == wantTransferSection) found.push_back(i);
    }
    return found;
}

// Where a shift-click from `index` should land. Container half to player half
// and back; in a menu that has no container half, the player's rows and the
// hotbar swap roles instead.
std::vector<int> transferTargetsFor(const ContainerMenu& menu, int index) {
    std::vector<int> container = collectRoles(menu, true);
    MenuSlot::Role role = menu.slot(index).role;

    if (isTransferSection(role)) return menu.playerSectionSlots();
    if (!container.empty()) return container;

    std::vector<int> targets;
    MenuSlot::Role wanted =
        role == MenuSlot::Role::Hotbar ? MenuSlot::Role::Inventory : MenuSlot::Role::Hotbar;
    for (int i = 0; i < menu.slotCount(); ++i) {
        if (menu.slot(i).role == wanted) targets.push_back(i);
    }
    return targets;
}

bool isResultSlot(const ContainerMenu& menu, int index) {
    return menu.layout().resultSlot >= 0 && index == menu.layout().resultSlot;
}

void touchCraftInput(ContainerMenu& menu, int index) {
    if (menu.slot(index).role == MenuSlot::Role::CraftInput) menu.refreshResult();
}

// Every removal from a slot goes through here so the crafting grid can never
// hand out a result without paying for it.
MetaStack takeFromSlot(ContainerMenu& menu, int index, int amount, SlotActionResult& out) {
    if (isResultSlot(menu, index)) {
        CraftingView view = menu.crafting();
        MetaStack crafted = view.takeResult();
        if (!crafted.empty()) {
            out.craftedResult = true;
            out.craftedCount += crafted.count;
        }
        return crafted;
    }
    MetaStack taken = menu.takeFrom(index, amount);
    touchCraftInput(menu, index);
    return taken;
}

int putIntoSlot(ContainerMenu& menu, int index, MetaStack& incoming) {
    int moved = menu.insertAt(index, incoming);
    if (moved > 0) touchCraftInput(menu, index);
    return moved;
}

bool swapWithSlot(ContainerMenu& menu, int index, CursorStack& cursor) {
    MenuSlot& target = menu.slot(index);
    if (target.takeOnly) return false;
    if (!menu.canPlace(index, cursor.held)) return false;
    if (target.cell.limitFor(cursor.held) < cursor.held.count) return false;
    cursor.held = target.cell.swapWith(cursor.held);
    touchCraftInput(menu, index);
    return true;
}

SlotActionResult leftClick(ContainerMenu& menu, CursorStack& cursor, const SlotAction& action) {
    SlotActionResult out;
    int index = action.slotIndex;
    if (!menu.validIndex(index)) return out;

    if (cursor.empty()) {
        MetaStack taken = takeFromSlot(menu, index, menu.stackAt(index).count, out);
        if (taken.empty()) return out;
        out.moved = taken.count;
        cursor.held = taken;
        out.cursorChanged = true;
        out.applied = true;
        return out;
    }

    if (isResultSlot(menu, index)) {
        const MetaStack& pending = menu.stackAt(index);
        if (pending.empty() || !stacksMergeable(cursor.held, pending)) return out;
        if (cursor.held.room() < pending.count) return out;
        MetaStack crafted = takeFromSlot(menu, index, pending.count, out);
        out.moved = crafted.count;
        transferAll(crafted, cursor.held);
        out.cursorChanged = true;
        out.applied = true;
        return out;
    }

    int moved = putIntoSlot(menu, index, cursor.held);
    if (moved > 0) {
        out.moved = moved;
        out.cursorChanged = true;
        out.applied = true;
        return out;
    }

    if (menu.stackAt(index).empty() || stacksMergeable(menu.stackAt(index), cursor.held)) return out;
    if (!swapWithSlot(menu, index, cursor)) return out;
    out.moved = cursor.held.count + menu.stackAt(index).count;
    out.cursorChanged = true;
    out.applied = true;
    return out;
}

SlotActionResult rightClick(ContainerMenu& menu, CursorStack& cursor, const SlotAction& action) {
    SlotActionResult out;
    int index = action.slotIndex;
    if (!menu.validIndex(index)) return out;

    if (cursor.empty()) {
        if (menu.stackAt(index).empty()) return out;
        int half = (menu.stackAt(index).count + 1) / 2;
        MetaStack taken = takeFromSlot(menu, index, half, out);
        if (taken.empty()) return out;
        out.moved = taken.count;
        cursor.held = taken;
        out.cursorChanged = true;
        out.applied = true;
        return out;
    }

    if (isResultSlot(menu, index)) return out;

    MetaStack single = splitOff(cursor.held, 1);
    int moved = putIntoSlot(menu, index, single);
    if (moved == 0) {
        transferAll(single, cursor.held);
        return out;
    }
    out.moved = moved;
    out.cursorChanged = true;
    out.applied = true;
    return out;
}

SlotActionResult shiftClick(ContainerMenu& menu, const SlotAction& action) {
    SlotActionResult out;
    int index = action.slotIndex;
    if (!menu.validIndex(index) || menu.stackAt(index).empty()) return out;

    if (isResultSlot(menu, index)) {
        CraftingView view = menu.crafting();
        int rounds = view.craftAll(view.maxCrafts());
        if (rounds <= 0) return out;
        out.craftedResult = true;
        out.craftedCount = rounds;
        out.applied = true;
        return out;
    }

    std::vector<int> targets = transferTargetsFor(menu, index);
    MetaStack moving = menu.slot(index).cell.takeAll();
    int moved = menu.distributeInto(targets, moving);
    if (!moving.empty()) {
        // Whatever did not fit goes straight back where it came from.
        menu.slot(index).cell.insert(moving);
    }
    touchCraftInput(menu, index);
    if (moved == 0) return out;
    out.moved = moved;
    out.applied = true;
    return out;
}

SlotActionResult hotbarSwap(ContainerMenu& menu, const SlotAction& action) {
    SlotActionResult out;
    int index = action.slotIndex;
    const MenuLayout& layout = menu.layout();
    if (!menu.validIndex(index) || layout.hotbarFirst < 0) return out;
    if (action.hotbarButton < 0 || action.hotbarButton >= layout.hotbarCount) return out;

    int hotbarIndex = layout.hotbarFirst + action.hotbarButton;
    if (hotbarIndex == index) return out;

    if (isResultSlot(menu, index)) {
        if (!menu.stackAt(hotbarIndex).empty()) return out;
        MetaStack crafted = takeFromSlot(menu, index, menu.stackAt(index).count, out);
        if (crafted.empty()) return out;
        out.moved = crafted.count;
        menu.slot(hotbarIndex).cell.stack = crafted;
        out.applied = true;
        return out;
    }

    MenuSlot& source = menu.slot(index);
    MenuSlot& hotbar = menu.slot(hotbarIndex);
    if (source.takeOnly) return out;
    if (source.cell.stack.empty() && hotbar.cell.stack.empty()) return out;
    if (!hotbar.cell.stack.empty()) {
        if (!menu.canPlace(index, hotbar.cell.stack)) return out;
        if (source.cell.limitFor(hotbar.cell.stack) < hotbar.cell.stack.count) return out;
    }

    out.moved = source.cell.stack.count + hotbar.cell.stack.count;
    MetaStack carried = source.cell.stack;
    source.cell.stack = hotbar.cell.stack;
    hotbar.cell.stack = carried;
    touchCraftInput(menu, index);
    out.applied = true;
    return out;
}

SlotActionResult middleClick(ContainerMenu& menu, CursorStack& cursor, const SlotAction& action) {
    SlotActionResult out;
    int index = action.slotIndex;
    if (!menu.validIndex(index) || !menu.creativeMode()) return out;
    if (!cursor.empty() || menu.stackAt(index).empty()) return out;

    MetaStack clone = menu.stackAt(index);
    clone.count = clone.maxCount();
    out.created = clone.count;
    cursor.held = clone;
    out.cursorChanged = true;
    out.applied = true;
    return out;
}

SlotActionResult doubleClick(ContainerMenu& menu, CursorStack& cursor) {
    SlotActionResult out;
    if (cursor.empty() || cursor.held.room() == 0) return out;

    // Partial stacks first: a double click is meant to tidy up leftovers before
    // it starts breaking apart full stacks.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < menu.slotCount(); ++i) {
            if (cursor.held.room() == 0) break;
            MenuSlot& source = menu.slot(i);
            if (source.takeOnly || source.cell.stack.empty()) continue;
            if (!stacksMergeable(source.cell.stack, cursor.held)) continue;
            bool partial = source.cell.stack.count < source.cell.stack.maxCount();
            if ((pass == 0) != partial) continue;
            int before = cursor.held.count;
            transferAll(source.cell.stack, cursor.held);
            int gained = cursor.held.count - before;
            if (gained > 0) {
                out.moved += gained;
                touchCraftInput(menu, i);
            }
        }
    }
    if (out.moved == 0) return out;
    out.cursorChanged = true;
    out.applied = true;
    return out;
}

SlotActionResult dragStart(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                           const SlotAction& action, bool rightButton) {
    SlotActionResult out;
    if (cursor.empty()) return out;
    drag.reset();
    drag.active = true;
    drag.rightButton = rightButton;
    drag.payload = cursor.held;
    if (menu.validIndex(action.slotIndex) && menu.canPlace(action.slotIndex, cursor.held)) {
        drag.paintedSlots.push_back(action.slotIndex);
    }
    out.applied = true;
    return out;
}

SlotActionResult dragAdd(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                         const SlotAction& action, bool rightButton) {
    SlotActionResult out;
    if (!drag.active || drag.rightButton != rightButton) return out;
    int index = action.slotIndex;
    if (!menu.validIndex(index) || drag.alreadyPainted(index)) return out;
    if (!menu.canPlace(index, cursor.held)) return out;
    const MetaStack& existing = menu.stackAt(index);
    if (!existing.empty() && !stacksMergeable(existing, cursor.held)) return out;
    drag.paintedSlots.push_back(index);
    out.applied = true;
    return out;
}

SlotActionResult dragEnd(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                         bool rightButton) {
    SlotActionResult out;
    if (!drag.active || drag.rightButton != rightButton) return out;

    int painted = static_cast<int>(drag.paintedSlots.size());
    if (painted == 0 || cursor.empty()) {
        drag.reset();
        out.applied = true;
        return out;
    }

    int share = rightButton ? 1 : drag.payload.count / painted;
    if (share > 0) {
        for (int index : drag.paintedSlots) {
            if (cursor.empty()) break;
            MetaStack portion = splitOff(cursor.held, share);
            int moved = putIntoSlot(menu, index, portion);
            out.moved += moved;
            if (!portion.empty()) transferAll(portion, cursor.held);
        }
    }

    drag.reset();
    out.cursorChanged = out.moved > 0;
    out.applied = true;
    return out;
}

SlotActionResult dropAction(ContainerMenu& menu, CursorStack& cursor, const SlotAction& action,
                            bool wholeStack) {
    SlotActionResult out;

    if (!cursor.empty()) {
        if (menu.validIndex(action.slotIndex)) return out;
        MetaStack thrown = wholeStack ? splitOff(cursor.held, cursor.held.count)
                                      : splitOff(cursor.held, 1);
        out.noteDrop(thrown);
        out.cursorChanged = true;
        out.applied = true;
        return out;
    }

    if (!menu.validIndex(action.slotIndex)) return out;
    int index = action.slotIndex;
    if (menu.stackAt(index).empty()) return out;
    int amount = wholeStack ? menu.stackAt(index).count : 1;
    // A result slot always hands over a whole craft: there is no such thing as
    // crafting one third of a recipe, so Q on a result drops the whole batch.
    MetaStack thrown = takeFromSlot(menu, index, amount, out);
    if (thrown.empty()) return out;
    out.noteDrop(thrown);
    out.applied = true;
    return out;
}

} // namespace

const char* clickKindName(ClickKind kind) {
    switch (kind) {
        case ClickKind::Left:           return "left";
        case ClickKind::Right:          return "right";
        case ClickKind::ShiftLeft:      return "shiftLeft";
        case ClickKind::ShiftRight:     return "shiftRight";
        case ClickKind::HotbarSwap:     return "hotbarSwap";
        case ClickKind::Middle:         return "middle";
        case ClickKind::Double:         return "double";
        case ClickKind::DragLeftStart:  return "dragLeftStart";
        case ClickKind::DragLeftAdd:    return "dragLeftAdd";
        case ClickKind::DragLeftEnd:    return "dragLeftEnd";
        case ClickKind::DragRightStart: return "dragRightStart";
        case ClickKind::DragRightAdd:   return "dragRightAdd";
        case ClickKind::DragRightEnd:   return "dragRightEnd";
        case ClickKind::DropOne:        return "dropOne";
        case ClickKind::DropStack:      return "dropStack";
        case ClickKind::Count:          break;
    }
    return "unknown";
}

void DragState::reset() {
    active = false;
    rightButton = false;
    paintedSlots.clear();
    payload.clear();
}

bool DragState::alreadyPainted(int index) const {
    return std::find(paintedSlots.begin(), paintedSlots.end(), index) != paintedSlots.end();
}

SlotAction clickAt(ClickKind kind, int slotIndex) {
    SlotAction action;
    action.kind = kind;
    action.slotIndex = slotIndex;
    return action;
}

SlotAction hotbarSwapAt(int slotIndex, int hotbarButton) {
    SlotAction action;
    action.kind = ClickKind::HotbarSwap;
    action.slotIndex = slotIndex;
    action.hotbarButton = hotbarButton;
    return action;
}

void SlotActionResult::noteDrop(const MetaStack& stack) {
    if (stack.empty()) return;
    dropped += stack.count;
    droppedStacks.push_back(stack);
}

SlotActionResult applySlotAction(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                                 const SlotAction& action) {
    switch (action.kind) {
        case ClickKind::Left:           return leftClick(menu, cursor, action);
        case ClickKind::Right:          return rightClick(menu, cursor, action);
        case ClickKind::ShiftLeft:
        case ClickKind::ShiftRight:     return shiftClick(menu, action);
        case ClickKind::HotbarSwap:     return hotbarSwap(menu, action);
        case ClickKind::Middle:         return middleClick(menu, cursor, action);
        case ClickKind::Double:         return doubleClick(menu, cursor);
        case ClickKind::DragLeftStart:  return dragStart(menu, cursor, drag, action, false);
        case ClickKind::DragLeftAdd:    return dragAdd(menu, cursor, drag, action, false);
        case ClickKind::DragLeftEnd:    return dragEnd(menu, cursor, drag, false);
        case ClickKind::DragRightStart: return dragStart(menu, cursor, drag, action, true);
        case ClickKind::DragRightAdd:   return dragAdd(menu, cursor, drag, action, true);
        case ClickKind::DragRightEnd:   return dragEnd(menu, cursor, drag, true);
        case ClickKind::DropOne:        return dropAction(menu, cursor, action, false);
        case ClickKind::DropStack:      return dropAction(menu, cursor, action, true);
        case ClickKind::Count:          break;
    }
    return SlotActionResult{};
}

int totalAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor, ItemId item) {
    int total = menu.totalOf(item);
    if (!cursor.empty() && cursor.held.item == item) total += cursor.held.count;
    return total;
}

int totalItemsAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor) {
    return menu.totalItems() + cursor.held.count;
}

} // namespace vf
