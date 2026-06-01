#pragma once

#include <unordered_map>
#include <vector>
#include "Item.h"
#include "Block.h"
#include "Math.h"

namespace vf {

constexpr int redstoneComparatorMax = 15;

inline Vec3i faceDelta(Face face) {
    switch (face) {
        case Face::PosX: return {1, 0, 0};
        case Face::NegX: return {-1, 0, 0};
        case Face::PosY: return {0, 1, 0};
        case Face::NegY: return {0, -1, 0};
        case Face::PosZ: return {0, 0, 1};
        case Face::NegZ: return {0, 0, -1};
    }
    return {0, 0, 0};
}

// Generic multi-slot inventory holding ItemSlot values. Backs chests, hoppers,
// droppers, dispensers and container minecarts. Merging respects each item's
// own max stack (tools do not stack, blocks stack to 64).
class Container {
public:
    Container() = default;
    explicit Container(int slotCount) : slots(slotCount) {}

    int size() const { return static_cast<int>(slots.size()); }

    const ItemSlot& slot(int index) const { return slots[index]; }
    ItemSlot& slot(int index) { return slots[index]; }

    void setSlot(int index, const ItemSlot& stack) { slots[index] = stack; }
    void clear() { for (ItemSlot& stack : slots) stack = ItemSlot{}; }

    bool isEmpty() const;
    bool isFull() const;
    int usedSlots() const;
    int totalItems() const;

    // Inserts as much of `incoming` as fits, merging into matching stacks first
    // then filling empty slots (each capped at the item's max stack). Reduces
    // `incoming.count` by what was taken. Returns the amount inserted.
    int insert(ItemSlot& incoming);
    int insert(ItemId item, int count, int durability = 0);

    // Pulls up to `amount` of `item` from any slots (first-fit, matching
    // durability). Returns the removed stack (empty when nothing matched).
    ItemSlot extract(ItemId item, int amount);
    ItemSlot extractSlot(int index, int amount);

    // Moves up to `maxItems` items from this container into `dest`, walking
    // slots in order. Returns how many items actually moved.
    int transferTo(Container& dest, int maxItems);

    // Redstone comparator reading 0..15 from average fill fraction, matching the
    // vanilla formula: 0 when empty, 15 when every slot is at its max stack.
    int comparatorSignal() const;

private:
    std::vector<ItemSlot> slots;
};

// Position keyed registry of block-entity inventories. Chests, hoppers,
// droppers and dispensers each own a Container addressed by their world cell.
class ContainerStore {
public:
    Container& ensure(const Vec3i& pos, int slotCount);
    Container* at(const Vec3i& pos);
    const Container* at(const Vec3i& pos) const;
    bool has(const Vec3i& pos) const { return containers.find(pos) != containers.end(); }
    void remove(const Vec3i& pos) { containers.erase(pos); }
    std::size_t count() const { return containers.size(); }

private:
    std::unordered_map<Vec3i, Container, Vec3iHash> containers;
};

} // namespace vf
