#include "vf/Item.h"

#include <algorithm>
#include <array>

namespace vf {

namespace {

float tierSpeed(ToolTier tier) {
    switch (tier) {
        case ToolTier::None:    return 1.0f;
        case ToolTier::Wood:    return 2.0f;
        case ToolTier::Stone:   return 4.0f;
        case ToolTier::Iron:    return 6.0f;
        case ToolTier::Diamond: return 8.0f;
        case ToolTier::Gold:    return 12.0f;
    }
    return 1.0f;
}

int tierDurability(ToolTier tier) {
    switch (tier) {
        case ToolTier::None:    return 0;
        case ToolTier::Wood:    return 60;
        case ToolTier::Stone:   return 130;
        case ToolTier::Iron:    return 250;
        case ToolTier::Gold:    return 35;
        case ToolTier::Diamond: return 1560;
    }
    return 0;
}

ItemInfo blockEntry() { return ItemInfo{ItemKind::Block, defaultStackSize, ToolType::None, ToolTier::None, 0, 0.0f}; }
ItemInfo materialEntry() { return ItemInfo{ItemKind::Material, defaultStackSize, ToolType::None, ToolTier::None, 0, 0.0f}; }
ItemInfo toolEntry(ToolType type, ToolTier tier) { return ItemInfo{ItemKind::Tool, 1, type, tier, 0, 0.0f}; }
ItemInfo foodEntry(int hunger, float saturation) {
    return ItemInfo{ItemKind::Food, defaultStackSize, ToolType::None, ToolTier::None, hunger, saturation};
}

std::array<ItemInfo, static_cast<std::size_t>(ItemId::Count)> buildItemTable() {
    std::array<ItemInfo, static_cast<std::size_t>(ItemId::Count)> table{};
    auto set = [&](ItemId id, ItemInfo info) { table[static_cast<std::size_t>(id)] = info; };

    const ItemId blockItems[] = {
        ItemId::Grass, ItemId::Dirt, ItemId::Stone, ItemId::Sand, ItemId::Wood, ItemId::Leaves,
        ItemId::Snow, ItemId::CoalOre, ItemId::IronOre, ItemId::GoldOre, ItemId::Gravel,
        ItemId::Glowstone, ItemId::Cobblestone, ItemId::Planks, ItemId::Sandstone, ItemId::Ice,
        ItemId::Obsidian, ItemId::DiamondOre, ItemId::RedstoneOre, ItemId::EmeraldOre,
        ItemId::LapisOre, ItemId::CraftingTable, ItemId::Furnace, ItemId::Chest, ItemId::Torch,
        ItemId::RedstoneWire, ItemId::RedstoneTorch, ItemId::RedstoneLamp, ItemId::Lever,
        ItemId::Button, ItemId::RedstoneBlock, ItemId::Repeater, ItemId::Comparator,
        ItemId::Piston, ItemId::PistonSticky, ItemId::Tnt, ItemId::Ladder,
    };
    for (ItemId id : blockItems) set(id, blockEntry());

    const ItemId materialItems[] = {
        ItemId::Stick, ItemId::Gunpowder, ItemId::CoalLump, ItemId::IronIngot, ItemId::GoldIngot,
        ItemId::DiamondGem, ItemId::EmeraldGem, ItemId::RedstoneDust, ItemId::LapisLazuli,
    };
    for (ItemId id : materialItems) set(id, materialEntry());

    struct ToolRow { ToolTier tier; ItemId pickaxe, axe, shovel, sword, hoe; };
    const ToolRow toolRows[] = {
        {ToolTier::Wood, ItemId::WoodPickaxe, ItemId::WoodAxe, ItemId::WoodShovel, ItemId::WoodSword, ItemId::WoodHoe},
        {ToolTier::Stone, ItemId::StonePickaxe, ItemId::StoneAxe, ItemId::StoneShovel, ItemId::StoneSword, ItemId::StoneHoe},
        {ToolTier::Iron, ItemId::IronPickaxe, ItemId::IronAxe, ItemId::IronShovel, ItemId::IronSword, ItemId::IronHoe},
        {ToolTier::Gold, ItemId::GoldPickaxe, ItemId::GoldAxe, ItemId::GoldShovel, ItemId::GoldSword, ItemId::GoldHoe},
        {ToolTier::Diamond, ItemId::DiamondPickaxe, ItemId::DiamondAxe, ItemId::DiamondShovel, ItemId::DiamondSword, ItemId::DiamondHoe},
    };
    for (const ToolRow& row : toolRows) {
        set(row.pickaxe, toolEntry(ToolType::Pickaxe, row.tier));
        set(row.axe, toolEntry(ToolType::Axe, row.tier));
        set(row.shovel, toolEntry(ToolType::Shovel, row.tier));
        set(row.sword, toolEntry(ToolType::Sword, row.tier));
        set(row.hoe, toolEntry(ToolType::Hoe, row.tier));
    }

    set(ItemId::Apple, foodEntry(4, 2.4f));
    set(ItemId::Bread, foodEntry(5, 6.0f));
    set(ItemId::RawPorkchop, foodEntry(3, 1.8f));
    set(ItemId::CookedPorkchop, foodEntry(8, 6.4f));
    set(ItemId::RawBeef, foodEntry(3, 1.8f));
    set(ItemId::CookedBeef, foodEntry(8, 6.4f));
    set(ItemId::RawMutton, foodEntry(2, 1.2f));
    set(ItemId::CookedMutton, foodEntry(6, 4.8f));
    set(ItemId::RottenFlesh, foodEntry(2, 0.4f));

    return table;
}

struct BlockItemPair { Block block; ItemId item; };

const std::array<BlockItemPair, 37>& blockItemPairs() {
    static const std::array<BlockItemPair, 37> pairs = {{
        {Block::Grass, ItemId::Grass}, {Block::Dirt, ItemId::Dirt}, {Block::Stone, ItemId::Stone},
        {Block::Sand, ItemId::Sand}, {Block::Wood, ItemId::Wood}, {Block::Leaves, ItemId::Leaves},
        {Block::Snow, ItemId::Snow}, {Block::CoalOre, ItemId::CoalOre}, {Block::IronOre, ItemId::IronOre},
        {Block::GoldOre, ItemId::GoldOre}, {Block::Gravel, ItemId::Gravel},
        {Block::Glowstone, ItemId::Glowstone}, {Block::Cobblestone, ItemId::Cobblestone},
        {Block::Planks, ItemId::Planks}, {Block::Sandstone, ItemId::Sandstone}, {Block::Ice, ItemId::Ice},
        {Block::Obsidian, ItemId::Obsidian}, {Block::DiamondOre, ItemId::DiamondOre},
        {Block::RedstoneOre, ItemId::RedstoneOre}, {Block::EmeraldOre, ItemId::EmeraldOre},
        {Block::LapisOre, ItemId::LapisOre}, {Block::CraftingTable, ItemId::CraftingTable},
        {Block::Furnace, ItemId::Furnace}, {Block::Chest, ItemId::Chest}, {Block::Torch, ItemId::Torch},
        {Block::RedstoneWire, ItemId::RedstoneWire}, {Block::RedstoneTorch, ItemId::RedstoneTorch},
        {Block::RedstoneLamp, ItemId::RedstoneLamp}, {Block::Lever, ItemId::Lever},
        {Block::Button, ItemId::Button}, {Block::RedstoneBlock, ItemId::RedstoneBlock},
        {Block::Repeater, ItemId::Repeater}, {Block::Comparator, ItemId::Comparator},
        {Block::Piston, ItemId::Piston}, {Block::PistonSticky, ItemId::PistonSticky},
        {Block::Tnt, ItemId::Tnt}, {Block::Ladder, ItemId::Ladder},
    }};
    return pairs;
}

} // namespace

const ItemInfo& itemInfo(ItemId id) {
    static const std::array<ItemInfo, static_cast<std::size_t>(ItemId::Count)> table = buildItemTable();
    return table[static_cast<std::size_t>(id)];
}

bool isBlockItem(ItemId id) { return itemInfo(id).kind == ItemKind::Block; }
bool isTool(ItemId id) { return itemInfo(id).kind == ItemKind::Tool; }
bool isFood(ItemId id) { return itemInfo(id).kind == ItemKind::Food; }

int maxStackSize(ItemId id) { return itemInfo(id).maxStack; }
int toolMaxDurability(ItemId id) { return tierDurability(itemInfo(id).toolTier); }

Block blockForItem(ItemId id) {
    for (const BlockItemPair& pair : blockItemPairs())
        if (pair.item == id) return pair.block;
    return Block::Air;
}

ItemId itemForBlock(Block block) {
    for (const BlockItemPair& pair : blockItemPairs())
        if (pair.block == block) return pair.item;
    return ItemId::None;
}

ToolType correctToolFor(Block block) {
    switch (block) {
        case Block::Stone:
        case Block::Cobblestone:
        case Block::Sandstone:
        case Block::CoalOre:
        case Block::IronOre:
        case Block::GoldOre:
        case Block::DiamondOre:
        case Block::RedstoneOre:
        case Block::EmeraldOre:
        case Block::LapisOre:
        case Block::Obsidian:
        case Block::Furnace:
        case Block::FurnaceLit:
        case Block::RedstoneBlock:
        case Block::Piston:
        case Block::PistonSticky:
        case Block::PistonHead:
            return ToolType::Pickaxe;
        case Block::Wood:
        case Block::Planks:
        case Block::CraftingTable:
        case Block::Chest:
        case Block::Ladder:
            return ToolType::Axe;
        case Block::Dirt:
        case Block::Grass:
        case Block::Sand:
        case Block::Gravel:
        case Block::Snow:
            return ToolType::Shovel;
        case Block::Leaves:
            return ToolType::Hoe;
        default:
            return ToolType::None;
    }
}

float toolTierSpeedMultiplier(ToolTier tier) { return tierSpeed(tier); }

float miningSpeedMultiplier(ItemId toolItem, Block block) {
    if (!isTool(toolItem)) return 1.0f;
    const ItemInfo& info = itemInfo(toolItem);
    if (info.toolType != correctToolFor(block)) return 1.0f;
    return tierSpeed(info.toolTier);
}

FoodValue foodValue(ItemId id) {
    const ItemInfo& info = itemInfo(id);
    return {info.foodHunger, info.foodSaturation};
}

ItemSlot makeToolStack(ItemId toolItem) {
    return ItemSlot{toolItem, 1, toolMaxDurability(toolItem)};
}

int mergeStacks(ItemSlot& into, ItemSlot& from) {
    if (from.empty()) return 0;
    if (into.empty()) {
        into = from;
        from = ItemSlot{};
        return into.count;
    }
    if (into.item != from.item || into.durability != from.durability) return 0;

    int room = maxStackSize(into.item) - into.count;
    if (room <= 0) return 0;
    int moved = std::min(room, from.count);
    into.count += moved;
    from.count -= moved;
    if (from.count <= 0) from = ItemSlot{};
    return moved;
}

ItemSlot splitStack(ItemSlot& stack, int amount) {
    if (stack.empty() || amount <= 0) return ItemSlot{};
    amount = std::min(amount, stack.count);
    ItemSlot piece{stack.item, amount, stack.durability};
    stack.count -= amount;
    if (stack.count <= 0) stack = ItemSlot{};
    return piece;
}

bool damageTool(ItemSlot& stack, int amount) {
    if (stack.empty() || !isTool(stack.item)) return false;
    stack.durability -= amount;
    if (stack.durability <= 0) {
        stack = ItemSlot{};
        return true;
    }
    return false;
}

} // namespace vf
