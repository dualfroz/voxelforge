#pragma once

#include <cstdint>
#include "Block.h"

namespace vf {

enum class ItemId : uint16_t {
    None = 0,

    Grass, Dirt, Stone, Sand, Wood, Leaves, Snow,
    CoalOre, IronOre, GoldOre, Gravel, Glowstone,
    Cobblestone, Planks, Sandstone, Ice, Obsidian,
    DiamondOre, RedstoneOre, EmeraldOre, LapisOre,
    CraftingTable, Furnace, Chest, Torch,
    RedstoneWire, RedstoneTorch, RedstoneLamp,
    Lever, Button, RedstoneBlock, Repeater, Comparator,
    Piston, PistonSticky, Tnt, Ladder,

    Stick, Gunpowder,
    CoalLump, IronIngot, GoldIngot, DiamondGem, EmeraldGem, RedstoneDust, LapisLazuli,

    WoodPickaxe, WoodAxe, WoodShovel, WoodSword, WoodHoe,
    StonePickaxe, StoneAxe, StoneShovel, StoneSword, StoneHoe,
    IronPickaxe, IronAxe, IronShovel, IronSword, IronHoe,
    GoldPickaxe, GoldAxe, GoldShovel, GoldSword, GoldHoe,
    DiamondPickaxe, DiamondAxe, DiamondShovel, DiamondSword, DiamondHoe,

    Apple, Bread,
    RawPorkchop, CookedPorkchop,
    RawBeef, CookedBeef,
    RawMutton, CookedMutton,
    RottenFlesh,

    Count
};

enum class ItemKind : uint8_t { Block, Material, Tool, Food };
enum class ToolType : uint8_t { None, Pickaxe, Axe, Shovel, Sword, Hoe };
enum class ToolTier : uint8_t { None, Wood, Stone, Iron, Gold, Diamond };

constexpr int defaultStackSize = 64;

struct ItemInfo {
    ItemKind kind = ItemKind::Material;
    int maxStack = defaultStackSize;
    ToolType toolType = ToolType::None;
    ToolTier toolTier = ToolTier::None;
    int foodHunger = 0;
    float foodSaturation = 0.0f;
};

const ItemInfo& itemInfo(ItemId id);

bool isBlockItem(ItemId id);
bool isTool(ItemId id);
bool isFood(ItemId id);

int maxStackSize(ItemId id);
int toolMaxDurability(ItemId id);

Block blockForItem(ItemId id);
ItemId itemForBlock(Block block);

ToolType correctToolFor(Block block);

float toolTierSpeedMultiplier(ToolTier tier);

float miningSpeedMultiplier(ItemId toolItem, Block block);

struct FoodValue {
    int hunger = 0;
    float saturation = 0.0f;
};

FoodValue foodValue(ItemId id);

struct ItemSlot {
    ItemId item = ItemId::None;
    int count = 0;
    int durability = 0;

    bool empty() const { return count <= 0 || item == ItemId::None; }
};

ItemSlot makeToolStack(ItemId toolItem);

int mergeStacks(ItemSlot& into, ItemSlot& from);

ItemSlot splitStack(ItemSlot& stack, int amount);

bool damageTool(ItemSlot& stack, int amount = 1);

}
