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

VF_TEST(harvestLevelsGateTheOresExactlyWhereTheTiersSay) {
    CHECK_EQ(toolHarvestLevel(ToolTier::None), 0);
    CHECK_EQ(toolHarvestLevel(ToolTier::Wood), 1);
    CHECK_EQ(toolHarvestLevel(ToolTier::Gold), 1);
    CHECK_EQ(toolHarvestLevel(ToolTier::Stone), 2);
    CHECK_EQ(toolHarvestLevel(ToolTier::Iron), 3);
    CHECK_EQ(toolHarvestLevel(ToolTier::Diamond), 4);

    const LootRegistry& registry = lootRegistry();
    const struct { Block block; ItemId tool; bool allowed; } cases[] = {
        {Block::Stone, ItemId::None, false},
        {Block::Stone, ItemId::DiamondShovel, false},
        {Block::Stone, ItemId::WoodPickaxe, true},
        {Block::IronOre, ItemId::WoodPickaxe, false},
        {Block::IronOre, ItemId::GoldPickaxe, false},
        {Block::IronOre, ItemId::StonePickaxe, true},
        {Block::DiamondOre, ItemId::StonePickaxe, false},
        {Block::DiamondOre, ItemId::IronPickaxe, true},
        {Block::Obsidian, ItemId::IronPickaxe, false},
        {Block::Obsidian, ItemId::DiamondPickaxe, true},
        {Block::CryingObsidian, ItemId::IronPickaxe, false},
        {Block::CopperOre, ItemId::WoodPickaxe, false},
        {Block::CopperOre, ItemId::StonePickaxe, true},
        {Block::Dirt, ItemId::None, true},
        {Block::Wood, ItemId::None, true},
    };
    for (const auto& item : cases) {
        CHECK_EQ(toolAllows(registry.blockRule(item.block), item.tool), item.allowed);
    }

    CHECK(registry.rollBlock(Block::Obsidian, contextWith(ItemId::IronPickaxe, 9u)).empty());
    CHECK_EQ(registry.rollBlock(Block::Obsidian, contextWith(ItemId::DiamondPickaxe, 9u))
                 .countOf(ItemId::Obsidian),
             1);
}

VF_TEST(stoneBecomesCobblestoneUnlessItIsTakenWholeOrBrokenInsideFire) {
    const LootRegistry& registry = lootRegistry();

    LootContext plain = contextWith(ItemId::IronPickaxe, 17u);
    CHECK_EQ(registry.rollBlock(Block::Stone, plain).countOf(ItemId::Cobblestone), 1);

    LootContext silk = enchantedWith(ItemId::IronPickaxe, EnchantKind::SilkTouch, 1, 17u);
    LootRoll harvested = registry.rollBlock(Block::Stone, silk);
    CHECK_EQ(harvested.countOf(ItemId::Stone), 1);
    CHECK_EQ(harvested.totalCount(), 1);

    LootContext burning = contextWith(ItemId::IronPickaxe, 17u);
    burning.fiery = true;
    LootRoll melted = registry.rollBlock(Block::Stone, burning);
    CHECK_EQ(melted.countOf(ItemId::Stone), 1);
    CHECK_EQ(melted.countOf(ItemId::Cobblestone), 0);

    // Cobblestone bakes back into stone the same way, and iron and gold ore
    // come out as ingots.
    LootRoll cobble = registry.rollBlock(Block::Cobblestone, burning);
    CHECK_EQ(cobble.countOf(ItemId::Stone), 1);
    CHECK_EQ(registry.rollBlock(Block::IronOre, burning).countOf(ItemId::IronIngot), 1);
    CHECK_EQ(registry.rollBlock(Block::GoldOre, burning).countOf(ItemId::GoldIngot), 1);
    CHECK_EQ(registry.rollBlock(Block::IronOre, plain).countOf(ItemId::IronOre), 1);
}

