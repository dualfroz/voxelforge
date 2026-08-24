#include <algorithm>
#include <vector>

#include "testHarness.h"
#include "vf/LootTables.h"

using namespace vf;

namespace {

using Species = MobDropRule::Species;
using Structure = ChestFill::Structure;

uint32_t seedFor(int index) {
    uint32_t mixed = static_cast<uint32_t>(index) + 1u;
    mixed ^= mixed >> 16;
    mixed *= 0x7feb352du;
    mixed ^= mixed >> 15;
    mixed *= 0x846ca68bu;
    mixed ^= mixed >> 16;
    return mixed == 0u ? 1u : mixed;
}

LootContext contextWith(ItemId tool, uint32_t seed) {
    LootContext context;
    context.tool = tool;
    context.seed = seed;
    return context;
}

LootContext enchantedWith(ItemId tool, EnchantKind kind, int level, uint32_t seed) {
    LootContext context = contextWith(tool, seed);
    addEnchant(context.toolEnchants, kind, level);
    return context;
}

bool sameRoll(const LootRoll& left, const LootRoll& right) {
    if (left.items.size() != right.items.size()) return false;
    for (std::size_t i = 0; i < left.items.size(); ++i) {
        if (left.items[i].item != right.items[i].item) return false;
        if (left.items[i].count != right.items[i].count) return false;
        if (left.items[i].durability != right.items[i].durability) return false;
    }
    return true;
}

ItemId bestToolFor(ToolType type) {
    switch (type) {
        case ToolType::Pickaxe: return ItemId::DiamondPickaxe;
        case ToolType::Axe:     return ItemId::DiamondAxe;
        case ToolType::Shovel:  return ItemId::DiamondShovel;
        case ToolType::Sword:   return ItemId::DiamondSword;
        case ToolType::Hoe:     return ItemId::DiamondHoe;
        case ToolType::None:    return ItemId::None;
    }
    return ItemId::None;
}

const std::vector<ItemId>& toolPalette() {
    static const std::vector<ItemId> tools = {
        ItemId::None,           ItemId::WoodPickaxe,    ItemId::WoodAxe,     ItemId::WoodShovel,
        ItemId::WoodSword,      ItemId::WoodHoe,        ItemId::StonePickaxe, ItemId::StoneAxe,
        ItemId::StoneShovel,    ItemId::StoneSword,     ItemId::StoneHoe,    ItemId::IronPickaxe,
        ItemId::IronAxe,        ItemId::IronShovel,     ItemId::IronSword,   ItemId::IronHoe,
        ItemId::GoldPickaxe,    ItemId::GoldAxe,        ItemId::GoldShovel,  ItemId::GoldSword,
        ItemId::GoldHoe,        ItemId::DiamondPickaxe, ItemId::DiamondAxe,  ItemId::DiamondShovel,
        ItemId::DiamondSword,   ItemId::DiamondHoe,
    };
    return tools;
}

bool ruleCanYield(const BlockDropRule& rule, ItemId item) {
    return item == rule.drop || item == rule.silkTouchDrop() || item == rule.firedDrop ||
           item == rule.bonusDrop;
}

bool poolCanYield(const LootPool& pool, ItemId item) {
    for (const LootEntry& entry : pool.entries) {
        if (entry.item == item) return true;
    }
    return false;
}

ItemId firstEntryOfPool(const LootTable& table, std::size_t poolIndex) {
    if (poolIndex >= table.pools.size()) return ItemId::None;
    if (table.pools[poolIndex].entries.empty()) return ItemId::None;
    return table.pools[poolIndex].entries[0].item;
}

} // namespace

VF_TEST(theRegistrysPublishedConstantsAreTheOnesTheBoundsWereDerivedFrom) {
    CHECK_EQ(fishingBaseWaitTicks, 600);
    CHECK_EQ(fishingMinWaitTicks, 100);
    CHECK_EQ(fishingLureTicksPerLevel, 100);
    CHECK_EQ(fishingFishWeight, 85);
    CHECK_EQ(fishingTreasureWeight, 5);
    CHECK_EQ(fishingJunkWeight, 10);
    CHECK_EQ(fishingLuckWeightShift, 2);
    CHECK_EQ(fishingFishWeight + fishingTreasureWeight + fishingJunkWeight, 100);
    CHECK_EQ(shearToolType, ToolType::Hoe);

    CHECK_EQ(static_cast<int>(Block::Count), 130);
    CHECK_EQ(static_cast<int>(Species::Count), 17);
    CHECK_EQ(static_cast<int>(Structure::Count), 6);
    CHECK_EQ(static_cast<int>(FishingLoot::Pool::Count), 3);
}

VF_TEST(everyBlockInThePaletteCarriesAnAuthoredRuleInEnumOrder) {
    const LootRegistry& registry = lootRegistry();
    CHECK(registry.coversEveryBlock());
    CHECK_EQ(registry.blockRules().size(), static_cast<std::size_t>(Block::Count));

    int outOfOrder = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        const BlockDropRule& rule = registry.blockRule(block);
        if (rule.block != block) ++outOfOrder;
        CHECK(rule.minCount >= 0);
        CHECK(rule.maxCount >= rule.minCount);
        CHECK(rule.dropChance > 0.0f);
        CHECK(rule.dropChance <= 1.0f);
    }
    CHECK_EQ(outOfOrder, 0);

    CHECK(registry.coversEveryMob());
    CHECK_EQ(registry.mobRules().size(), static_cast<std::size_t>(Species::Count));
    CHECK(registry.coversEveryStructure());
    CHECK_EQ(registry.chestFills().size(), static_cast<std::size_t>(Structure::Count));
}

VF_TEST(everyRuleNamesTheSameToolClassTheItemModuleAlreadyDeclares) {
    const LootRegistry& registry = lootRegistry();
    int mismatches = 0;
    int constrained = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        ToolType declared = correctToolFor(block);
        if (declared == ToolType::None) continue;
        ++constrained;
        if (registry.blockRule(block).harvestTool != declared) ++mismatches;
    }
    CHECK_EQ(mismatches, 0);
    CHECK_EQ(constrained, 28);
}

VF_TEST(theRegistryIsNeverMorePermissiveThanTheSalvagedToolCheck) {
    const LootRegistry& registry = lootRegistry();
    int leaks = 0;
    int gatedCases = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        for (ItemId tool : toolPalette()) {
            if (toolQualifiesFor(block, tool)) continue;
            ++gatedCases;
            if (!registry.rollBlock(block, contextWith(tool, 1234u)).empty()) ++leaks;
        }
    }
    CHECK_EQ(leaks, 0);
    CHECK(gatedCases > 100);
}

