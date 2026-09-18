#pragma once

#include <cstdint>
#include <vector>

#include "Damage.h"
#include "Enchant.h"
#include "Item.h"

namespace vf {

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
    int weight = 1;
    int minCount = 1;
    int maxCount = 1;
    float chance = 1.0f;
    float chancePerLevel = 0.0f;
    uint32_t conditions = 0;
    uint32_t functions = 0;
};

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

LootContext lootContextFor(const HurtSource& source, ItemId tool, const EnchantSet& toolEnchants,
                           uint32_t seed);

ItemId smeltedForm(ItemId raw);

bool toolQualifiesFor(Block block, ItemId tool);

const LootTable& blockLootTable(Block block);
const LootTable& mobLootTable(LootTable::MobKind kind);
const LootTable& chestLootTable(LootTable::ChestKind kind);

LootRoll rollLootTable(const LootTable& table, const LootContext& context);

LootRoll rollBlockDrops(Block block, const LootContext& context);
LootRoll rollMobDrops(LootTable::MobKind kind, const LootContext& context);
LootRoll rollChestContents(LootTable::ChestKind kind, const LootContext& context);

}