VF_TEST(oreRulesMultiplyWithFortuneAndHandBackTheOreBlockUnderSilkTouch) {
    const LootRegistry& registry = lootRegistry();
    const struct { Block block; ItemId raw; int lowest; int highest; } ores[] = {
        {Block::CoalOre, ItemId::CoalLump, 1, 1},
        {Block::DiamondOre, ItemId::DiamondGem, 1, 1},
        {Block::RedstoneOre, ItemId::RedstoneDust, 4, 5},
        {Block::LapisOre, ItemId::LapisLazuli, 4, 9},
        {Block::Glowstone, ItemId::Glowstone, 2, 4},
    };
    for (const auto& ore : ores) {
        int seenLowest = 1000;
        int seenHighest = 0;
        int fortuneHighest = 0;
        for (int i = 0; i < 500; ++i) {
            LootContext plain = contextWith(ItemId::DiamondPickaxe, seedFor(i));
            int count = registry.rollBlock(ore.block, plain).countOf(ore.raw);
            seenLowest = std::min(seenLowest, count);
            seenHighest = std::max(seenHighest, count);

            LootContext lucky = enchantedWith(ItemId::DiamondPickaxe, EnchantKind::Fortune, 3,
                                              seedFor(i));
            fortuneHighest = std::max(fortuneHighest,
                                      registry.rollBlock(ore.block, lucky).countOf(ore.raw));
        }
        CHECK_EQ(seenLowest, ore.lowest);
        CHECK_EQ(seenHighest, ore.highest);
        CHECK_EQ(fortuneHighest, ore.highest * 4);

        LootContext silk = enchantedWith(ItemId::DiamondPickaxe, EnchantKind::SilkTouch, 1, 5u);
        LootRoll whole = registry.rollBlock(ore.block, silk);
        CHECK_EQ(whole.countOf(itemForBlock(ore.block)), 1);
        CHECK_EQ(whole.totalCount(), 1);
    }
}

VF_TEST(shearClassToolsTakeTheBlockItselfWithoutNeedingSilkTouch) {
    const LootRegistry& registry = lootRegistry();

    for (ItemId hoe : {ItemId::WoodHoe, ItemId::DiamondHoe}) {
        LootRoll sheared = registry.rollBlock(Block::Leaves, contextWith(hoe, 42u));
        CHECK_EQ(sheared.countOf(ItemId::Leaves), 1);
        CHECK_EQ(sheared.totalCount(), 1);
    }

    // A pickaxe falls through to the chance table instead.
    int leafBlocks = 0;
    int cuttings = 0;
    for (int i = 0; i < 800; ++i) {
        LootRoll roll = registry.rollBlock(Block::Leaves,
                                           contextWith(ItemId::IronPickaxe, seedFor(i)));
        leafBlocks += roll.countOf(ItemId::Leaves);
        cuttings += roll.countOf(ItemId::Wood);
    }
    CHECK_EQ(leafBlocks, 0);
    CHECK(cuttings > 0);

    // Shearing a block the palette has no item for still yields nothing.
    for (Block block : {Block::Wool, Block::Cobweb, Block::Vine, Block::Kelp, Block::Moss}) {
        CHECK(registry.blockRule(block).shearHarvest);
        CHECK(registry.rollBlock(block, contextWith(ItemId::DiamondHoe, 8u)).empty());
    }
}

VF_TEST(everyBlockRuleOnlyEverProducesItemsItDeclaredUpFront) {
    const LootRegistry& registry = lootRegistry();
    int strayItems = 0;
    int rollsMade = 0;

    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        const BlockDropRule& rule = registry.blockRule(block);

        for (int sample = 0; sample < 12; ++sample) {
            LootContext context = contextWith(bestToolFor(rule.harvestTool), seedFor(sample));
            if (sample % 3 == 1) addEnchant(context.toolEnchants, EnchantKind::SilkTouch, 1);
            if (sample % 3 == 2) addEnchant(context.toolEnchants, EnchantKind::Fortune, 3);
            if (sample % 4 == 3) context.fiery = true;

            LootRoll roll = registry.rollBlock(block, context);
            ++rollsMade;
            for (const ItemSlot& stack : roll.items) {
                if (!ruleCanYield(rule, stack.item)) ++strayItems;
                if (stack.count <= 0) ++strayItems;
            }
        }
    }
    CHECK_EQ(strayItems, 0);
    CHECK_EQ(rollsMade, static_cast<int>(Block::Count) * 12);
}

