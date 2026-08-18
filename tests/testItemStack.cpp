#include "testHarness.h"
#include "vf/ItemStack.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

EnchantSet withEnchant(EnchantKind kind, int level) {
    EnchantSet set;
    addEnchant(set, kind, level);
    return set;
}

int hitsUntilBroken(MetaStack stack, uint32_t seed, int limit) {
    uint32_t rng = seed;
    for (int hits = 1; hits <= limit; ++hits) {
        WearResult wear = damageStack(stack, 1, rng);
        if (wear == WearResult::Broke) return hits;
    }
    return -1;
}

} // namespace

VF_TEST(stackLimitFollowsTheItemKindRatherThanAFixedNumber) {
    CHECK_EQ(makeStack(ItemId::Stone, 1).maxCount(), 64);
    CHECK_EQ(makeStack(ItemId::Bread, 1).maxCount(), 64);
    CHECK_EQ(makeTool(ItemId::IronPickaxe).maxCount(), 1);
    CHECK_EQ(stackLimitFor(ItemId::None), 0);

    MetaStack overfilled = makeStack(ItemId::Stone, 200);
    CHECK_EQ(overfilled.count, 64);
    CHECK(overfilled.full());
    CHECK_EQ(overfilled.room(), 0);
}

VF_TEST(aFreshToolCarriesItsFullDurabilityAndNothingElse) {
    MetaStack pickaxe = makeTool(ItemId::DiamondPickaxe);
    CHECK_EQ(pickaxe.count, 1);
    CHECK_EQ(pickaxe.meta.durability.max, toolMaxDurability(ItemId::DiamondPickaxe));
    CHECK_EQ(pickaxe.meta.durability.used, 0);
    CHECK(pickaxe.meta.durability.damageable());
    CHECK(!pickaxe.meta.durability.broken());
    CHECK_NEAR(pickaxe.meta.durability.fraction(), 1.0f, 0.0001f);
    CHECK(pickaxe.meta.pristine());

    MetaStack notATool = makeTool(ItemId::Stone);
    CHECK(notATool.empty());
}

VF_TEST(twoUsedToolsOnlyMergeWhenTheirWearIsIdentical) {
    uint32_t rng = 7u;
    MetaStack fresh = makeTool(ItemId::IronPickaxe);
    MetaStack used = makeTool(ItemId::IronPickaxe);
    damageStack(used, 5, rng);

    CHECK(!stacksMergeable(fresh, used));
    CHECK(stacksMergeable(fresh, makeTool(ItemId::IronPickaxe)));

    MetaStack sameWear = makeTool(ItemId::IronPickaxe);
    sameWear.meta.durability.used = used.meta.durability.used;
    CHECK(stacksMergeable(used, sameWear));

    StackTransfer refused = transferAll(used, fresh);
    CHECK(refused.incompatible);
    CHECK_EQ(refused.moved, 0);
    CHECK_EQ(fresh.count, 1);
    CHECK_EQ(used.count, 1);
}

VF_TEST(anEnchantedSwordNeverMergesWithAPlainOne) {
    MetaStack plain = makeTool(ItemId::DiamondSword);
    MetaStack enchanted = makeEnchantedTool(ItemId::DiamondSword,
                                            withEnchant(EnchantKind::Sharpness, 3));

    CHECK(!stacksMergeable(plain, enchanted));
    CHECK(!stacksIdentical(plain, enchanted));

    MetaStack sameEnchant = makeEnchantedTool(ItemId::DiamondSword,
                                              withEnchant(EnchantKind::Sharpness, 3));
    CHECK(stacksMergeable(enchanted, sameEnchant));

    MetaStack otherLevel = makeEnchantedTool(ItemId::DiamondSword,
                                             withEnchant(EnchantKind::Sharpness, 2));
    CHECK(!stacksMergeable(enchanted, otherLevel));
}

VF_TEST(enchantmentOrderDoesNotChangeStackIdentity) {
    EnchantSet forward;
    addEnchant(forward, EnchantKind::Unbreaking, 2);
    addEnchant(forward, EnchantKind::Efficiency, 4);

    EnchantSet reversed;
    addEnchant(reversed, EnchantKind::Efficiency, 4);
    addEnchant(reversed, EnchantKind::Unbreaking, 2);

    CHECK(sameEnchants(forward, reversed));
    CHECK(stacksMergeable(makeEnchantedTool(ItemId::IronPickaxe, forward),
                          makeEnchantedTool(ItemId::IronPickaxe, reversed)));

    EnchantSet shorter = withEnchant(EnchantKind::Efficiency, 4);
    CHECK(!sameEnchants(forward, shorter));
}

