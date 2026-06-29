#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Enchant.h"
#include "Item.h"
#include "Nbt.h"

namespace vf {

// The stack type this header introduces is deliberately NOT called ItemStack:
// vf::ItemStack is already taken by Inventory.h, where it means "a Block plus a
// count" and backs the legacy Hotbar. That older type cannot carry durability,
// enchantments or a custom name, which is why Enchant, Damage and Loot produce
// values with nowhere to live. MetaStack is the stack that can hold them, and
// both names coexist in one translation unit (see testHeaderCoexistence.cpp).

// Wear on a damageable item. `max` of zero means the item never wears out,
// which is the normal state for blocks, materials and food.
struct Durability {
    int used = 0;
    int max = 0;

    bool damageable() const { return max > 0; }
    int remaining() const { return max > used ? max - used : 0; }
    bool broken() const { return max > 0 && used >= max; }
    // 1.0 for an untouched item, 0.0 for one about to break.
    float fraction() const;

    bool operator==(const Durability& other) const {
        return used == other.used && max == other.max;
    }
    bool operator!=(const Durability& other) const { return !(*this == other); }
};

// Everything about one stack beyond "which item, how many". Two stacks merge
// only when their whole StackMeta compares equal, so a half-worn pickaxe never
// silently absorbs a fresh one and an enchanted sword stays distinct from a
// plain one.
struct StackMeta {
    Durability durability;
    EnchantSet enchants;
    std::string customName;
    bool unbreakable = false;
    // Anvil prior-work penalty, the same number Enchant.h's anvilCombine takes.
    int repairCost = 0;

    bool pristine() const;
    bool operator==(const StackMeta& other) const;
    bool operator!=(const StackMeta& other) const { return !(*this == other); }
};

// Order-insensitive comparison of two enchantment sets: the same enchantments
// listed in a different order describe the same item.
bool sameEnchants(const EnchantSet& a, const EnchantSet& b);

// Item.h's per-item cap. Kept as a one-line forwarder so the stack layer never
// grows a second opinion about how tall a stack may be.
int stackLimitFor(ItemId item);

struct MetaStack {
    ItemId item = ItemId::None;
    int count = 0;
    StackMeta meta;

    bool empty() const { return count <= 0 || item == ItemId::None; }
    // An item carrying per-instance state cannot share a slot with anything,
    // so it caps at one regardless of what Item.h says about its kind.
    int maxCount() const;
    int room() const;
    bool full() const { return !empty() && count >= maxCount(); }

    void clear();
};

MetaStack makeStack(ItemId item, int count);
// A tool at full durability, ready to be worn down by damageStack. Named
// makeTool rather than makeToolStack because Item.h already owns that name for
// the ItemSlot flavour, and overloading on return type alone is not a thing.
MetaStack makeTool(ItemId toolItem);
MetaStack makeNamedStack(ItemId item, int count, const std::string& name);
MetaStack makeEnchantedTool(ItemId toolItem, const EnchantSet& enchants);

// Same item and byte-for-byte the same metadata. Empty stacks never merge.
bool stacksMergeable(const MetaStack& a, const MetaStack& b);
// Mergeable and the same count, which is what a round trip has to reproduce.
bool stacksIdentical(const MetaStack& a, const MetaStack& b);

struct StackTransfer {
    int moved = 0;
    bool sourceEmptied = false;
    bool destinationFull = false;
    // The two stacks could never have merged: different item or different
    // metadata. Distinguishes "nothing fitted" from "nothing was allowed".
    bool incompatible = false;
};

// Moves up to `amount` items from `from` into `into`. An empty destination
// adopts the source's item and metadata; a non-empty one only accepts a
// mergeable stack. Whatever does not fit stays in `from`.
StackTransfer transferStack(MetaStack& from, MetaStack& into, int amount);
StackTransfer transferAll(MetaStack& from, MetaStack& into);

// Removes up to `amount` and returns it as its own stack, metadata copied.
MetaStack splitOff(MetaStack& stack, int amount);
// The right-click split: the larger half stays on the cursor, matching vanilla.
MetaStack splitHalf(MetaStack& stack);

// Merges every mergeable pair in place, packing items towards the front of the
// list. Returns how many slots ended up empty that were not empty before.
int defragmentStacks(std::vector<MetaStack>& stacks);

// Total items of one kind across a list, ignoring metadata.
int totalCountOf(const std::vector<MetaStack>& stacks, ItemId item);

// Sort key for an inventory sort button: real items before empty slots, then by
// item id, undamaged before damaged, fuller stacks first, then by custom name.
bool stackOrderLess(const MetaStack& a, const MetaStack& b);
void sortStacks(std::vector<MetaStack>& stacks);

// Xorshift32 shared by every random decision in this layer, so a seed fully
// determines a run. Matches the generator the combat and loot modules use.
uint32_t stackRandom(uint32_t& state);

// Vanilla Unbreaking: a tool ignores a wear point with probability
// level/(level+1), armour with the flatter 0.6 + 0.4/(level+1) curve that
// Damage.cpp already applies to worn pieces.
bool unbreakingSkipsWear(int unbreakingLevel, bool armorPiece, uint32_t& rng);

enum class WearResult : uint8_t {
    NotDamageable,
    Absorbed,   // Unbreaking or the unbreakable flag ate the whole hit
    Damaged,
    Broke,      // hit zero durability and the stack is gone
    Survived,   // hit zero durability but Mending held it at one point left
};

// Spends up to `amount` durability, rolling Unbreaking once per point. Clears
// the stack when it breaks. Mending keeps the item alive with a single point
// remaining instead of destroying it.
WearResult damageStack(MetaStack& stack, int amount, uint32_t& rng);

// Experience repair: two durability points per experience point, capped at what
// is actually missing. Returns the durability restored.
int mendingRepair(MetaStack& stack, int experience);

// What a recipe leaves behind when this item is used as an ingredient. A
// damageable ingredient is not eaten by the craft: it comes back one point
// worse, and only disappears once that point kills it. Anything else returns an
// empty stack, meaning "consumed".
MetaStack craftingRemainder(const MetaStack& ingredient);

// Bridge to Container.h, which stores ItemSlot: item, count and the durability
// still left. Enchantments and custom names have no home in an ItemSlot, so a
// round trip through this pair is lossy by construction. Use the Nbt pair below
// whenever the metadata has to survive.
ItemSlot toItemSlot(const MetaStack& stack);
MetaStack fromItemSlot(const ItemSlot& slot);

// Lossless persistence. An empty stack serialises to a compound holding only a
// zero id, so a fixed-size inventory keeps its slot positions.
NbtTag stackToNbt(const MetaStack& stack);
bool stackFromNbt(const NbtTag& tag, MetaStack& out);

NbtTag stackListToNbt(const std::vector<MetaStack>& stacks);
bool stackListFromNbt(const NbtTag& tag, std::vector<MetaStack>& out);

} // namespace vf