VF_TEST(everyBlockThatDeclaresACertainDropActuallyHandsItOver) {
    const LootRegistry& registry = lootRegistry();
    int certainDrops = 0;
    int missed = 0;

    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        const BlockDropRule& rule = registry.blockRule(block);
        if (rule.drop == ItemId::None || rule.dropChance < 1.0f) continue;
        ++certainDrops;

        LootContext context = contextWith(bestToolFor(rule.harvestTool), seedFor(index));
        LootRoll roll = registry.rollBlock(block, context);
        if (roll.countOf(rule.drop) < rule.minCount) ++missed;
    }
    CHECK_EQ(missed, 0);
    CHECK_EQ(certainDrops, 62);
}

VF_TEST(blocksTheItemPaletteCannotExpressStillCarryARuleThatYieldsNothing) {
    const LootRegistry& registry = lootRegistry();
    int barren = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        const BlockDropRule& rule = registry.blockRule(block);
        if (!rule.yieldsNothing()) continue;
        ++barren;
        CHECK_EQ(rule.block, block);
        CHECK(blockRuleTable(rule).empty());

        LootContext generous = enchantedWith(bestToolFor(rule.harvestTool), EnchantKind::Fortune, 3,
                                             seedFor(index));
        addEnchant(generous.toolEnchants, EnchantKind::SilkTouch, 1);
        CHECK(registry.rollBlock(block, generous).empty());
    }
    CHECK_EQ(barren, static_cast<int>(Block::Count) - 64);
}

VF_TEST(theSameSeedReproducesTheSameDropForEveryBlockInThePalette) {
    const LootRegistry& registry = lootRegistry();
    int divergences = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        LootContext context = enchantedWith(ItemId::DiamondPickaxe, EnchantKind::Fortune, 2,
                                            seedFor(index));
        LootRoll first = registry.rollBlock(block, context);
        LootRoll second = registry.rollBlock(block, context);
        if (!sameRoll(first, second)) ++divergences;
    }
    CHECK_EQ(divergences, 0);
}

VF_TEST(everyMobRuleOnlyEverProducesItemsItDeclaredUpFront) {
    const LootRegistry& registry = lootRegistry();
    int strayItems = 0;
    for (int index = 0; index < static_cast<int>(Species::Count); ++index) {
        Species species = static_cast<Species>(index);
        const MobDropRule& rule = registry.mobRule(species);
        CHECK_EQ(rule.species, species);
        CHECK(rule.maxCount >= rule.minCount);

        for (int sample = 0; sample < 40; ++sample) {
            LootContext context = contextWith(ItemId::IronSword, seedFor(sample));
            context.playerKill = sample % 2 == 0;
            context.fiery = sample % 3 == 0;
            if (sample % 5 == 0) addEnchant(context.toolEnchants, EnchantKind::Looting, 3);

            for (const ItemSlot& stack : registry.rollMob(species, context).items) {
                bool declared = stack.item == rule.drop || stack.item == rule.firedDrop ||
                                stack.item == rule.rareDrop;
                if (!declared) ++strayItems;
            }
        }
    }
    CHECK_EQ(strayItems, 0);
    CHECK(registry.rollMob(Species::Count, contextWith(ItemId::None, 1u)).empty());
}

VF_TEST(theRegistryAgreesWithTheSalvagedMobTablesOnEverySharedSpecies) {
    const LootRegistry& registry = lootRegistry();
    const struct { Species species; LootTable::MobKind kind; } shared[] = {
        {Species::Zombie, LootTable::MobKind::Zombie},
        {Species::Skeleton, LootTable::MobKind::Skeleton},
        {Species::Creeper, LootTable::MobKind::Creeper},
        {Species::Sheep, LootTable::MobKind::Sheep},
        {Species::Cow, LootTable::MobKind::Cow},
        {Species::Pig, LootTable::MobKind::Pig},
    };
    for (const auto& pair : shared) {
        const MobDropRule& rule = registry.mobRule(pair.species);
        const LootTable& salvaged = mobLootTable(pair.kind);
        CHECK_EQ(rule.drop, firstEntryOfPool(salvaged, 0));
        if (rule.rareDrop != ItemId::None) {
            CHECK_EQ(rule.rareDrop, firstEntryOfPool(salvaged, 1));
        }
    }
}

