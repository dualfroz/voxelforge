#pragma once

#include <cstdint>
#include <vector>

#include "ContainerMenu.h"

namespace vf {

// Click semantics for an open ContainerMenu. This is where inventories usually
// break, so the whole vocabulary lives in one place and every action reports
// exactly what it moved.

enum class ClickKind : uint8_t {
    Left,            // take all / put all / swap
    Right,           // take half / put one
    ShiftLeft,       // move to the other section, merging before filling
    ShiftRight,      // same rule; kept distinct so callers can bind both
    HotbarSwap,      // number key: exchange the slot with a hotbar slot
    Middle,          // creative clone, otherwise nothing happens
    Double,          // gather every mergeable stack onto the cursor
    DragLeftStart,
    DragLeftAdd,
    DragLeftEnd,     // split the held stack evenly, remainder stays held
    DragRightStart,
    DragRightAdd,
    DragRightEnd,    // one item per dragged slot
    DropOne,         // Q
    DropStack,       // Ctrl+Q
    Count
};

const char* clickKindName(ClickKind kind);

// The stack the mouse carries between slots. It is deliberately its own type:
// the cursor is not a menu slot and must never be reachable by index.
struct CursorStack {
    MetaStack held;

    bool empty() const { return held.empty(); }
    void clear() { held.clear(); }
};

// A drag paints a set of slots while the button is down. The items stay on the
// cursor for the whole gesture and are only distributed on release, so an
// abandoned drag cannot lose anything.
struct DragState {
    bool active = false;
    bool rightButton = false;
    std::vector<int> paintedSlots;
    // Cursor contents when the drag began, needed to compute an even split.
    MetaStack payload;

    void reset();
    bool alreadyPainted(int index) const;
};

struct SlotAction {
    ClickKind kind = ClickKind::Left;
    int slotIndex = -1;
    // Which hotbar slot a HotbarSwap targets, 0..8.
    int hotbarButton = -1;
};

SlotAction clickAt(ClickKind kind, int slotIndex);
SlotAction hotbarSwapAt(int slotIndex, int hotbarButton);

struct SlotActionResult {
    bool applied = false;
    // Items that changed slot, or moved between a slot and the cursor.
    int moved = 0;
    // Items that left the menu entirely. They are also listed in droppedStacks
    // so a caller can spawn the right entities.
    int dropped = 0;
    std::vector<MetaStack> droppedStacks;
    // Only a creative middle click can be non-zero here. Every other action is
    // conservative, which is what the invariant test pins down.
    int created = 0;
    bool cursorChanged = false;
    bool craftedResult = false;
    int craftedCount = 0;

    void noteDrop(const MetaStack& stack);
};

// Applies one click. `cursor` and `drag` are the player's own state and are
// carried between calls; the menu is the screen being clicked on.
SlotActionResult applySlotAction(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                                 const SlotAction& action);

// Total items of one kind across everything the action model can touch: the
// menu, the cursor, and whatever a drag is holding. The invariant is that this
// number only ever changes by what an action reports as dropped or created.
int totalAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor, ItemId item);
int totalItemsAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor);

} // namespace vf
