#include <algorithm>

#include "testHarness.h"
#include "vf/Loot.h"

using namespace vf;

namespace {

// Consecutive small seeds share too much structure for a raw xorshift stream,
// so the sampling tests walk a scrambled sequence instead.
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

LootContext enchanted(ItemId tool, EnchantKind kind, int level, uint32_t seed) {
    LootContext context = contextWith(tool, seed);
    addEnchant(context.toolEnchants, kind, level);
    return context;
}

LootContext killContext(bool byPlayer, uint32_t seed) {
    LootContext context = contextWith(ItemId::IronSword, seed);
    context.playerKill = byPlayer;
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

LootEntry entryOf(ItemId item, uint32_t conditions) {
    LootEntry entry;
    entry.item = item;
    entry.conditions = conditions;
    return entry;
}

LootTable singlePool(const std::vector<LootEntry>& entries, int minRolls = 1, int maxRolls = 1) {
    LootPool pool;
    pool.entries = entries;
    pool.minRolls = minRolls;
    pool.maxRolls = maxRolls;
    LootTable table;
    table.pools.push_back(pool);
    return table;
}

int maxOverSeeds(Block block, const EnchantSet& enchants, ItemId tool, ItemId wanted, int samples) {
    int best = 0;
    for (int i = 0; i < samples; ++i) {
        LootContext context = contextWith(tool, seedFor(i));
        context.toolEnchants = enchants;
        best = std::max(best, rollBlockDrops(block, context).countOf(wanted));
    }
    return best;
}

} // namespace

VF_TEST(theSameSeedAlwaysReproducesTheSameRollAcrossEveryTableFamily) {
    LootContext blockContext = enchanted(ItemId::DiamondPickaxe, EnchantKind::Fortune, 3, 0xabcdef01u);
    CHECK(sameRoll(rollBlockDrops(Block::LapisOre, blockContext),
                   rollBlockDrops(Block::LapisOre, blockContext)));

    LootContext mobContext = killContext(true, 0x1234567u);
    addEnchant(mobContext.toolEnchants, EnchantKind::Looting, 2);
    CHECK(sameRoll(rollMobDrops(LootTable::MobKind::Zombie, mobContext),
                   rollMobDrops(LootTable::MobKind::Zombie, mobContext)));

    LootContext chestContext = contextWith(ItemId::None, 0x55aa33ccu);
    LootRoll first = rollChestContents(LootTable::ChestKind::Dungeon, chestContext);
    LootRoll second = rollChestContents(LootTable::ChestKind::Dungeon, chestContext);
    CHECK(sameRoll(first, second));
    CHECK(first.totalCount() > 0);
}

VF_TEST(differentSeedsPullApartTheContentsOfTheSameChest) {
    LootRoll reference = rollChestContents(LootTable::ChestKind::Dungeon,
                                           contextWith(ItemId::None, seedFor(0)));
    int different = 0;
    for (int i = 1; i < 64; ++i) {
        LootRoll roll = rollChestContents(LootTable::ChestKind::Dungeon,
                                          contextWith(ItemId::None, seedFor(i)));
        if (!sameRoll(reference, roll)) ++different;
    }
    CHECK(different >= 55);
}

VF_TEST(stoneDropsCobblestoneUnlessTheSilkTouchBranchTakesOver) {
    LootContext plain = contextWith(ItemId::StonePickaxe, 7u);
    LootRoll broken = rollBlockDrops(Block::Stone, plain);
    CHECK_EQ(broken.countOf(ItemId::Cobblestone), 1);
    CHECK_EQ(broken.countOf(ItemId::Stone), 0);

    LootContext silk = enchanted(ItemId::StonePickaxe, EnchantKind::SilkTouch, 1, 7u);
    LootRoll harvested = rollBlockDrops(Block::Stone, silk);
    CHECK_EQ(harvested.countOf(ItemId::Stone), 1);
    CHECK_EQ(harvested.countOf(ItemId::Cobblestone), 0);
}

VF_TEST(oresDropTheirRawFormAndSilkTouchDropsTheOreBlockItself) {
    const struct { Block block; ItemId raw; ItemId oreItem; } ores[] = {
        {Block::CoalOre, ItemId::CoalLump, ItemId::CoalOre},
        {Block::IronOre, ItemId::IronOre, ItemId::IronOre},
        {Block::DiamondOre, ItemId::DiamondGem, ItemId::DiamondOre},
        {Block::EmeraldOre, ItemId::EmeraldGem, ItemId::EmeraldOre},
    };
    for (const auto& ore : ores) {
        LootContext plain = contextWith(ItemId::DiamondPickaxe, 31u);
        CHECK_EQ(rollBlockDrops(ore.block, plain).countOf(ore.raw), 1);

        LootContext silk = enchanted(ItemId::DiamondPickaxe, EnchantKind::SilkTouch, 1, 31u);
        LootRoll harvested = rollBlockDrops(ore.block, silk);
        CHECK_EQ(harvested.countOf(ore.oreItem), 1);
        CHECK_EQ(harvested.totalCount(), 1);
    }
}

VF_TEST(fortuneMultipliesOreDropsAndNeverDropsBelowTheBaseYield) {
    EnchantSet bare;
    EnchantSet fortune;
    addEnchant(fortune, EnchantKind::Fortune, 3);

    int worstPlain = 99;
    int worstFortune = 99;
    for (int i = 0; i < 400; ++i) {
        LootContext plain = contextWith(ItemId::DiamondPickaxe, seedFor(i));
        worstPlain = std::min(worstPlain, rollBlockDrops(Block::DiamondOre, plain)
                                              .countOf(ItemId::DiamondGem));

        LootContext lucky = contextWith(ItemId::DiamondPickaxe, seedFor(i));
        lucky.toolEnchants = fortune;
        worstFortune = std::min(worstFortune, rollBlockDrops(Block::DiamondOre, lucky)
                                                  .countOf(ItemId::DiamondGem));
    }
    CHECK_EQ(worstPlain, 1);
    CHECK_EQ(worstFortune, 1);

    CHECK_EQ(maxOverSeeds(Block::DiamondOre, bare, ItemId::DiamondPickaxe, ItemId::DiamondGem, 400),
             1);
    CHECK_EQ(maxOverSeeds(Block::DiamondOre, fortune, ItemId::DiamondPickaxe, ItemId::DiamondGem,
                          400),
             4);

    // Silk Touch wins over Fortune: the ore block itself, never a multiplied gem.
    LootContext both = enchanted(ItemId::DiamondPickaxe, EnchantKind::SilkTouch, 1, 99u);
    addEnchant(both.toolEnchants, EnchantKind::Fortune, 3);
    LootRoll roll = rollBlockDrops(Block::DiamondOre, both);
    CHECK_EQ(roll.countOf(ItemId::DiamondOre), 1);
    CHECK_EQ(roll.countOf(ItemId::DiamondGem), 0);
}

VF_TEST(grassYieldsDirtAndBedrockYieldsNothingAtAll) {
    LootContext shovel = contextWith(ItemId::IronShovel, 5u);
    LootRoll grass = rollBlockDrops(Block::Grass, shovel);
    CHECK_EQ(grass.countOf(ItemId::Dirt), 1);
    CHECK_EQ(grass.countOf(ItemId::Grass), 0);

    LootContext pickaxe = contextWith(ItemId::DiamondPickaxe, 5u);
    CHECK(rollBlockDrops(Block::Bedrock, pickaxe).empty());
    CHECK(blockLootTable(Block::Bedrock).empty());

    LootContext silk = enchanted(ItemId::DiamondPickaxe, EnchantKind::SilkTouch, 1, 5u);
    CHECK(rollBlockDrops(Block::Bedrock, silk).empty());
}

VF_TEST(leavesOnlyDropTheirSubstituteCuttingWhenSilkTouchIsAbsent) {
    LootContext silk = enchanted(ItemId::IronHoe, EnchantKind::SilkTouch, 1, 3u);
    LootRoll harvested = rollBlockDrops(Block::Leaves, silk);
    CHECK_EQ(harvested.countOf(ItemId::Leaves), 1);
    CHECK_EQ(harvested.totalCount(), 1);

    int cuttings = 0;
    int cuttingsWithFortune = 0;
    int leafBlocks = 0;
    EnchantSet fortune;
    addEnchant(fortune, EnchantKind::Fortune, 3);
    for (int i = 0; i < 1500; ++i) {
        LootContext plain = contextWith(ItemId::IronHoe, seedFor(i));
        LootRoll roll = rollBlockDrops(Block::Leaves, plain);
        cuttings += roll.countOf(ItemId::Wood);
        leafBlocks += roll.countOf(ItemId::Leaves);

        LootContext lucky = contextWith(ItemId::IronHoe, seedFor(i));
        lucky.toolEnchants = fortune;
        cuttingsWithFortune += rollBlockDrops(Block::Leaves, lucky).countOf(ItemId::Wood);
    }
    CHECK_EQ(leafBlocks, 0);
    CHECK(cuttings > 0);
    CHECK(cuttingsWithFortune > cuttings * 2);
}

VF_TEST(toolTierGatesTheOresTheWayTheHarvestLevelsSay) {
    CHECK(!toolQualifiesFor(Block::Stone, ItemId::None));
    CHECK(!toolQualifiesFor(Block::Stone, ItemId::IronAxe));
    CHECK(toolQualifiesFor(Block::Stone, ItemId::WoodPickaxe));

    CHECK(!toolQualifiesFor(Block::IronOre, ItemId::WoodPickaxe));
    CHECK(!toolQualifiesFor(Block::IronOre, ItemId::GoldPickaxe));
    CHECK(toolQualifiesFor(Block::IronOre, ItemId::StonePickaxe));

    CHECK(!toolQualifiesFor(Block::DiamondOre, ItemId::StonePickaxe));
    CHECK(!toolQualifiesFor(Block::DiamondOre, ItemId::GoldPickaxe));
    CHECK(toolQualifiesFor(Block::DiamondOre, ItemId::IronPickaxe));
    CHECK(toolQualifiesFor(Block::EmeraldOre, ItemId::DiamondPickaxe));

    // Nothing outside the pickaxe class is gated at all.
    CHECK(toolQualifiesFor(Block::Dirt, ItemId::None));
    CHECK(toolQualifiesFor(Block::Leaves, ItemId::None));
    CHECK(toolQualifiesFor(Block::Grass, ItemId::WoodShovel));
}

VF_TEST(aBlockBrokenWithTooWeakAToolProducesNoDropsAtAll) {
    LootContext weak = contextWith(ItemId::StonePickaxe, 11u);
    CHECK(rollBlockDrops(Block::DiamondOre, weak).empty());

    LootContext bare = contextWith(ItemId::None, 11u);
    CHECK(rollBlockDrops(Block::Stone, bare).empty());

    // The gate runs before the table, so even Silk Touch cannot rescue it.
    LootContext silkOnAStonePick = enchanted(ItemId::StonePickaxe, EnchantKind::SilkTouch, 1, 11u);
    CHECK(rollBlockDrops(Block::DiamondOre, silkOnAStonePick).empty());
}

VF_TEST(zombiesDropFleshAlwaysAndIronOnlyWhenAPlayerLandedTheKill) {
    int ingotsFromPlayers = 0;
    int ingotsFromTheWorld = 0;
    int fleshTotal = 0;
    for (int i = 0; i < 3000; ++i) {
        LootRoll byPlayer = rollMobDrops(LootTable::MobKind::Zombie, killContext(true, seedFor(i)));
        LootRoll byWorld = rollMobDrops(LootTable::MobKind::Zombie, killContext(false, seedFor(i)));
        ingotsFromPlayers += byPlayer.countOf(ItemId::IronIngot);
        ingotsFromTheWorld += byWorld.countOf(ItemId::IronIngot);
        fleshTotal += byPlayer.countOf(ItemId::RottenFlesh);
    }
    CHECK_EQ(ingotsFromTheWorld, 0);
    CHECK(ingotsFromPlayers > 0);
    CHECK(fleshTotal > 2000);
    CHECK(fleshTotal < 4500);
}

VF_TEST(lootingRaisesBothTheDropCeilingAndTheRareDropRate) {
    int plainCeiling = 0;
    int lootedCeiling = 0;
    int plainIngots = 0;
    int lootedIngots = 0;
    for (int i = 0; i < 3000; ++i) {
        LootContext plain = killContext(true, seedFor(i));
        LootContext looted = killContext(true, seedFor(i));
        addEnchant(looted.toolEnchants, EnchantKind::Looting, 3);

        LootRoll plainRoll = rollMobDrops(LootTable::MobKind::Zombie, plain);
        LootRoll lootedRoll = rollMobDrops(LootTable::MobKind::Zombie, looted);
        plainCeiling = std::max(plainCeiling, plainRoll.countOf(ItemId::RottenFlesh));
        lootedCeiling = std::max(lootedCeiling, lootedRoll.countOf(ItemId::RottenFlesh));
        plainIngots += plainRoll.countOf(ItemId::IronIngot);
        lootedIngots += lootedRoll.countOf(ItemId::IronIngot);
    }
    CHECK_EQ(plainCeiling, 2);
    CHECK_EQ(lootedCeiling, 5);
    CHECK(lootedIngots > plainIngots);
}

VF_TEST(livestockDropRawMeatThatComesOutCookedWhenTheKillWasFiery) {
    const struct { LootTable::MobKind kind; ItemId raw; ItemId cooked; int ceiling; } herd[] = {
        {LootTable::MobKind::Sheep, ItemId::RawMutton, ItemId::CookedMutton, 2},
        {LootTable::MobKind::Cow, ItemId::RawBeef, ItemId::CookedBeef, 3},
        {LootTable::MobKind::Pig, ItemId::RawPorkchop, ItemId::CookedPorkchop, 3},
    };
    for (const auto& animal : herd) {
        int rawSeen = 0;
        int cookedSeen = 0;
        int highest = 0;
        for (int i = 0; i < 300; ++i) {
            LootContext plain = killContext(true, seedFor(i));
            LootRoll rawRoll = rollMobDrops(animal.kind, plain);
            rawSeen += rawRoll.countOf(animal.raw);
            highest = std::max(highest, rawRoll.countOf(animal.raw));
            CHECK_EQ(rawRoll.countOf(animal.cooked), 0);

            LootContext burning = killContext(true, seedFor(i));
            burning.fiery = true;
            LootRoll cookedRoll = rollMobDrops(animal.kind, burning);
            cookedSeen += cookedRoll.countOf(animal.cooked);
            CHECK_EQ(cookedRoll.countOf(animal.raw), 0);
        }
        CHECK_EQ(rawSeen, cookedSeen);
        CHECK_EQ(highest, animal.ceiling);
    }
}

VF_TEST(lootContextForCarriesTheKillOverAndRefusesADeadSeed) {
    EnchantSet enchants;
    addEnchant(enchants, EnchantKind::Looting, 2);

    HurtSource blast = environmentSource(DamageType::Explosion);
    LootContext fromBlast = lootContextFor(blast, ItemId::IronSword, enchants, 0u);
    CHECK_EQ(fromBlast.killedBy, DamageType::Explosion);
    CHECK(!fromBlast.fiery);
    CHECK(!fromBlast.playerKill);
    CHECK_EQ(fromBlast.tool, ItemId::IronSword);
    CHECK_EQ(fromBlast.looting(), 2);
    CHECK(fromBlast.seed != 0u);

    HurtSource lava = environmentSource(DamageType::Lava);
    LootContext fromLava = lootContextFor(lava, ItemId::None, EnchantSet{}, 4242u);
    CHECK(fromLava.fiery);
    CHECK_EQ(fromLava.seed, 4242u);

    HurtSource swing = meleeSource(1, Vec3{}, true);
    LootContext fromSwing = lootContextFor(swing, ItemId::DiamondSword, EnchantSet{}, 9u);
    CHECK(fromSwing.playerKill);
    CHECK(!fromSwing.fiery);
    CHECK_EQ(fromSwing.looting(), 0);
    CHECK_EQ(fromSwing.fortune(), 0);
    CHECK(!fromSwing.silkTouch());
}

VF_TEST(smeltedFormOnlyRewritesTheItemsThatHaveACookedCounterpart) {
    CHECK_EQ(smeltedForm(ItemId::RawPorkchop), ItemId::CookedPorkchop);
    CHECK_EQ(smeltedForm(ItemId::RawBeef), ItemId::CookedBeef);
    CHECK_EQ(smeltedForm(ItemId::RawMutton), ItemId::CookedMutton);
    CHECK_EQ(smeltedForm(ItemId::IronOre), ItemId::IronIngot);
    CHECK_EQ(smeltedForm(ItemId::GoldOre), ItemId::GoldIngot);

    const ItemId unchanged[] = {ItemId::None,       ItemId::Stone,      ItemId::CookedBeef,
                                ItemId::DiamondGem, ItemId::RottenFlesh, ItemId::Bread};
    for (ItemId item : unchanged) CHECK_EQ(smeltedForm(item), item);
}

VF_TEST(chestPoolsStayInsideTheirDeclaredRollAndCountBounds) {
    const struct { LootTable::ChestKind kind; int lowest; int highest; } chests[] = {
        {LootTable::ChestKind::Dungeon, 2, 16},
        {LootTable::ChestKind::Village, 3, 20},
        {LootTable::ChestKind::Temple, 2, 32},
    };
    for (const auto& chest : chests) {
        int seenLowest = 1000;
        int seenHighest = 0;
        for (int i = 0; i < 800; ++i) {
            LootRoll roll = rollChestContents(chest.kind, contextWith(ItemId::None, seedFor(i)));
            seenLowest = std::min(seenLowest, roll.totalCount());
            seenHighest = std::max(seenHighest, roll.totalCount());
        }
        CHECK(seenLowest >= chest.lowest);
        CHECK(seenHighest <= chest.highest);
        CHECK(seenHighest > seenLowest);
    }
}

VF_TEST(rareChestEntriesShowUpFarLessOftenThanCommonOnes) {
    int diamonds = 0;
    int iron = 0;
    for (int i = 0; i < 4000; ++i) {
        LootRoll roll = rollChestContents(LootTable::ChestKind::Dungeon,
                                          contextWith(ItemId::None, seedFor(i)));
        if (roll.countOf(ItemId::DiamondGem) > 0) ++diamonds;
        if (roll.countOf(ItemId::IronIngot) > 0) ++iron;
    }
    CHECK(diamonds > 0);
    CHECK(iron > diamonds * 5);
}

VF_TEST(toolsComeOutOfAChestAsSeparateFullyRepairedStacks) {
    int hoeStacks = 0;
    int badStacks = 0;
    for (int i = 0; i < 600; ++i) {
        LootRoll roll = rollChestContents(LootTable::ChestKind::Village,
                                          contextWith(ItemId::None, seedFor(i)));
        for (const ItemSlot& stack : roll.items) {
            if (stack.item != ItemId::WoodHoe) continue;
            ++hoeStacks;
            if (stack.count != 1) ++badStacks;
            if (stack.durability != toolMaxDurability(ItemId::WoodHoe)) ++badStacks;
        }
    }
    CHECK(hoeStacks > 0);
    CHECK_EQ(badStacks, 0);
}

VF_TEST(stacksOfTheSameMaterialMergeInsteadOfPilingUpAsSeparateEntries) {
    LootPool pool;
    pool.minRolls = 4;
    pool.maxRolls = 4;
    LootEntry ingot;
    ingot.item = ItemId::IronIngot;
    pool.entries.push_back(ingot);

    LootTable table;
    table.pools.push_back(pool);

    LootRoll roll = rollLootTable(table, contextWith(ItemId::None, 77u));
    CHECK_EQ(roll.items.size(), std::size_t{1});
    CHECK_EQ(roll.countOf(ItemId::IronIngot), 4);
    CHECK_EQ(roll.totalCount(), 4);
    CHECK_EQ(roll.countOf(ItemId::GoldIngot), 0);
}

VF_TEST(killConditionsGateEntriesOnHowTheTargetActuallyDied) {
    LootTable table = singlePool({entryOf(ItemId::Gunpowder, LootEntry::Explosive),
                                  entryOf(ItemId::Stick, LootEntry::Ranged)});

    LootContext blasted = contextWith(ItemId::None, 13u);
    blasted.killedBy = DamageType::Explosion;
    LootRoll fromBlast = rollLootTable(table, blasted);
    CHECK_EQ(fromBlast.countOf(ItemId::Gunpowder), 1);
    CHECK_EQ(fromBlast.countOf(ItemId::Stick), 0);

    LootContext shot = contextWith(ItemId::None, 13u);
    shot.killedBy = DamageType::Projectile;
    LootRoll fromArrow = rollLootTable(table, shot);
    CHECK_EQ(fromArrow.countOf(ItemId::Stick), 1);
    CHECK_EQ(fromArrow.countOf(ItemId::Gunpowder), 0);

    LootContext punched = contextWith(ItemId::None, 13u);
    punched.killedBy = DamageType::Melee;
    CHECK(rollLootTable(table, punched).empty());
}

VF_TEST(enchantmentConditionsGateEntriesOnTheToolThatWasUsed) {
    LootTable table = singlePool({entryOf(ItemId::EmeraldGem, LootEntry::NeedsLooting),
                                  entryOf(ItemId::LapisLazuli, LootEntry::NeedsFortune),
                                  entryOf(ItemId::Stick, LootEntry::NotSilkTouched)});

    LootContext bare = contextWith(ItemId::None, 21u);
    LootRoll plain = rollLootTable(table, bare);
    CHECK_EQ(plain.countOf(ItemId::Stick), 1);
    CHECK_EQ(plain.totalCount(), 1);

    LootContext silk = enchanted(ItemId::IronPickaxe, EnchantKind::SilkTouch, 1, 21u);
    CHECK(rollLootTable(table, silk).empty());

    LootContext lucky = enchanted(ItemId::IronPickaxe, EnchantKind::Fortune, 2, 21u);
    LootRoll withFortune = rollLootTable(table, lucky);
    CHECK_EQ(withFortune.totalCount(), 1);
    CHECK(withFortune.countOf(ItemId::LapisLazuli) + withFortune.countOf(ItemId::Stick) == 1);
    CHECK_EQ(withFortune.countOf(ItemId::EmeraldGem), 0);
}

VF_TEST(theSmeltIfFieryFunctionRewritesTheItemAtEmitTime) {
    LootEntry beef;
    beef.item = ItemId::RawBeef;
    beef.minCount = 2;
    beef.maxCount = 2;
    beef.functions = LootEntry::SmeltIfFiery;
    LootTable table = singlePool({beef});

    LootContext cold = contextWith(ItemId::None, 5u);
    LootRoll raw = rollLootTable(table, cold);
    CHECK_EQ(raw.countOf(ItemId::RawBeef), 2);

    LootContext burning = contextWith(ItemId::None, 5u);
    burning.fiery = true;
    LootRoll cooked = rollLootTable(table, burning);
    CHECK_EQ(cooked.countOf(ItemId::CookedBeef), 2);
    CHECK_EQ(cooked.countOf(ItemId::RawBeef), 0);
}

VF_TEST(bonusRollsPerLootingAddWholeExtraPullsToAPool) {
    LootPool pool;
    pool.bonusRollsPerLooting = 2;
    LootEntry gold;
    gold.item = ItemId::GoldIngot;
    pool.entries.push_back(gold);
    LootTable table;
    table.pools.push_back(pool);

    CHECK_EQ(rollLootTable(table, contextWith(ItemId::None, 3u)).countOf(ItemId::GoldIngot), 1);

    LootContext looted = enchanted(ItemId::IronSword, EnchantKind::Looting, 3, 3u);
    CHECK_EQ(rollLootTable(table, looted).countOf(ItemId::GoldIngot), 7);
}

VF_TEST(anEmptyOrOutOfRangeTableRollsToNothingInsteadOfReadingPastItsEnd) {
    LootContext context = contextWith(ItemId::DiamondPickaxe, 1u);
    CHECK(rollLootTable(LootTable{}, context).empty());

    CHECK(blockLootTable(Block::Count).empty());
    CHECK(mobLootTable(LootTable::MobKind::Count).empty());
    CHECK(chestLootTable(LootTable::ChestKind::Count).empty());

    CHECK(rollMobDrops(LootTable::MobKind::Count, context).empty());
    CHECK(rollChestContents(LootTable::ChestKind::Count, context).empty());

    // Air has no item form, so its self-drop table comes out empty rather than
    // dropping a phantom stack.
    CHECK(rollBlockDrops(Block::Air, context).empty());
}

VF_TEST(countOfAndTotalCountAgreeWithTheStacksTheyWereBuiltFrom) {
    LootRoll roll;
    roll.items.push_back({ItemId::IronIngot, 3, 0});
    roll.items.push_back({ItemId::Bread, 2, 0});
    roll.items.push_back({ItemId::IronIngot, 4, 0});

    CHECK_EQ(roll.countOf(ItemId::IronIngot), 7);
    CHECK_EQ(roll.countOf(ItemId::Bread), 2);
    CHECK_EQ(roll.countOf(ItemId::DiamondGem), 0);
    CHECK_EQ(roll.totalCount(), 9);
    CHECK(!roll.empty());
    CHECK(LootRoll{}.empty());
    CHECK_EQ(LootRoll{}.totalCount(), 0);
}