VF_TEST(livestockComesOutCookedWhenTheKillWasFieryAndRawOtherwise) {
    const LootRegistry& registry = lootRegistry();
    const struct { Species species; ItemId raw; ItemId cooked; } herd[] = {
        {Species::Sheep, ItemId::RawMutton, ItemId::CookedMutton},
        {Species::Cow, ItemId::RawBeef, ItemId::CookedBeef},
        {Species::Pig, ItemId::RawPorkchop, ItemId::CookedPorkchop},
    };
    for (const auto& animal : herd) {
        CHECK(!registry.mobRule(animal.species).hostile);
        int raw = 0;
        int cooked = 0;
        for (int i = 0; i < 300; ++i) {
            LootContext cold = contextWith(ItemId::IronSword, seedFor(i));
            LootRoll rawRoll = registry.rollMob(animal.species, cold);
            raw += rawRoll.countOf(animal.raw);
            CHECK_EQ(rawRoll.countOf(animal.cooked), 0);

            LootContext burning = cold;
            burning.fiery = true;
            LootRoll cookedRoll = registry.rollMob(animal.species, burning);
            cooked += cookedRoll.countOf(animal.cooked);
            CHECK_EQ(cookedRoll.countOf(animal.raw), 0);
        }
        CHECK(raw > 0);
        CHECK_EQ(raw, cooked);
    }
}

VF_TEST(dropsThatNeedAPlayerKillNeverShowUpFromAnEnvironmentalDeath) {
    const LootRegistry& registry = lootRegistry();
    CHECK(registry.mobRule(Species::Blaze).dropNeedsPlayerKill);

    int fromPlayers = 0;
    int fromTheWorld = 0;
    int witchGlowstoneFromTheWorld = 0;
    for (int i = 0; i < 2000; ++i) {
        LootContext byPlayer = contextWith(ItemId::IronSword, seedFor(i));
        byPlayer.playerKill = true;
        LootContext byWorld = contextWith(ItemId::None, seedFor(i));

        fromPlayers += registry.rollMob(Species::Blaze, byPlayer).countOf(ItemId::CoalLump);
        fromTheWorld += registry.rollMob(Species::Blaze, byWorld).countOf(ItemId::CoalLump);
        witchGlowstoneFromTheWorld +=
            registry.rollMob(Species::Witch, byWorld).countOf(ItemId::Glowstone);
    }
    CHECK(fromPlayers > 0);
    CHECK_EQ(fromTheWorld, 0);

    // A witch is the counter example: her glowstone is not player gated.
    CHECK(!registry.mobRule(Species::Witch).rareNeedsPlayerKill);
    CHECK(witchGlowstoneFromTheWorld > 0);
}

VF_TEST(ironGolemsIgnoreLootingWhileZombiesDoNot) {
    const LootRegistry& registry = lootRegistry();
    CHECK(!registry.mobRule(Species::IronGolem).lootingRaisesCount);

    int golemLowest = 100;
    int golemHighest = 0;
    int golemLootedHighest = 0;
    int zombieHighest = 0;
    int zombieLootedHighest = 0;
    for (int i = 0; i < 1500; ++i) {
        LootContext plain = contextWith(ItemId::IronSword, seedFor(i));
        plain.playerKill = true;
        LootContext looted = plain;
        addEnchant(looted.toolEnchants, EnchantKind::Looting, 3);

        int golem = registry.rollMob(Species::IronGolem, plain).countOf(ItemId::IronIngot);
        golemLowest = std::min(golemLowest, golem);
        golemHighest = std::max(golemHighest, golem);
        golemLootedHighest = std::max(golemLootedHighest,
                                      registry.rollMob(Species::IronGolem, looted)
                                          .countOf(ItemId::IronIngot));

        zombieHighest = std::max(zombieHighest,
                                 registry.rollMob(Species::Zombie, plain)
                                     .countOf(ItemId::RottenFlesh));
        zombieLootedHighest = std::max(zombieLootedHighest,
                                       registry.rollMob(Species::Zombie, looted)
                                           .countOf(ItemId::RottenFlesh));
    }
    CHECK_EQ(golemLowest, 3);
    CHECK_EQ(golemHighest, 5);
    CHECK_EQ(golemLootedHighest, 5);
    CHECK_EQ(zombieHighest, 2);
    CHECK_EQ(zombieLootedHighest, 5);
}

