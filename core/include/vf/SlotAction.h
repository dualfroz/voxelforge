#pragma once

#include <cstdint>
#include <vector>

#include "ContainerMenu.h"

namespace vf {

enum class ClickKind : uint8_t {
    Left,
    Right,
    ShiftLeft,
    ShiftRight,
    HotbarSwap,
    Middle,
    Double,
    DragLeftStart,
    DragLeftAdd,
    DragLeftEnd,
    DragRightStart,
    DragRightAdd,
    DragRightEnd,
    DropOne,
    DropStack,
    Count
};

const char* clickKindName(ClickKind kind);

struct CursorStack {
    MetaStack held;

    bool empty() const { return held.empty(); }
    void clear() { held.clear(); }
};

struct DragState {
    bool active = false;
    bool rightButton = false;
    std::vector<int> paintedSlots;
    MetaStack payload;

    void reset();
    bool alreadyPainted(int index) const;
};

struct SlotAction {
    ClickKind kind = ClickKind::Left;
    int slotIndex = -1;
    int hotbarButton = -1;
};

SlotAction clickAt(ClickKind kind, int slotIndex);
SlotAction hotbarSwapAt(int slotIndex, int hotbarButton);

struct SlotActionResult {
    bool applied = false;
    int moved = 0;
    int dropped = 0;
    std::vector<MetaStack> droppedStacks;
    int created = 0;
    bool cursorChanged = false;
    bool craftedResult = false;
    int craftedCount = 0;

    void noteDrop(const MetaStack& stack);
};

SlotActionResult applySlotAction(ContainerMenu& menu, CursorStack& cursor, DragState& drag,
                                 const SlotAction& action);

int totalAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor, ItemId item);
int totalItemsAcrossMenuAndCursor(const ContainerMenu& menu, const CursorStack& cursor);

}