VF_TEST(aCustomNameKeepsAStackOnItsOwn) {
    MetaStack named = makeNamedStack(ItemId::Stone, 10, "Cornerstone");
    MetaStack plain = makeStack(ItemId::Stone, 10);

    CHECK_EQ(named.count, 1);
    CHECK_EQ(named.maxCount(), 1);
    CHECK(!stacksMergeable(named, plain));
    CHECK(stacksMergeable(named, makeNamedStack(ItemId::Stone, 1, "Cornerstone")));
    CHECK(!stacksMergeable(named, makeNamedStack(ItemId::Stone, 1, "Keystone")));
}

VF_TEST(transferMovesOnlyWhatFitsAndLeavesTheRestBehind) {
    MetaStack source = makeStack(ItemId::Cobblestone, 40);
    MetaStack destination = makeStack(ItemId::Cobblestone, 40);

    StackTransfer moved = transferAll(source, destination);
    CHECK_EQ(moved.moved, 24);
    CHECK(moved.destinationFull);
    CHECK(!moved.sourceEmptied);
    CHECK_EQ(destination.count, 64);
    CHECK_EQ(source.count, 16);

    MetaStack elsewhere;
    StackTransfer rest = transferAll(source, elsewhere);
    CHECK_EQ(rest.moved, 16);
    CHECK(rest.sourceEmptied);
    CHECK(source.empty());
    CHECK_EQ(elsewhere.count, 16);
    CHECK(elsewhere.item == ItemId::Cobblestone);
}

VF_TEST(transferRefusesAnIncompatibleDestinationWithoutMovingAnything) {
    MetaStack planks = makeStack(ItemId::Planks, 12);
    MetaStack stone = makeStack(ItemId::Stone, 5);

    StackTransfer blocked = transferAll(planks, stone);
    CHECK(blocked.incompatible);
    CHECK_EQ(blocked.moved, 0);
    CHECK_EQ(planks.count, 12);
    CHECK_EQ(stone.count, 5);
}

VF_TEST(splittingAStackLeavesTheLargerHalfBehind) {
    MetaStack odd = makeStack(ItemId::Sand, 7);
    MetaStack taken = splitHalf(odd);
    CHECK_EQ(taken.count, 4);
    CHECK_EQ(odd.count, 3);

    MetaStack single = makeStack(ItemId::Sand, 1);
    MetaStack lastOne = splitHalf(single);
    CHECK_EQ(lastOne.count, 1);
    CHECK(single.empty());

    MetaStack pile = makeStack(ItemId::Sand, 30);
    MetaStack five = splitOff(pile, 5);
    CHECK_EQ(five.count, 5);
    CHECK_EQ(pile.count, 25);
    CHECK_EQ(splitOff(pile, 0).count, 0);
}

VF_TEST(splittingCarriesTheMetadataOntoBothHalves) {
    MetaStack named = makeStack(ItemId::Stone, 20);
    named.meta.repairCost = 3;
    MetaStack half = splitHalf(named);
    CHECK_EQ(half.meta.repairCost, 3);
    CHECK_EQ(named.meta.repairCost, 3);
    CHECK(stacksMergeable(half, named));
}

VF_TEST(defragmentPacksCompatibleStacksAndFreesTheSlotsBehindThem) {
    std::vector<MetaStack> pouch = {
        makeStack(ItemId::Stone, 40), MetaStack{}, makeStack(ItemId::Stone, 40),
        makeStack(ItemId::Planks, 5), makeStack(ItemId::Stone, 10),
        makeStack(ItemId::Planks, 7),
    };

    int freed = defragmentStacks(pouch);
    CHECK_EQ(totalCountOf(pouch, ItemId::Stone), 90);
    CHECK_EQ(totalCountOf(pouch, ItemId::Planks), 12);
    CHECK(pouch[0].item == ItemId::Stone);
    CHECK_EQ(pouch[0].count, 64);
    CHECK(pouch[1].item == ItemId::Stone);
    CHECK_EQ(pouch[1].count, 26);
    CHECK(pouch[2].item == ItemId::Planks);
    CHECK_EQ(pouch[2].count, 12);
    CHECK(pouch[3].empty());
    CHECK(pouch[5].empty());
    CHECK_EQ(freed, 2);
}

VF_TEST(defragmentKeepsStacksWithDifferentMetadataApart) {
    uint32_t rng = 11u;
    MetaStack worn = makeTool(ItemId::IronAxe);
    damageStack(worn, 3, rng);

    std::vector<MetaStack> pouch = {makeTool(ItemId::IronAxe), worn, makeTool(ItemId::IronAxe)};
    defragmentStacks(pouch);

    CHECK_EQ(totalCountOf(pouch, ItemId::IronAxe), 3);
    CHECK_EQ(pouch[0].count, 1);
    CHECK_EQ(pouch[1].count, 1);
    CHECK_EQ(pouch[2].count, 1);
}