VF_TEST(villagersLeaveNothingBehindAndSayItInTheirRule) {
    const LootRegistry& registry = lootRegistry();
    const MobDropRule& villager = registry.mobRule(Species::Villager);
    CHECK_EQ(villager.drop, ItemId::None);
    CHECK_EQ(villager.rareDrop, ItemId::None);
    CHECK(!villager.hostile);
    CHECK(mobRuleTable(villager).empty());

    for (int i = 0; i < 200; ++i) {
        LootContext context = enchantedWith(ItemId::DiamondSword, EnchantKind::Looting, 3,
                                            seedFor(i));
        context.playerKill = true;
        CHECK(registry.rollMob(Species::Villager, context).empty());
    }
}

VF_TEST(everyStructureChestFillsInsideItsDeclaredBoundsAndOnlyFromItsOwnPool) {
    const LootRegistry& registry = lootRegistry();
    const struct { Structure structure; int lowest; int highest; } bounds[] = {
        {Structure::Village, 3, 20},   {Structure::Dungeon, 2, 16},
        {Structure::Mineshaft, 3, 40}, {Structure::Temple, 2, 32},
        {Structure::NetherFortress, 2, 20}, {Structure::EndShip, 2, 18},
    };
    for (const auto& entry : bounds) {
        const ChestFill& fill = registry.chestFill(entry.structure);
        CHECK_EQ(fill.structure, entry.structure);
        CHECK(!fill.pool.entries.empty());

        int seenLowest = 1000;
        int seenHighest = 0;
        int strayItems = 0;
        for (int i = 0; i < 700; ++i) {
            LootRoll roll = registry.rollChest(entry.structure, contextWith(ItemId::None,
                                                                           seedFor(i)));
            seenLowest = std::min(seenLowest, roll.totalCount());
            seenHighest = std::max(seenHighest, roll.totalCount());
            for (const ItemSlot& stack : roll.items) {
                if (!poolCanYield(fill.pool, stack.item)) ++strayItems;
            }
        }
        CHECK_EQ(strayItems, 0);
        CHECK(seenLowest >= entry.lowest);
        CHECK(seenHighest <= entry.highest);
        CHECK(seenHighest > seenLowest);
    }
    CHECK(registry.rollChest(Structure::Count, contextWith(ItemId::None, 1u)).empty());
}

VF_TEST(structureChestCountsStayInsideTheRangeTheStructureDeclares) {
    const LootRegistry& registry = lootRegistry();
    for (int index = 0; index < static_cast<int>(Structure::Count); ++index) {
        Structure structure = static_cast<Structure>(index);
        const ChestFill& fill = registry.chestFill(structure);
        CHECK(fill.chestsMin >= 1);
        CHECK(fill.chestsMax >= fill.chestsMin);

        int seenLowest = 1000;
        int seenHighest = 0;
        for (int i = 0; i < 400; ++i) {
            int count = registry.chestCountFor(structure, seedFor(i));
            seenLowest = std::min(seenLowest, count);
            seenHighest = std::max(seenHighest, count);
        }
        CHECK_EQ(seenLowest, fill.chestsMin);
        CHECK_EQ(seenHighest, fill.chestsMax);
        CHECK_EQ(registry.chestCountFor(structure, 77u), registry.chestCountFor(structure, 77u));
    }
    CHECK_EQ(registry.chestCountFor(Structure::Count, 5u), 0);
}

VF_TEST(diamondGearOnlyTurnsUpInTheEndShipAndPickaxesOnlyInMineshafts) {
    const LootRegistry& registry = lootRegistry();
    int endShipSwords = 0;
    int mineshaftPickaxes = 0;
    int elsewhereGear = 0;

    for (int index = 0; index < static_cast<int>(Structure::Count); ++index) {
        Structure structure = static_cast<Structure>(index);
        for (int i = 0; i < 600; ++i) {
            LootRoll roll = registry.rollChest(structure, contextWith(ItemId::None, seedFor(i)));
            int swords = roll.countOf(ItemId::DiamondSword);
            int pickaxes = roll.countOf(ItemId::IronPickaxe);
            if (structure == Structure::EndShip) {
                endShipSwords += swords;
            } else {
                elsewhereGear += swords;
            }
            if (structure == Structure::Mineshaft) {
                mineshaftPickaxes += pickaxes;
            } else {
                elsewhereGear += pickaxes;
            }
        }
    }
    CHECK(endShipSwords > 0);
    CHECK(mineshaftPickaxes > 0);
    CHECK_EQ(elsewhereGear, 0);
}

