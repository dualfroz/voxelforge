#include "vf/Crafting.h"

#include <algorithm>

namespace vf {

namespace {

bool matchesShaped(const CraftGrid& grid, const ShapedRecipe& recipe, int minRow, int minCol,
                   bool mirror) {
    for (int r = 0; r < recipe.height; ++r) {
        for (int c = 0; c < recipe.width; ++c) {
            int patternCol = mirror ? recipe.width - 1 - c : c;
            ItemId want = recipe.pattern[r * recipe.width + patternCol];
            ItemId got = grid[(minRow + r) * craftGridWidth + (minCol + c)];
            if (want != got) return false;
        }
    }
    return true;
}

ShapedRecipe shaped1x2(ItemId top, ItemId bottom, ItemId result, int count) {
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 2;
    recipe.pattern = {top, bottom};
    recipe.result = {result, count};
    recipe.allowMirror = false;
    return recipe;
}

ShapedRecipe shaped1x3(ItemId top, ItemId mid, ItemId bottom, ItemId result, int count) {
    ShapedRecipe recipe;
    recipe.width = 1;
    recipe.height = 3;
    recipe.pattern = {top, mid, bottom};
    recipe.result = {result, count};
    recipe.allowMirror = false;
    return recipe;
}

ShapedRecipe shaped2x2(ItemId r0c0, ItemId r0c1, ItemId r1c0, ItemId r1c1, ItemId result, int count) {
    ShapedRecipe recipe;
    recipe.width = 2;
    recipe.height = 2;
    recipe.pattern = {r0c0, r0c1, r1c0, r1c1};
    recipe.result = {result, count};
    recipe.allowMirror = true;
    return recipe;
}

ShapedRecipe shaped2x3(ItemId r0c0, ItemId r0c1, ItemId r1c0, ItemId r1c1, ItemId r2c0, ItemId r2c1,
                       ItemId result, int count) {
    ShapedRecipe recipe;
    recipe.width = 2;
    recipe.height = 3;
    recipe.pattern = {r0c0, r0c1, r1c0, r1c1, r2c0, r2c1};
    recipe.result = {result, count};
    recipe.allowMirror = true;
    return recipe;
}

ShapedRecipe shaped3x3(const std::array<ItemId, craftGridCells>& pattern, ItemId result, int count,
                       bool allowMirror = true) {
    ShapedRecipe recipe;
    recipe.width = 3;
    recipe.height = 3;
    recipe.pattern = pattern;
    recipe.result = {result, count};
    recipe.allowMirror = allowMirror;
    return recipe;
}

struct ToolMaterial {
    ToolTier tier;
    ItemId material;
};

} // namespace

CraftingRegistry::CraftingRegistry() { registerDefaults(); }

void CraftingRegistry::addShaped(const ShapedRecipe& recipe) { shaped.push_back(recipe); }
void CraftingRegistry::addShapeless(const ShapelessRecipe& recipe) { shapeless.push_back(recipe); }
void CraftingRegistry::addSmelting(const SmeltingRecipe& recipe) { smelting.push_back(recipe); }

void CraftingRegistry::registerDefaults() {
    const ItemId x = ItemId::None;

    addShapeless({{ItemId::Wood}, {ItemId::Planks, 4}});
    addShaped(shaped1x2(ItemId::Planks, ItemId::Planks, ItemId::Stick, 4));
    addShaped(shaped2x2(ItemId::Planks, ItemId::Planks, ItemId::Planks, ItemId::Planks,
                        ItemId::CraftingTable, 1));
    addShaped(shaped3x3({ItemId::Cobblestone, ItemId::Cobblestone, ItemId::Cobblestone,
                         ItemId::Cobblestone, x, ItemId::Cobblestone,
                         ItemId::Cobblestone, ItemId::Cobblestone, ItemId::Cobblestone},
                        ItemId::Furnace, 1, false));
    addShaped(shaped3x3({ItemId::Planks, ItemId::Planks, ItemId::Planks,
                         ItemId::Planks, x, ItemId::Planks,
                         ItemId::Planks, ItemId::Planks, ItemId::Planks},
                        ItemId::Chest, 1, false));
    addShaped(shaped1x2(ItemId::CoalLump, ItemId::Stick, ItemId::Torch, 4));
    addShaped(shaped3x3({ItemId::Sand, ItemId::Gunpowder, ItemId::Sand,
                         ItemId::Gunpowder, ItemId::Sand, ItemId::Gunpowder,
                         ItemId::Sand, ItemId::Gunpowder, ItemId::Sand},
                        ItemId::Tnt, 1, false));

    const ToolMaterial tiers[] = {
        {ToolTier::Wood, ItemId::Planks},
        {ToolTier::Stone, ItemId::Cobblestone},
        {ToolTier::Iron, ItemId::IronIngot},
        {ToolTier::Gold, ItemId::GoldIngot},
        {ToolTier::Diamond, ItemId::DiamondGem},
    };

    for (const ToolMaterial& row : tiers) {
        ItemId m = row.material;
        ItemId s = ItemId::Stick;

        ItemId pickaxeItem, axeItem, shovelItem, swordItem, hoeItem;
        switch (row.tier) {
            case ToolTier::Wood:
                pickaxeItem = ItemId::WoodPickaxe; axeItem = ItemId::WoodAxe;
                shovelItem = ItemId::WoodShovel; swordItem = ItemId::WoodSword; hoeItem = ItemId::WoodHoe;
                break;
            case ToolTier::Stone:
                pickaxeItem = ItemId::StonePickaxe; axeItem = ItemId::StoneAxe;
                shovelItem = ItemId::StoneShovel; swordItem = ItemId::StoneSword; hoeItem = ItemId::StoneHoe;
                break;
            case ToolTier::Iron:
                pickaxeItem = ItemId::IronPickaxe; axeItem = ItemId::IronAxe;
                shovelItem = ItemId::IronShovel; swordItem = ItemId::IronSword; hoeItem = ItemId::IronHoe;
                break;
            case ToolTier::Gold:
                pickaxeItem = ItemId::GoldPickaxe; axeItem = ItemId::GoldAxe;
                shovelItem = ItemId::GoldShovel; swordItem = ItemId::GoldSword; hoeItem = ItemId::GoldHoe;
                break;
            case ToolTier::Diamond:
                pickaxeItem = ItemId::DiamondPickaxe; axeItem = ItemId::DiamondAxe;
                shovelItem = ItemId::DiamondShovel; swordItem = ItemId::DiamondSword; hoeItem = ItemId::DiamondHoe;
                break;
            default:
                pickaxeItem = axeItem = shovelItem = swordItem = hoeItem = ItemId::None;
                break;
        }

        addShaped(shaped3x3({m, m, m, x, s, x, x, s, x}, pickaxeItem, 1, false));
        addShaped(shaped2x3(m, m, m, s, x, s, axeItem, 1));
        addShaped(shaped1x3(m, s, s, shovelItem, 1));
        addShaped(shaped1x3(m, m, s, swordItem, 1));
        addShaped(shaped2x3(m, m, x, s, x, s, hoeItem, 1));
    }

    addSmelting({ItemId::IronOre, {ItemId::IronIngot, 1}, 10.0f});
    addSmelting({ItemId::GoldOre, {ItemId::GoldIngot, 1}, 10.0f});
    addSmelting({ItemId::CoalOre, {ItemId::CoalLump, 1}, 8.0f});
    addSmelting({ItemId::DiamondOre, {ItemId::DiamondGem, 1}, 12.0f});
    addSmelting({ItemId::RedstoneOre, {ItemId::RedstoneDust, 1}, 8.0f});
    addSmelting({ItemId::EmeraldOre, {ItemId::EmeraldGem, 1}, 12.0f});
    addSmelting({ItemId::LapisOre, {ItemId::LapisLazuli, 1}, 8.0f});
    addSmelting({ItemId::Cobblestone, {ItemId::Stone, 1}, 10.0f});
    addSmelting({ItemId::RawPorkchop, {ItemId::CookedPorkchop, 1}, 10.0f});
    addSmelting({ItemId::RawBeef, {ItemId::CookedBeef, 1}, 10.0f});
    addSmelting({ItemId::RawMutton, {ItemId::CookedMutton, 1}, 10.0f});
}

CraftResult CraftingRegistry::craftMatch(const CraftGrid& grid) const {
    int minRow = craftGridHeight, maxRow = -1, minCol = craftGridWidth, maxCol = -1;
    for (int row = 0; row < craftGridHeight; ++row) {
        for (int col = 0; col < craftGridWidth; ++col) {
            if (grid[row * craftGridWidth + col] == ItemId::None) continue;
            minRow = std::min(minRow, row);
            maxRow = std::max(maxRow, row);
            minCol = std::min(minCol, col);
            maxCol = std::max(maxCol, col);
        }
    }
    if (maxRow < 0) return {};

    int width = maxCol - minCol + 1;
    int height = maxRow - minRow + 1;

    for (const ShapedRecipe& recipe : shaped) {
        if (recipe.width != width || recipe.height != height) continue;
        if (matchesShaped(grid, recipe, minRow, minCol, false)) return recipe.result;
        if (recipe.allowMirror && matchesShaped(grid, recipe, minRow, minCol, true)) return recipe.result;
    }

    std::vector<ItemId> present;
    for (ItemId id : grid)
        if (id != ItemId::None) present.push_back(id);
    std::sort(present.begin(), present.end());

    for (const ShapelessRecipe& recipe : shapeless) {
        if (recipe.ingredients.size() != present.size()) continue;
        std::vector<ItemId> want = recipe.ingredients;
        std::sort(want.begin(), want.end());
        if (want == present) return recipe.result;
    }

    return {};
}

const SmeltingRecipe* CraftingRegistry::smeltingFor(ItemId input) const {
    for (const SmeltingRecipe& recipe : smelting)
        if (recipe.input == input) return &recipe;
    return nullptr;
}

float fuelBurnSeconds(ItemId item) {
    switch (item) {
        case ItemId::Stick:      return 5.0f;
        case ItemId::Planks:     return 15.0f;
        case ItemId::Wood:       return 15.0f;
        case ItemId::CoalLump:   return 80.0f;
        default:                 return 0.0f;
    }
}

bool isFuel(ItemId item) { return fuelBurnSeconds(item) > 0.0f; }

Furnace::Furnace(const CraftingRegistry& reg) : registry(&reg) {}

void Furnace::tick(float dt) {
    const SmeltingRecipe* recipe = input.empty() ? nullptr : registry->smeltingFor(input.item);
    if (recipe == nullptr) smeltProgress = 0.0f;

    if (fuelTimeRemaining <= 0.0f && recipe != nullptr && !fuel.empty() && isFuel(fuel.item)) {
        float burn = fuelBurnSeconds(fuel.item);
        fuelTimeRemaining += burn;
        fuelTimeTotal = burn;
        fuel.count -= 1;
        if (fuel.count <= 0) fuel = ItemSlot{};
    }

    if (fuelTimeRemaining <= 0.0f) return;
    fuelTimeRemaining = std::max(0.0f, fuelTimeRemaining - dt);

    if (recipe == nullptr) return;

    bool canOutput = output.empty() ||
        (output.item == recipe->result.item &&
         output.count + recipe->result.count <= maxStackSize(output.item));
    if (!canOutput) return;

    smeltProgress += dt;
    if (smeltProgress >= recipe->smeltSeconds) {
        smeltProgress -= recipe->smeltSeconds;
        if (output.empty()) output = ItemSlot{recipe->result.item, recipe->result.count, 0};
        else output.count += recipe->result.count;
        input.count -= 1;
        if (input.count <= 0) input = ItemSlot{};
    }
}

float Furnace::smeltFraction() const {
    if (input.empty()) return 0.0f;
    const SmeltingRecipe* recipe = registry->smeltingFor(input.item);
    if (recipe == nullptr || recipe->smeltSeconds <= 0.0f) return 0.0f;
    return smeltProgress / recipe->smeltSeconds;
}

} // namespace vf