VF_TEST(sortingIsStableForStacksThatCompareEqual) {
    std::vector<MetaStack> pouch = {
        makeStack(ItemId::Planks, 10), MetaStack{}, makeStack(ItemId::Stone, 10),
        makeStack(ItemId::Planks, 10), makeStack(ItemId::Stone, 64),
    };
    pouch[0].meta.repairCost = 1;
    pouch[3].meta.repairCost = 2;

    sortStacks(pouch);

    CHECK(pouch[0].item == ItemId::Stone);
    CHECK_EQ(pouch[0].count, 64);
    CHECK(pouch[1].item == ItemId::Stone);
    CHECK(pouch[2].item == ItemId::Planks);
    CHECK_EQ(pouch[2].meta.repairCost, 1);
    CHECK(pouch[3].item == ItemId::Planks);
    CHECK_EQ(pouch[3].meta.repairCost, 2);
    CHECK(pouch[4].empty());
}

VF_TEST(nbtRoundTripKeepsDurabilityEnchantmentsAndTheCustomName) {
    EnchantSet enchants;
    addEnchant(enchants, EnchantKind::Sharpness, 4);
    addEnchant(enchants, EnchantKind::Unbreaking, 3);
    addEnchant(enchants, EnchantKind::Mending, 1);

    MetaStack sword = makeEnchantedTool(ItemId::DiamondSword, enchants);
    sword.meta.customName = "Vulpine Edge";
    sword.meta.durability.used = 137;
    sword.meta.repairCost = 9;

    MetaStack restored;
    CHECK(stackFromNbt(stackToNbt(sword), restored));

    CHECK(restored.item == ItemId::DiamondSword);
    CHECK_EQ(restored.count, 1);
    CHECK_EQ(restored.meta.durability.used, 137);
    CHECK_EQ(restored.meta.durability.max, sword.meta.durability.max);
    CHECK_EQ(restored.meta.repairCost, 9);
    CHECK(restored.meta.customName == std::string("Vulpine Edge"));
    CHECK_EQ(restored.meta.enchants.levelOf(EnchantKind::Sharpness), 4);
    CHECK_EQ(restored.meta.enchants.levelOf(EnchantKind::Unbreaking), 3);
    CHECK_EQ(restored.meta.enchants.levelOf(EnchantKind::Mending), 1);
    CHECK(stacksIdentical(sword, restored));
}

VF_TEST(nbtRoundTripSurvivesTheBinaryEncoderAndKeepsEmptySlotsEmpty) {
    std::vector<MetaStack> pouch = {
        makeStack(ItemId::Stone, 64),
        MetaStack{},
        makeNamedStack(ItemId::GoldSword, 1, "Bribe"),
        makeStack(ItemId::Apple, 3),
    };
    pouch[2].meta.unbreakable = true;

    NbtTag root = NbtTag::makeCompound();
    root.put("Pouch", stackListToNbt(pouch));

    std::vector<uint8_t> bytes = serializeNbt("Test", root);
    std::string name;
    NbtTag decoded;
    CHECK(deserializeNbt(bytes.data(), bytes.size(), name, decoded));

    const NbtTag* list = decoded.find("Pouch");
    CHECK(list != nullptr);
    if (list == nullptr) return;

    std::vector<MetaStack> restored;
    CHECK(stackListFromNbt(*list, restored));
    CHECK_EQ(restored.size(), pouch.size());
    for (std::size_t i = 0; i < pouch.size(); ++i) {
        CHECK(stacksIdentical(pouch[i], restored[i]));
    }
    CHECK(restored[1].empty());
    CHECK(restored[2].meta.unbreakable);
}

VF_TEST(nbtRejectsAStackTagThatNamesAnItemOutsideTheTable) {
    NbtTag broken = NbtTag::makeCompound();
    broken.put("Id", NbtTag::makeShort(static_cast<int16_t>(static_cast<int>(ItemId::Count) + 40)));
    broken.put("Count", NbtTag::makeShort(1));

    MetaStack out = makeStack(ItemId::Stone, 5);
    CHECK(!stackFromNbt(broken, out));
    CHECK(out.empty());

    NbtTag notACompound = NbtTag::makeInt(4);
    CHECK(!stackFromNbt(notACompound, out));
}

VF_TEST(aToolBreaksOnExactlyTheHitThatReachesZeroDurability) {
    uint32_t rng = 5u;
    MetaStack pickaxe = makeTool(ItemId::WoodPickaxe);
    int max = pickaxe.meta.durability.max;
    CHECK(max > 1);

    for (int hit = 1; hit < max; ++hit) {
        WearResult wear = damageStack(pickaxe, 1, rng);
        CHECK(wear == WearResult::Damaged);
    }
    CHECK_EQ(pickaxe.meta.durability.remaining(), 1);
    CHECK(!pickaxe.empty());

    WearResult final = damageStack(pickaxe, 1, rng);
    CHECK(final == WearResult::Broke);
    CHECK(pickaxe.empty());
    CHECK(damageStack(pickaxe, 1, rng) == WearResult::NotDamageable);
}