VF_TEST(fishingSplitsIntoThreePoolsAndLuckShiftsTheWeightsAwayFromJunk) {
    const LootRegistry& registry = lootRegistry();
    int plainFish = 0;
    int plainTreasure = 0;
    int plainJunk = 0;
    int luckyTreasure = 0;
    int luckyJunk = 0;

    for (int i = 0; i < 4000; ++i) {
        FishingLoot plain = registry.rollFishing(contextWith(ItemId::WoodHoe, seedFor(i)));
        CHECK_EQ(plain.fishWeight, fishingFishWeight);
        CHECK_EQ(plain.treasureWeight, fishingTreasureWeight);
        CHECK_EQ(plain.junkWeight, fishingJunkWeight);
        if (plain.pool == FishingLoot::Pool::Fish) ++plainFish;
        if (plain.pool == FishingLoot::Pool::Treasure) ++plainTreasure;
        if (plain.pool == FishingLoot::Pool::Junk) ++plainJunk;

        FishingLoot lucky = registry.rollFishing(
            enchantedWith(ItemId::WoodHoe, EnchantKind::Fortune, 3, seedFor(i)));
        CHECK_EQ(lucky.treasureWeight, fishingTreasureWeight + 3 * fishingLuckWeightShift);
        CHECK_EQ(lucky.junkWeight, fishingJunkWeight - 3 * fishingLuckWeightShift);
        if (lucky.pool == FishingLoot::Pool::Treasure) ++luckyTreasure;
        if (lucky.pool == FishingLoot::Pool::Junk) ++luckyJunk;
    }
    CHECK(plainFish > plainTreasure + plainJunk);
    CHECK(plainTreasure > 0);
    CHECK(plainJunk > 0);
    CHECK(luckyTreasure > plainTreasure);
    CHECK(luckyJunk < plainJunk);

    // An over-cap Fortune cannot come from addEnchant, but it can be placed on
    // a set by hand, and it must clamp the junk weight at nothing rather than
    // going negative and eating the treasure band.
    int junkAtHighLuck = 0;
    for (int i = 0; i < 500; ++i) {
        LootContext overCharged = contextWith(ItemId::WoodHoe, seedFor(i));
        overCharged.toolEnchants.entries.push_back({EnchantKind::Fortune, 9});
        FishingLoot outcome = registry.rollFishing(overCharged);
        CHECK_EQ(outcome.junkWeight, 0);
        CHECK_EQ(outcome.treasureWeight, fishingTreasureWeight + 9 * fishingLuckWeightShift);
        if (outcome.pool == FishingLoot::Pool::Junk) ++junkAtHighLuck;
    }
    CHECK_EQ(junkAtHighLuck, 0);
}

VF_TEST(everyFishingPoolHandsBackSomethingFromItsOwnTable) {
    const LootRegistry& registry = lootRegistry();
    int emptyCatches = 0;
    int strayItems = 0;
    for (int i = 0; i < 2000; ++i) {
        FishingLoot outcome = registry.rollFishing(
            enchantedWith(ItemId::WoodHoe, EnchantKind::Fortune, 2, seedFor(i)));
        if (outcome.caught.empty()) ++emptyCatches;

        const LootTable& table = registry.tableFor(LootTableId::ofFishing(outcome.pool));
        for (const ItemSlot& stack : outcome.caught.items) {
            if (!poolCanYield(table.pools[0], stack.item)) ++strayItems;
        }
    }
    CHECK_EQ(emptyCatches, 0);
    CHECK_EQ(strayItems, 0);
}

