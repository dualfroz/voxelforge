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

    int insert(ItemSlot& incoming);
    int insert(ItemId item, int count, int durability = 0);

    ItemSlot extract(ItemId item, int amount);
    ItemSlot extractSlot(int index, int amount);

    int transferTo(Container& dest, int maxItems);

    int comparatorSignal() const;

private:
    std::vector<ItemSlot> slots;
};

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

}
