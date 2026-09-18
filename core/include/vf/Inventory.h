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

    void scroll(int steps) { select(selected + steps); }

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

}