VF_TEST(lureShortensTheWaitAndTheWaitNeverFallsThroughItsFloor) {
    const LootRegistry& registry = lootRegistry();
    int plainTotal = 0;
    int luredTotal = 0;
    int belowFloor = 0;
    int aboveCeiling = 0;

    for (int i = 0; i < 1500; ++i) {
        FishingLoot plain = registry.rollFishing(contextWith(ItemId::WoodHoe, seedFor(i)));
        FishingLoot lured = registry.rollFishing(
            enchantedWith(ItemId::WoodHoe, EnchantKind::Efficiency, 3, seedFor(i)));
        plainTotal += plain.waitTicks;
        luredTotal += lured.waitTicks;

        if (plain.waitTicks < fishingMinWaitTicks) ++belowFloor;
        if (plain.waitTicks > fishingBaseWaitTicks) ++aboveCeiling;
        if (lured.waitTicks < fishingMinWaitTicks / 2) ++belowFloor;
        if (lured.waitTicks > plain.waitTicks) ++aboveCeiling;
    }
    CHECK_EQ(belowFloor, 0);
    CHECK_EQ(aboveCeiling, 0);
    CHECK(luredTotal < plainTotal);

    FishingLoot drowned = registry.rollFishing(
        enchantedWith(ItemId::WoodHoe, EnchantKind::Efficiency, 5, seedFor(3)));
    CHECK_EQ(drowned.waitTicks, fishingMinWaitTicks / 2);
}

VF_TEST(fishingIsFullyDeterministicForAGivenSeedAndRod) {
    const LootRegistry& registry = lootRegistry();
    for (int i = 0; i < 200; ++i) {
        LootContext context = enchantedWith(ItemId::WoodHoe, EnchantKind::Fortune, 2, seedFor(i));
        FishingLoot first = registry.rollFishing(context);
        FishingLoot second = registry.rollFishing(context);
        CHECK_EQ(first.pool, second.pool);
        CHECK_EQ(first.waitTicks, second.waitTicks);
        CHECK(sameRoll(first.caught, second.caught));
    }
}

VF_TEST(lootTableHandlesRoundTripForEveryDomainAndRejectStaleIndices) {
    const LootRegistry& registry = lootRegistry();

    LootTableId stone = LootTableId::ofBlock(Block::Stone);
    CHECK_EQ(stone.domain, LootTableId::Domain::Block);
    CHECK_EQ(stone.index, static_cast<uint16_t>(Block::Stone));
    CHECK(stone.valid());
    CHECK(stone == LootTableId::ofBlock(Block::Stone));
    CHECK(stone != LootTableId::ofBlock(Block::Dirt));

    CHECK(LootTableId::ofMob(Species::Creeper).valid());
    CHECK(LootTableId::ofChest(Structure::EndShip).valid());
    CHECK(LootTableId::ofFishing(FishingLoot::Pool::Treasure).valid());

    CHECK(!LootTableId::ofBlock(Block::Count).valid());
    CHECK(!LootTableId::ofMob(Species::Count).valid());
    CHECK(!LootTableId::ofChest(Structure::Count).valid());
    CHECK(!LootTableId::ofFishing(FishingLoot::Pool::Count).valid());

    LootTableId stale;
    stale.domain = LootTableId::Domain::Block;
    stale.index = 60000;
    CHECK(!stale.valid());
    CHECK(registry.tableFor(stale).empty());

    LootTableId noDomain;
    noDomain.domain = LootTableId::Domain::Count;
    CHECK(!noDomain.valid());
    CHECK(registry.tableFor(noDomain).empty());
}

VF_TEST(aTableFetchedByHandleRollsIdenticallyToTheRegistryShortcut) {
    const LootRegistry& registry = lootRegistry();

    for (Block block : {Block::Stone, Block::DiamondOre, Block::Grass, Block::Bookshelf}) {
        LootContext context = contextWith(bestToolFor(registry.blockRule(block).harvestTool), 909u);
        LootRoll viaHandle = rollLootTable(registry.tableFor(LootTableId::ofBlock(block)), context);
        CHECK(sameRoll(viaHandle, registry.rollBlock(block, context)));
    }

    for (Species species : {Species::Zombie, Species::Witch, Species::Cow}) {
        LootContext context = contextWith(ItemId::IronSword, 1717u);
        context.playerKill = true;
        LootRoll viaHandle = rollLootTable(registry.tableFor(LootTableId::ofMob(species)), context);
        CHECK(sameRoll(viaHandle, registry.rollMob(species, context)));
    }

    for (Structure structure : {Structure::Temple, Structure::NetherFortress}) {
        LootContext context = contextWith(ItemId::None, 31337u);
        LootRoll viaHandle = rollLootTable(registry.tableFor(LootTableId::ofChest(structure)),
                                           context);
        CHECK(sameRoll(viaHandle, registry.rollChest(structure, context)));
    }
}

