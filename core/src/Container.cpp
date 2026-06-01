#include "vf/Container.h"

#include <algorithm>
#include <cmath>

namespace vf {

bool Container::isEmpty() const {
    for (const ItemSlot& stack : slots)
        if (!stack.empty()) return false;
    return true;
}

bool Container::isFull() const {
    if (slots.empty()) return false;
    for (const ItemSlot& stack : slots) {
        if (stack.empty()) return false;
        if (stack.count < maxStackSize(stack.item)) return false;
    }
    return true;
}

int Container::usedSlots() const {
    int used = 0;
    for (const ItemSlot& stack : slots)
        if (!stack.empty()) ++used;
    return used;
}

int Container::totalItems() const {
    int total = 0;
    for (const ItemSlot& stack : slots)
        if (!stack.empty()) total += stack.count;
    return total;
}

int Container::insert(ItemSlot& incoming) {
    if (incoming.empty()) return 0;
    const int cap = maxStackSize(incoming.item);
    int inserted = 0;

    for (ItemSlot& stack : slots) {
        if (incoming.count <= 0) break;
        if (stack.empty()) continue;
        if (stack.item != incoming.item || stack.durability != incoming.durability) continue;
        int room = cap - stack.count;
        if (room <= 0) continue;
        int moved = std::min(room, incoming.count);
        stack.count += moved;
        incoming.count -= moved;
        inserted += moved;
    }

    for (ItemSlot& stack : slots) {
        if (incoming.count <= 0) break;
        if (!stack.empty()) continue;
        int moved = std::min(cap, incoming.count);
        stack = ItemSlot{incoming.item, moved, incoming.durability};
        incoming.count -= moved;
        inserted += moved;
    }

    if (incoming.count <= 0) incoming = ItemSlot{};
    return inserted;
}

int Container::insert(ItemId item, int count, int durability) {
    ItemSlot incoming{item, count, durability};
    insert(incoming);
    return incoming.count;
}

ItemSlot Container::extract(ItemId item, int amount) {
    if (item == ItemId::None || amount <= 0) return ItemSlot{};
    ItemSlot out{};
    for (ItemSlot& stack : slots) {
        if (amount <= 0) break;
        if (stack.empty() || stack.item != item) continue;
        if (out.count > 0 && out.durability != stack.durability) continue;
        int take = std::min(amount, stack.count);
        if (out.count == 0) {
            out.item = item;
            out.durability = stack.durability;
        }
        out.count += take;
        stack.count -= take;
        amount -= take;
        if (stack.count <= 0) stack = ItemSlot{};
    }
    return out.count > 0 ? out : ItemSlot{};
}

ItemSlot Container::extractSlot(int index, int amount) {
    return splitStack(slots[index], amount);
}

int Container::transferTo(Container& dest, int maxItems) {
    if (maxItems <= 0) return 0;
    int moved = 0;
    for (ItemSlot& stack : slots) {
        if (moved >= maxItems) break;
        if (stack.empty()) continue;
        int want = std::min(maxItems - moved, stack.count);
        ItemSlot piece{stack.item, want, stack.durability};
        int accepted = dest.insert(piece);
        if (accepted <= 0) continue;
        stack.count -= accepted;
        if (stack.count <= 0) stack = ItemSlot{};
        moved += accepted;
    }
    return moved;
}

int Container::comparatorSignal() const {
    if (slots.empty()) return 0;
    float fill = 0.0f;
    int filled = 0;
    for (const ItemSlot& stack : slots) {
        if (stack.empty()) continue;
        ++filled;
        fill += static_cast<float>(stack.count) / static_cast<float>(maxStackSize(stack.item));
    }
    if (filled == 0) return 0;
    fill /= static_cast<float>(slots.size());
    int signal = static_cast<int>(std::floor(fill * 14.0f)) + 1;
    return std::min(signal, redstoneComparatorMax);
}

Container& ContainerStore::ensure(const Vec3i& pos, int slotCount) {
    auto it = containers.find(pos);
    if (it != containers.end()) return it->second;
    auto inserted = containers.emplace(pos, Container{slotCount});
    return inserted.first->second;
}

Container* ContainerStore::at(const Vec3i& pos) {
    auto it = containers.find(pos);
    return it == containers.end() ? nullptr : &it->second;
}

const Container* ContainerStore::at(const Vec3i& pos) const {
    auto it = containers.find(pos);
    return it == containers.end() ? nullptr : &it->second;
}

} // namespace vf
