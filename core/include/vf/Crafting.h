#pragma once

#include <array>
#include <vector>
#include "Item.h"

namespace vf {

constexpr int craftGridWidth = 3;
constexpr int craftGridHeight = 3;
constexpr int craftGridCells = craftGridWidth * craftGridHeight;

using CraftGrid = std::array<ItemId, craftGridCells>;

struct CraftResult {
    ItemId item = ItemId::None;
    int count = 0;

    bool valid() const { return item != ItemId::None && count > 0; }
};

struct ShapedRecipe {
    int width = 0;
    int height = 0;
    std::array<ItemId, craftGridCells> pattern{};
    CraftResult result;
    bool allowMirror = true;
};

struct ShapelessRecipe {
    std::vector<ItemId> ingredients;
    CraftResult result;
};

struct SmeltingRecipe {
    ItemId input = ItemId::None;
    CraftResult result;
    float smeltSeconds = 10.0f;
};

// Registry of shaped, shapeless and smelting recipes. Constructing one loads the
// default recipe set (planks, sticks, crafting table, furnace, chest, the five
// tool types across all five tiers, torch, tnt and food/ore smelting).
class CraftingRegistry {
public:
    CraftingRegistry();

    void addShaped(const ShapedRecipe& recipe);
    void addShapeless(const ShapelessRecipe& recipe);
    void addSmelting(const SmeltingRecipe& recipe);

    // Matches a 3x3 crafting grid against shaped recipes first (position and,
    // unless disabled, horizontal mirror), then shapeless recipes (ingredient
    // multiset, position independent). ItemId::None entries are empty cells.
    CraftResult craftMatch(const CraftGrid& grid) const;

    const SmeltingRecipe* smeltingFor(ItemId input) const;

    const std::vector<ShapedRecipe>& shapedRecipes() const { return shaped; }
    const std::vector<ShapelessRecipe>& shapelessRecipes() const { return shapeless; }
    const std::vector<SmeltingRecipe>& smeltingRecipes() const { return smelting; }

private:
    void registerDefaults();

    std::vector<ShapedRecipe> shaped;
    std::vector<ShapelessRecipe> shapeless;
    std::vector<SmeltingRecipe> smelting;
};

// Seconds a unit of fuel keeps a furnace burning; 0 when the item is not fuel.
float fuelBurnSeconds(ItemId item);
bool isFuel(ItemId item);

enum class FurnaceSlotKind : uint8_t { Input, Fuel, Output };

// State machine for a single furnace: input/fuel/output slots, a burning fuel
// budget and smelt progress. Call tick() once per fixed step; it consumes fuel
// on demand, advances the current smelt and moves finished output.
class Furnace {
public:
    explicit Furnace(const CraftingRegistry& registry);

    ItemSlot input;
    ItemSlot fuel;
    ItemSlot output;

    void tick(float dt);

    bool isLit() const { return fuelTimeRemaining > 0.0f; }
    float fuelTimeRemaining = 0.0f;
    float fuelTimeTotal = 0.0f;
    float smeltProgress = 0.0f;   // seconds accumulated on the current item

    float smeltFraction() const;

private:
    const CraftingRegistry* registry;
};

} // namespace vf