VF_TEST(ruleToTableConversionBuildsExactlyThePoolsTheRuleDescribes) {
    const LootRegistry& registry = lootRegistry();

    LootTable stone = blockRuleTable(registry.blockRule(Block::Stone));
    CHECK_EQ(stone.pools.size(), std::size_t{3});
    CHECK_EQ(firstEntryOfPool(stone, 0), ItemId::Stone);
    CHECK_EQ(firstEntryOfPool(stone, 1), ItemId::Cobblestone);
    CHECK_EQ(firstEntryOfPool(stone, 2), ItemId::Stone);
    CHECK_EQ(stone.pools[0].entries[0].conditions, LootEntry::SilkTouched);
    CHECK_EQ(stone.pools[1].entries[0].conditions,
             LootEntry::NotSilkTouched | LootEntry::NotFiery);
    CHECK_EQ(stone.pools[2].entries[0].conditions, LootEntry::NotSilkTouched | LootEntry::Fiery);

    LootTable leaves = blockRuleTable(registry.blockRule(Block::Leaves));
    CHECK_EQ(leaves.pools.size(), std::size_t{3});
    CHECK_EQ(firstEntryOfPool(leaves, 1), ItemId::Wood);
    CHECK_EQ(leaves.pools[1].entries[0].functions, LootEntry::ChanceFortune);
    CHECK_EQ(firstEntryOfPool(leaves, 2), ItemId::Stick);

    LootTable lapis = blockRuleTable(registry.blockRule(Block::LapisOre));
    CHECK_EQ(lapis.pools.size(), std::size_t{2});
    CHECK_EQ(lapis.pools[1].entries[0].functions, LootEntry::CountRange | LootEntry::OreFortune);

    CHECK(blockRuleTable(registry.blockRule(Block::Air)).empty());

    LootTable cow = mobRuleTable(registry.mobRule(Species::Cow));
    CHECK_EQ(cow.pools.size(), std::size_t{2});
    CHECK_EQ(firstEntryOfPool(cow, 0), ItemId::RawBeef);
    CHECK_EQ(firstEntryOfPool(cow, 1), ItemId::CookedBeef);
    CHECK_EQ(cow.pools[0].entries[0].conditions, LootEntry::NotFiery);
    CHECK_EQ(cow.pools[1].entries[0].conditions, LootEntry::Fiery);

    LootTable zombie = mobRuleTable(registry.mobRule(Species::Zombie));
    CHECK_EQ(zombie.pools.size(), std::size_t{2});
    CHECK_EQ(zombie.pools[1].entries[0].conditions, LootEntry::PlayerKill);
    CHECK_EQ(zombie.pools[1].entries[0].functions, LootEntry::ChanceLooting);
}

VF_TEST(theSharedRegistryIsBuiltOnceAndAlwaysHandsBackTheSameTables) {
    CHECK(&lootRegistry() == &lootRegistry());
    const LootRegistry& registry = lootRegistry();
    CHECK(&registry.blockRule(Block::Stone) == &registry.blockRule(Block::Stone));
    CHECK(&registry.tableFor(LootTableId::ofBlock(Block::Stone)) ==
          &registry.tableFor(LootTableId::ofBlock(Block::Stone)));

    LootRegistry rebuilt;
    CHECK(rebuilt.coversEveryBlock());
    int divergences = 0;
    for (int index = 0; index < static_cast<int>(Block::Count); ++index) {
        Block block = static_cast<Block>(index);
        LootContext context = contextWith(ItemId::DiamondPickaxe, seedFor(index));
        if (!sameRoll(rebuilt.rollBlock(block, context), registry.rollBlock(block, context))) {
            ++divergences;
        }
    }
    CHECK_EQ(divergences, 0);
}
