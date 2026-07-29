#include "vf/Loot.h"

#include <algorithm>

namespace vf {

namespace {

uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int rollBelow(uint32_t& rng, int bound) {
    if (bound <= 1) return 0;
    return static_cast<int>(xorshift(rng) % static_cast<uint32_t>(bound));
}

int rollBetween(uint32_t& rng, int low, int high) {
    if (high <= low) return low;
    return low + rollBelow(rng, high - low + 1);
}

float unitFloat(uint32_t& rng) { return static_cast<float>(xorshift(rng)) / 4294967295.0f; }

bool conditionsPass(const LootEntry& entry, const LootContext& context) {
    if ((entry.conditions & LootEntry::PlayerKill) && !context.playerKill) return false;
    if ((entry.conditions & LootEntry::Fiery) && !context.fiery) return false;
    if ((entry.conditions & LootEntry::NotFiery) && context.fiery) return false;
    if ((entry.conditions & LootEntry::SilkTouched) && !context.silkTouch()) return false;
    if ((entry.conditions & LootEntry::NotSilkTouched) && context.silkTouch()) return false;
    if ((entry.conditions & LootEntry::NeedsLooting) && context.looting() <= 0) return false;
    if ((entry.conditions & LootEntry::NeedsFortune) && context.fortune() <= 0) return false;
    if ((entry.conditions & LootEntry::Explosive) && context.killedBy != DamageType::Explosion) {
        return false;
    }
    if ((entry.conditions & LootEntry::Ranged) && !isProjectileDamage(context.killedBy)) {
        return false;
    }
    return true;
}

void addToRoll(LootRoll& roll, ItemId item, int count) {
    if (item == ItemId::None || count <= 0) return;
    if (isTool(item)) {
        for (int i = 0; i < count; ++i) roll.items.push_back(makeToolStack(item));
        return;
    }
    for (ItemSlot& stack : roll.items) {
        if (stack.item != item) continue;
        stack.count += count;
        return;
    }
    roll.items.push_back({item, count, 0});
}

void emitEntry(LootRoll& roll, const LootEntry& entry, const LootContext& context, uint32_t& rng) {
    float chance = entry.chance;
    if (entry.functions & LootEntry::ChanceLooting) {
        chance += entry.chancePerLevel * static_cast<float>(context.looting());
    }
    if (entry.functions & LootEntry::ChanceFortune) {
        chance += entry.chancePerLevel * static_cast<float>(context.fortune());
    }
    if (chance < 1.0f && unitFloat(rng) > chance) return;

    int count = entry.minCount;
    if (entry.functions & LootEntry::CountRange) {
        count = rollBetween(rng, entry.minCount, entry.maxCount);
    }
    if (entry.functions & LootEntry::OreFortune) {
        int fortune = context.fortune();
        if (fortune > 0) count *= std::max(1, rollBelow(rng, fortune + 2));
    }
    if (entry.functions & LootEntry::LootingBonus) {
        count += rollBelow(rng, context.looting() + 1);
    }
    ItemId item = entry.item;
    if ((entry.functions & LootEntry::SmeltIfFiery) && context.fiery) item = smeltedForm(item);
    addToRoll(roll, item, count);
}

LootEntry fixedDrop(ItemId item, int count = 1) {
    LootEntry entry;
    entry.item = item;
    entry.minCount = count;
    entry.maxCount = count;
    return entry;
}

LootEntry rangedDrop(ItemId item, int minCount, int maxCount) {
    LootEntry entry;
    entry.item = item;
    entry.minCount = minCount;
    entry.maxCount = maxCount;
    entry.functions = LootEntry::CountRange;
    return entry;
}

LootEntry weightedDrop(ItemId item, int weight, int minCount, int maxCount) {
    LootEntry entry = rangedDrop(item, minCount, maxCount);
    entry.weight = weight;
    return entry;
}

LootEntry chanceDrop(ItemId item, float chance, float perLevel, uint32_t chanceFunction) {
    LootEntry entry = fixedDrop(item);
    entry.chance = chance;
    entry.chancePerLevel = perLevel;
    entry.functions = chanceFunction;
    return entry;
}

void addPool(LootTable& table, const LootEntry& entry) {
    LootPool pool;
    pool.entries.push_back(entry);
    table.pools.push_back(pool);
}

LootTable oreTable(Block block, ItemId rawDrop, int minCount, int maxCount) {
    LootEntry silk = fixedDrop(itemForBlock(block));
    silk.conditions = LootEntry::SilkTouched;

    LootEntry raw = rangedDrop(rawDrop, minCount, maxCount);
    raw.conditions = LootEntry::NotSilkTouched;
    raw.functions = LootEntry::CountRange | LootEntry::OreFortune;

    LootTable table;
    addPool(table, silk);
    addPool(table, raw);
    return table;
}

LootTable selfDropTable(Block block) {
    LootTable table;
    ItemId self = itemForBlock(block);
    if (self != ItemId::None) addPool(table, fixedDrop(self));
    return table;
}

LootTable stoneTable() {
    LootEntry silk = fixedDrop(ItemId::Stone);
    silk.conditions = LootEntry::SilkTouched;
    LootEntry cobble = fixedDrop(ItemId::Cobblestone);
    cobble.conditions = LootEntry::NotSilkTouched;

    LootTable table;
    addPool(table, silk);
    addPool(table, cobble);
    return table;
}

LootTable leavesTable() {
    LootEntry silk = fixedDrop(ItemId::Leaves);
    silk.conditions = LootEntry::SilkTouched;

    // The item palette has no sapling, so a wood cutting stands in as the
    // replantable drop; Fortune improves its odds the way vanilla does.
    LootEntry cutting = chanceDrop(ItemId::Wood, 0.05f, 0.04f, LootEntry::ChanceFortune);
    cutting.conditions = LootEntry::NotSilkTouched;

    LootEntry apple = chanceDrop(ItemId::Apple, 0.005f, 0.0f, 0);
    apple.conditions = LootEntry::NotSilkTouched;

    LootEntry sticks = rangedDrop(ItemId::Stick, 1, 2);
    sticks.chance = 0.02f;
    sticks.conditions = LootEntry::NotSilkTouched;

    LootTable table;
    addPool(table, silk);
    addPool(table, cutting);
    addPool(table, apple);
    addPool(table, sticks);
    return table;
}

LootTable zombieTable() {
    LootEntry flesh = rangedDrop(ItemId::RottenFlesh, 0, 2);
    flesh.functions = LootEntry::CountRange | LootEntry::LootingBonus;

    LootEntry ingot = chanceDrop(ItemId::IronIngot, 0.025f, 0.01f, LootEntry::ChanceLooting);
    ingot.conditions = LootEntry::PlayerKill;

    LootTable table;
    addPool(table, flesh);
    addPool(table, ingot);
    return table;
}

LootTable skeletonTable() {
    LootEntry shafts = rangedDrop(ItemId::Stick, 0, 2);
    shafts.functions = LootEntry::CountRange | LootEntry::LootingBonus;

    LootEntry coal = chanceDrop(ItemId::CoalLump, 0.05f, 0.01f, LootEntry::ChanceLooting);
    coal.conditions = LootEntry::PlayerKill;

    LootTable table;
    addPool(table, shafts);
    addPool(table, coal);
    return table;
}

LootTable creeperTable() {
    LootEntry powder = rangedDrop(ItemId::Gunpowder, 0, 2);
    powder.functions = LootEntry::CountRange | LootEntry::LootingBonus;

    LootTable table;
    addPool(table, powder);
    return table;
}

LootTable meatTable(ItemId rawMeat, int minCount, int maxCount) {
    LootEntry meat = rangedDrop(rawMeat, minCount, maxCount);
    meat.functions = LootEntry::CountRange | LootEntry::LootingBonus | LootEntry::SmeltIfFiery;

    LootTable table;
    addPool(table, meat);
    return table;
}

LootTable dungeonChestTable() {
    LootPool pool;
    pool.minRolls = 2;
    pool.maxRolls = 4;
    pool.entries = {
        weightedDrop(ItemId::IronIngot, 10, 1, 4), weightedDrop(ItemId::Bread, 10, 1, 1),
        weightedDrop(ItemId::CoalLump, 10, 1, 4),  weightedDrop(ItemId::RedstoneDust, 5, 1, 4),
        weightedDrop(ItemId::GoldIngot, 5, 1, 4),  weightedDrop(ItemId::LapisLazuli, 5, 1, 3),
        weightedDrop(ItemId::Apple, 8, 1, 3),      weightedDrop(ItemId::DiamondGem, 1, 1, 2),
    };
    LootTable table;
    table.pools.push_back(pool);
    return table;
}

LootTable villageChestTable() {
    LootPool pool;
    pool.minRolls = 3;
    pool.maxRolls = 5;
    pool.entries = {
        weightedDrop(ItemId::Bread, 10, 1, 3),  weightedDrop(ItemId::Apple, 8, 1, 3),
        weightedDrop(ItemId::Stick, 6, 1, 4),   weightedDrop(ItemId::IronIngot, 5, 1, 2),
        weightedDrop(ItemId::WoodHoe, 3, 1, 1), weightedDrop(ItemId::EmeraldGem, 2, 1, 2),
    };
    LootTable table;
    table.pools.push_back(pool);
    return table;
}

LootTable templeChestTable() {
    LootPool pool;
    pool.minRolls = 2;
    pool.maxRolls = 4;
    pool.entries = {
        weightedDrop(ItemId::Gunpowder, 5, 1, 8),   weightedDrop(ItemId::RottenFlesh, 5, 3, 7),
        weightedDrop(ItemId::GoldIngot, 5, 1, 3),   weightedDrop(ItemId::Bread, 5, 1, 1),
        weightedDrop(ItemId::LapisLazuli, 2, 1, 3), weightedDrop(ItemId::DiamondGem, 1, 1, 3),
        weightedDrop(ItemId::EmeraldGem, 1, 1, 3),
    };
    LootTable table;
    table.pools.push_back(pool);
    return table;
}

std::vector<LootTable> buildBlockTables() {
    std::size_t blockCount = static_cast<std::size_t>(Block::Count);
    std::vector<LootTable> tables(blockCount);
    for (std::size_t i = 0; i < blockCount; ++i) {
        tables[i] = selfDropTable(static_cast<Block>(i));
    }
    tables[static_cast<std::size_t>(Block::Bedrock)] = LootTable{};
    tables[static_cast<std::size_t>(Block::Stone)] = stoneTable();
    tables[static_cast<std::size_t>(Block::Grass)] = selfDropTable(Block::Dirt);
    tables[static_cast<std::size_t>(Block::Leaves)] = leavesTable();
    tables[static_cast<std::size_t>(Block::CoalOre)] =
        oreTable(Block::CoalOre, ItemId::CoalLump, 1, 1);
    tables[static_cast<std::size_t>(Block::IronOre)] =
        oreTable(Block::IronOre, ItemId::IronOre, 1, 1);
    tables[static_cast<std::size_t>(Block::GoldOre)] =
        oreTable(Block::GoldOre, ItemId::GoldOre, 1, 1);
    tables[static_cast<std::size_t>(Block::DiamondOre)] =
        oreTable(Block::DiamondOre, ItemId::DiamondGem, 1, 1);
    tables[static_cast<std::size_t>(Block::EmeraldOre)] =
        oreTable(Block::EmeraldOre, ItemId::EmeraldGem, 1, 1);
    tables[static_cast<std::size_t>(Block::LapisOre)] =
        oreTable(Block::LapisOre, ItemId::LapisLazuli, 4, 9);
    tables[static_cast<std::size_t>(Block::RedstoneOre)] =
        oreTable(Block::RedstoneOre, ItemId::RedstoneDust, 4, 5);
    tables[static_cast<std::size_t>(Block::Glowstone)] =
        oreTable(Block::Glowstone, ItemId::Glowstone, 2, 4);
    return tables;
}

const LootTable& emptyTable() {
    static const LootTable table;
    return table;
}

} // namespace

int LootRoll::countOf(ItemId item) const {
    int total = 0;
    for (const ItemSlot& stack : items) {
        if (stack.item == item) total += stack.count;
    }
    return total;
}

int LootRoll::totalCount() const {
    int total = 0;
    for (const ItemSlot& stack : items) total += stack.count;
    return total;
}

LootContext lootContextFor(const HurtSource& source, ItemId tool, const EnchantSet& toolEnchants,
                           uint32_t seed) {
    LootContext context;
    context.seed = seed != 0 ? seed : 0x9e3779b9u;
    context.killedBy = source.type;
    context.fiery = source.fiery || isFireDamage(source.type);
    context.playerKill = source.byPlayer;
    context.tool = tool;
    context.toolEnchants = toolEnchants;
    return context;
}

ItemId smeltedForm(ItemId raw) {
    switch (raw) {
        case ItemId::RawPorkchop: return ItemId::CookedPorkchop;
        case ItemId::RawBeef:     return ItemId::CookedBeef;
        case ItemId::RawMutton:   return ItemId::CookedMutton;
        case ItemId::IronOre:     return ItemId::IronIngot;
        case ItemId::GoldOre:     return ItemId::GoldIngot;
        default:                  return raw;
    }
}

bool toolQualifiesFor(Block block, ItemId tool) {
    if (correctToolFor(block) != ToolType::Pickaxe) return true;

    const ItemInfo& info = itemInfo(tool);
    if (info.toolType != ToolType::Pickaxe) return false;
    switch (block) {
        case Block::DiamondOre:
        case Block::EmeraldOre:
        case Block::GoldOre:
            return info.toolTier == ToolTier::Iron || info.toolTier == ToolTier::Diamond;
        case Block::IronOre:
        case Block::LapisOre:
        case Block::RedstoneOre:
            return info.toolTier != ToolTier::Wood && info.toolTier != ToolTier::Gold;
        default:
            return true;
    }
}

const LootTable& blockLootTable(Block block) {
    static const std::vector<LootTable> tables = buildBlockTables();
    if (block >= Block::Count) return emptyTable();
    return tables[static_cast<std::size_t>(block)];
}

const LootTable& mobLootTable(LootTable::MobKind kind) {
    static const std::vector<LootTable> tables = {
        zombieTable(),
        skeletonTable(),
        creeperTable(),
        meatTable(ItemId::RawMutton, 1, 2),
        meatTable(ItemId::RawBeef, 1, 3),
        meatTable(ItemId::RawPorkchop, 1, 3),
    };
    if (kind >= LootTable::MobKind::Count) return emptyTable();
    return tables[static_cast<std::size_t>(kind)];
}

const LootTable& chestLootTable(LootTable::ChestKind kind) {
    static const std::vector<LootTable> tables = {
        dungeonChestTable(),
        villageChestTable(),
        templeChestTable(),
    };
    if (kind >= LootTable::ChestKind::Count) return emptyTable();
    return tables[static_cast<std::size_t>(kind)];
}

LootRoll rollLootTable(const LootTable& table, const LootContext& context) {
    LootRoll roll;
    uint32_t rng = context.seed != 0 ? context.seed : 0x9e3779b9u;

    for (const LootPool& pool : table.pools) {
        int rolls = rollBetween(rng, pool.minRolls, pool.maxRolls);
        rolls += pool.bonusRollsPerLooting * context.looting();
        for (int r = 0; r < rolls; ++r) {
            int totalWeight = 0;
            for (const LootEntry& entry : pool.entries) {
                if (conditionsPass(entry, context)) totalWeight += entry.weight;
            }
            if (totalWeight <= 0) continue;
            int pick = rollBelow(rng, totalWeight);
            for (const LootEntry& entry : pool.entries) {
                if (!conditionsPass(entry, context)) continue;
                pick -= entry.weight;
                if (pick >= 0) continue;
                emitEntry(roll, entry, context, rng);
                break;
            }
        }
    }
    return roll;
}

LootRoll rollBlockDrops(Block block, const LootContext& context) {
    if (!toolQualifiesFor(block, context.tool)) return {};
    return rollLootTable(blockLootTable(block), context);
}

LootRoll rollMobDrops(LootTable::MobKind kind, const LootContext& context) {
    return rollLootTable(mobLootTable(kind), context);
}

LootRoll rollChestContents(LootTable::ChestKind kind, const LootContext& context) {
    return rollLootTable(chestLootTable(kind), context);
}

} // namespace vf
