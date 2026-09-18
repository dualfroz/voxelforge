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

class CraftingRegistry {
public:
    CraftingRegistry();

    void addShaped(const ShapedRecipe& recipe);
    void addShapeless(const ShapelessRecipe& recipe);
    void addSmelting(const SmeltingRecipe& recipe);

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

float fuelBurnSeconds(ItemId item);
bool isFuel(ItemId item);

enum class FurnaceSlotKind : uint8_t { Input, Fuel, Output };

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
    float smeltProgress = 0.0f;

    float smeltFraction() const;

private:
    const CraftingRegistry* registry;
};

}
