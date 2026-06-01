#pragma once

#include <array>
#include "Block.h"

namespace vf {

constexpr int hotbarSize = 9;
constexpr int stackLimit = 99;

struct ItemStack {
    Block block = Block::Air;
    int count = 0;
    bool empty() const { return count <= 0 || block == Block::Air; }
};

// The player's hotbar: a fixed row of stacks with one selected slot. Placing a
// block consumes from the selected stack; breaking a block returns it here.
class Hotbar {
public:
    Hotbar() {
        const Block starters[hotbarSize] = {Block::Grass, Block::Dirt,   Block::Stone,
                                            Block::Sand,   Block::Wood,   Block::Leaves,
                                            Block::Glowstone, Block::Water, Block::Snow};
        for (int i = 0; i < hotbarSize; ++i) slots[i] = {starters[i], 64};
    }

    int selectedIndex() const { return selected; }
    Block selectedBlock() const { return slots[selected].block; }
    const ItemStack& slot(int index) const { return slots[index]; }

    void select(int index) {
        selected = ((index % hotbarSize) + hotbarSize) % hotbarSize;
    }

    // Wheel or bracket keys: step the selection, wrapping both ends.
    void scroll(int steps) { select(selected + steps); }

    // Breaking a block: stack it onto a matching slot, else the first empty one.
    void add(Block block, int count) {
        if (block == Block::Air || count <= 0) return;
        for (ItemStack& stack : slots) {
            if (stack.block == block && stack.count < stackLimit) {
                stack.count += count;
                if (stack.count > stackLimit) stack.count = stackLimit;
                return;
            }
        }
        for (ItemStack& stack : slots) {
            if (stack.empty()) {
                stack = {block, count > stackLimit ? stackLimit : count};
                return;
            }
        }
    }

    // Placing a block: draw one from the selected stack. False if it was empty.
    bool consumeSelected() {
        ItemStack& stack = slots[selected];
        if (stack.empty()) return false;
        if (--stack.count <= 0) stack.count = 0;
        return true;
    }

private:
    std::array<ItemStack, hotbarSize> slots;
    int selected = 0;
};

} // namespace vf
