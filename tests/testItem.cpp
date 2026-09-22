#include "testHarness.h"
#include "vf/Item.h"

using namespace vf;

VF_TEST(blockItemRoundTripsBothWays) {
    CHECK(itemForBlock(Block::Stone) == ItemId::Stone);
    CHECK(blockForItem(ItemId::Stone) == Block::Stone);
    CHECK(itemForBlock(Block::CraftingTable) == ItemId::CraftingTable);
    CHECK(blockForItem(ItemId::CraftingTable) == Block::CraftingTable);
    CHECK(itemForBlock(Block::Tnt) == ItemId::Tnt);
    CHECK(blockForItem(ItemId::Tnt) == Block::Tnt);
}

VF_TEST(blockItemMappingHasNoCounterpartForUnobtainableBlocks) {
    CHECK(itemForBlock(Block::Bedrock) == ItemId::None);
    CHECK(itemForBlock(Block::Air) == ItemId::None);
    CHECK(itemForBlock(Block::Water) == ItemId::None);
    CHECK(blockForItem(ItemId::IronIngot) == Block::Air);
    CHECK(blockForItem(ItemId::Stick) == Block::Air);
    CHECK(blockForItem(ItemId::WoodPickaxe) == Block::Air);
}

VF_TEST(itemKindsAndStackSizesAreCorrect) {
    CHECK(isBlockItem(ItemId::Stone));
    CHECK(!isBlockItem(ItemId::WoodPickaxe));
    CHECK(isTool(ItemId::DiamondSword));
    CHECK(!isTool(ItemId::Stone));
    CHECK(isFood(ItemId::Bread));
    CHECK(!isFood(ItemId::Stick));

    CHECK_EQ(maxStackSize(ItemId::Stone), 64);
    CHECK_EQ(maxStackSize(ItemId::WoodPickaxe), 1);
    CHECK_EQ(maxStackSize(ItemId::Bread), 64);
}

VF_TEST(toolDurabilityRisesWithTierExceptGold) {
    int wood = toolMaxDurability(ItemId::WoodPickaxe);
    int stone = toolMaxDurability(ItemId::StonePickaxe);
    int iron = toolMaxDurability(ItemId::IronPickaxe);
    int gold = toolMaxDurability(ItemId::GoldPickaxe);
    int diamond = toolMaxDurability(ItemId::DiamondPickaxe);

    CHECK(wood > 0);
    CHECK(stone > wood);
    CHECK(iron > stone);
    CHECK(diamond > iron);
    CHECK(gold < wood);
    CHECK_EQ(toolMaxDurability(ItemId::Stone), 0);
}

VF_TEST(correctToolMatchesExpectedBlockClasses) {
    CHECK(correctToolFor(Block::Stone) == ToolType::Pickaxe);
    CHECK(correctToolFor(Block::Cobblestone) == ToolType::Pickaxe);
    CHECK(correctToolFor(Block::Wood) == ToolType::Axe);
    CHECK(correctToolFor(Block::Planks) == ToolType::Axe);
    CHECK(correctToolFor(Block::Dirt) == ToolType::Shovel);
    CHECK(correctToolFor(Block::Sand) == ToolType::Shovel);
    CHECK(correctToolFor(Block::Leaves) == ToolType::Hoe);
    CHECK(correctToolFor(Block::Torch) == ToolType::None);
}

VF_TEST(miningSpeedMultiplierRewardsMatchingToolAndTier) {
    CHECK_NEAR(miningSpeedMultiplier(ItemId::None, Block::Stone), 1.0f, 1e-6f);
    CHECK_NEAR(miningSpeedMultiplier(ItemId::WoodPickaxe, Block::Stone), 2.0f, 1e-6f);
    CHECK_NEAR(miningSpeedMultiplier(ItemId::DiamondPickaxe, Block::Stone), 8.0f, 1e-6f);
    CHECK_NEAR(miningSpeedMultiplier(ItemId::StonePickaxe, Block::Wood), 1.0f, 1e-6f);
    CHECK_NEAR(miningSpeedMultiplier(ItemId::StoneAxe, Block::Wood), 4.0f, 1e-6f);
}

VF_TEST(damageToolBreaksAtZeroDurability) {
    ItemSlot stack = makeToolStack(ItemId::WoodPickaxe);
    int durability = stack.durability;
    CHECK(durability > 0);

    bool broke = false;
    for (int i = 0; i < durability - 1; ++i) {
        broke = damageTool(stack);
        CHECK(!broke);
        CHECK(!stack.empty());
    }
    broke = damageTool(stack);
    CHECK(broke);
    CHECK(stack.empty());
    CHECK(stack.item == ItemId::None);
}

VF_TEST(damageToolIsNoOpOnNonTools) {
    ItemSlot stack{ItemId::Stone, 10, 0};
    bool broke = damageTool(stack);
    CHECK(!broke);
    CHECK_EQ(stack.count, 10);
}

VF_TEST(mergeStacksFillsUpToLimitAndLeavesRemainder) {
    ItemSlot into{ItemId::Stone, 60, 0};
    ItemSlot from{ItemId::Stone, 20, 0};
    int moved = mergeStacks(into, from);
    CHECK_EQ(moved, 4);
    CHECK_EQ(into.count, 64);
    CHECK_EQ(from.count, 16);
}

VF_TEST(mergeStacksIntoEmptyMovesEverything) {
    ItemSlot into{};
    ItemSlot from{ItemId::Cobblestone, 30, 0};
    int moved = mergeStacks(into, from);
    CHECK_EQ(moved, 30);
    CHECK(into.item == ItemId::Cobblestone);
    CHECK_EQ(into.count, 30);
    CHECK(from.empty());
}

VF_TEST(mergeStacksRefusesDifferentItems) {
    ItemSlot into{ItemId::Stone, 5, 0};
    ItemSlot from{ItemId::Dirt, 5, 0};
    int moved = mergeStacks(into, from);
    CHECK_EQ(moved, 0);
    CHECK_EQ(into.count, 5);
    CHECK_EQ(from.count, 5);
}

VF_TEST(splitStackDividesAndEmptiesFully) {
    ItemSlot stack{ItemId::Planks, 10, 0};
    ItemSlot half = splitStack(stack, 4);
    CHECK_EQ(half.count, 4);
    CHECK(half.item == ItemId::Planks);
    CHECK_EQ(stack.count, 6);

    ItemSlot rest = splitStack(stack, 100);
    CHECK_EQ(rest.count, 6);
    CHECK(stack.empty());
}

VF_TEST(foodValueRewardsCookedOverRaw) {
    FoodValue raw = foodValue(ItemId::RawPorkchop);
    FoodValue cooked = foodValue(ItemId::CookedPorkchop);
    CHECK(cooked.hunger > raw.hunger);
    CHECK(cooked.saturation > raw.saturation);
    FoodValue notFood = foodValue(ItemId::Stone);
    CHECK_EQ(notFood.hunger, 0);
}
