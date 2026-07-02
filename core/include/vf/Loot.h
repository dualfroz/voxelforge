#pragma once

#include <cstdint>
#include <vector>

#include "Damage.h"
#include "Enchant.h"
#include "Item.h"

namespace vf {

// A loot roll reads the kill exactly as combat reported it, so the two modules
// share one description of "what hit this".

// One candidate drop. `conditions` gate whether the entry is eligible at all,
// `functions` shape the count and the item once it has been picked.
struct LootEntry {
    static constexpr uint32_t PlayerKill = 1u << 0;
    static constexpr uint32_t Fiery = 1u << 1;
    static constexpr uint32_t NotFiery = 1u << 2;
    static constexpr uint32_t SilkTouched = 1u << 3;
    static constexpr uint32_t NotSilkTouched = 1u << 4;
    static constexpr uint32_t NeedsLooting = 1u << 5;
    static constexpr uint32_t NeedsFortune = 1u << 6;
    static constexpr uint32_t Explosive = 1u << 7;
    static constexpr uint32_t Ranged = 1u << 8;

    static constexpr uint32_t CountRange = 1u << 0;
    static constexpr uint32_t OreFortune = 1u << 1;
    static constexpr uint32_t LootingBonus = 1u << 2;
    static constexpr uint32_t ChanceLooting = 1u << 3;
    static constexpr uint32_t ChanceFortune = 1u << 4;
    static constexpr uint32_t SmeltIfFiery = 1u << 5;

    ItemId item = ItemId::None;
    // Relative pull inside its pool. A pool picks exactly one eligible entry per
    // roll, weighted by this.
    int weight = 1;
    int minCount = 1;
    int maxCount = 1;
    // A chance of one never touches the rng, which keeps single-entry pools from
    // shifting the stream for everything behind them.
    float chance = 1.0f;
    float chancePerLevel = 0.0f;
    uint32_t conditions = 0;
    uint32_t functions = 0;
};

// One pool fires `minRolls`..`maxRolls` times and each fire picks one entry.
struct LootPool {
    std::vector<LootEntry> entries;
    int minRolls = 1;
    int maxRolls = 1;
    int bonusRollsPerLooting = 0;
};

struct LootTable {
    enum class MobKind : uint8_t { Zombie, Skeleton, Creeper, Sheep, Cow, Pig, Count };
    enum class ChestKind : uint8_t { Dungeon, Village, Temple, Count };

    std::vector<LootPool> pools;

    bool empty() const { return pools.empty(); }
};

// Everything a roll needs to know about the circumstances of the drop. The
// enchantment queries read the tool that broke the block or landed the kill.
struct LootContext {
    uint32_t seed = 0;
    DamageType killedBy = DamageType::Melee;
    bool fiery = false;
    bool playerKill = false;
    ItemId tool = ItemId::None;
    EnchantSet toolEnchants;

    bool silkTouch() const { return toolEnchants.levelOf(EnchantKind::SilkTouch) > 0; }
    int looting() const { return toolEnchants.levelOf(EnchantKind::Looting); }
    int fortune() const { return toolEnchants.levelOf(EnchantKind::Fortune); }
};

struct LootRoll {
    std::vector<ItemSlot> items;

    int countOf(ItemId item) const;
    int totalCount() const;
    bool empty() const { return items.empty(); }
};

// Falls back to a fixed non-zero seed, because the xorshift stream is dead at
// zero and a caller that forgot to seed would otherwise silently get nothing.
LootContext lootContextFor(const HurtSource& source, ItemId tool, const EnchantSet& toolEnchants,
                           uint32_t seed);

// The cooked or smelted counterpart of a raw drop, or the input unchanged when
// it has none.
ItemId smeltedForm(ItemId raw);

// Whether the held tool is good enough to make the block drop anything. Only
// pickaxe blocks are gated; everything else drops for bare hands.
bool toolQualifiesFor(Block block, ItemId tool);

const LootTable& blockLootTable(Block block);
const LootTable& mobLootTable(LootTable::MobKind kind);
const LootTable& chestLootTable(LootTable::ChestKind kind);

// Deterministic in context.seed: the same context against the same table always
// produces the same roll.
LootRoll rollLootTable(const LootTable& table, const LootContext& context);

LootRoll rollBlockDrops(Block block, const LootContext& context);
LootRoll rollMobDrops(LootTable::MobKind kind, const LootContext& context);
LootRoll rollChestContents(LootTable::ChestKind kind, const LootContext& context);

} // namespace vf
