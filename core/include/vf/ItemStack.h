#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Enchant.h"
#include "Item.h"
#include "Nbt.h"

namespace vf {

struct Durability {
    int used = 0;
    int max = 0;

    bool damageable() const { return max > 0; }
    int remaining() const { return max > used ? max - used : 0; }
    bool broken() const { return max > 0 && used >= max; }
    float fraction() const;

    bool operator==(const Durability& other) const {
        return used == other.used && max == other.max;
    }
    bool operator!=(const Durability& other) const { return !(*this == other); }
};

struct StackMeta {
    Durability durability;
    EnchantSet enchants;
    std::string customName;
    bool unbreakable = false;
    int repairCost = 0;

    bool pristine() const;
    bool operator==(const StackMeta& other) const;
    bool operator!=(const StackMeta& other) const { return !(*this == other); }
};

bool sameEnchants(const EnchantSet& a, const EnchantSet& b);

int stackLimitFor(ItemId item);

struct MetaStack {
    ItemId item = ItemId::None;
    int count = 0;
    StackMeta meta;

    bool empty() const { return count <= 0 || item == ItemId::None; }
    int maxCount() const;
    int room() const;
    bool full() const { return !empty() && count >= maxCount(); }

    void clear();
};

MetaStack makeStack(ItemId item, int count);
MetaStack makeTool(ItemId toolItem);
MetaStack makeNamedStack(ItemId item, int count, const std::string& name);
MetaStack makeEnchantedTool(ItemId toolItem, const EnchantSet& enchants);

bool stacksMergeable(const MetaStack& a, const MetaStack& b);
bool stacksIdentical(const MetaStack& a, const MetaStack& b);

struct StackTransfer {
    int moved = 0;
    bool sourceEmptied = false;
    bool destinationFull = false;
    bool incompatible = false;
};

StackTransfer transferStack(MetaStack& from, MetaStack& into, int amount);
StackTransfer transferAll(MetaStack& from, MetaStack& into);

MetaStack splitOff(MetaStack& stack, int amount);
MetaStack splitHalf(MetaStack& stack);

int defragmentStacks(std::vector<MetaStack>& stacks);

int totalCountOf(const std::vector<MetaStack>& stacks, ItemId item);

bool stackOrderLess(const MetaStack& a, const MetaStack& b);
void sortStacks(std::vector<MetaStack>& stacks);

uint32_t stackRandom(uint32_t& state);

bool unbreakingSkipsWear(int unbreakingLevel, bool armorPiece, uint32_t& rng);

enum class WearResult : uint8_t {
    NotDamageable,
    Absorbed,
    Damaged,
    Broke,
    Survived,
};

WearResult damageStack(MetaStack& stack, int amount, uint32_t& rng);

int mendingRepair(MetaStack& stack, int experience);

MetaStack craftingRemainder(const MetaStack& ingredient);

ItemSlot toItemSlot(const MetaStack& stack);
MetaStack fromItemSlot(const ItemSlot& slot);

NbtTag stackToNbt(const MetaStack& stack);
bool stackFromNbt(const NbtTag& tag, MetaStack& out);

NbtTag stackListToNbt(const std::vector<MetaStack>& stacks);
bool stackListFromNbt(const NbtTag& tag, std::vector<MetaStack>& out);

}
