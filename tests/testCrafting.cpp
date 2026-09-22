#include "testHarness.h"
#include "vf/Crafting.h"

using namespace vf;

namespace {

CraftGrid emptyGrid() {
    CraftGrid grid;
    grid.fill(ItemId::None);
    return grid;
}

void place(CraftGrid& grid, int row, int col, ItemId item) {
    grid[static_cast<std::size_t>(row * craftGridWidth + col)] = item;
}

}

VF_TEST(defaultRegistryLoadsExpectedRecipeCounts) {
    CraftingRegistry registry;
    CHECK_EQ(registry.shapedRecipes().size(), static_cast<std::size_t>(31));
    CHECK_EQ(registry.shapelessRecipes().size(), static_cast<std::size_t>(1));
    CHECK_EQ(registry.smeltingRecipes().size(), static_cast<std::size_t>(11));
}

VF_TEST(shapelessPlanksMatchesFromAnySingleCell) {
    CraftingRegistry registry;
    for (int i = 0; i < craftGridCells; ++i) {
        CraftGrid grid = emptyGrid();
        grid[static_cast<std::size_t>(i)] = ItemId::Wood;
        CraftResult result = registry.craftMatch(grid);
        CHECK(result.valid());
        CHECK(result.item == ItemId::Planks);
        CHECK_EQ(result.count, 4);
    }
}

VF_TEST(shapedRecipeMatchesRegardlessOfGridOffset) {
    CraftingRegistry registry;
    int origins[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (auto& origin : origins) {
        CraftGrid grid = emptyGrid();
        place(grid, origin[0], origin[1], ItemId::Planks);
        place(grid, origin[0], origin[1] + 1, ItemId::Planks);
        place(grid, origin[0] + 1, origin[1], ItemId::Planks);
        place(grid, origin[0] + 1, origin[1] + 1, ItemId::Planks);
        CraftResult result = registry.craftMatch(grid);
        CHECK(result.valid());
        CHECK(result.item == ItemId::CraftingTable);
    }
}

VF_TEST(shapedRecipeMatchesMirroredOrientation) {
    CraftingRegistry registry;

    CraftGrid direct = emptyGrid();
    place(direct, 0, 0, ItemId::Planks);
    place(direct, 0, 1, ItemId::Planks);
    place(direct, 1, 0, ItemId::Planks);
    place(direct, 1, 1, ItemId::Stick);
    place(direct, 2, 1, ItemId::Stick);
    CraftResult directResult = registry.craftMatch(direct);
    CHECK(directResult.valid());
    CHECK(directResult.item == ItemId::WoodAxe);

    CraftGrid mirrored = emptyGrid();
    place(mirrored, 0, 1, ItemId::Planks);
    place(mirrored, 0, 2, ItemId::Planks);
    place(mirrored, 1, 1, ItemId::Stick);
    place(mirrored, 1, 2, ItemId::Planks);
    place(mirrored, 2, 1, ItemId::Stick);
    CraftResult mirroredResult = registry.craftMatch(mirrored);
    CHECK(mirroredResult.valid());
    CHECK(mirroredResult.item == ItemId::WoodAxe);
}

VF_TEST(shapedPickaxeBreaksWhenAStickIsMissing) {
    CraftingRegistry registry;
    CraftGrid grid = emptyGrid();
    place(grid, 0, 0, ItemId::Cobblestone);
    place(grid, 0, 1, ItemId::Cobblestone);
    place(grid, 0, 2, ItemId::Cobblestone);
    place(grid, 1, 1, ItemId::Stick);
    place(grid, 2, 1, ItemId::Stick);
    CraftResult good = registry.craftMatch(grid);
    CHECK(good.valid());
    CHECK(good.item == ItemId::StonePickaxe);

    place(grid, 2, 1, ItemId::None);
    CraftResult broken = registry.craftMatch(grid);
    CHECK(!broken.valid());
}

VF_TEST(incoherentGridProducesNoMatch) {
    CraftingRegistry registry;
    CraftGrid grid = emptyGrid();
    place(grid, 0, 0, ItemId::Stone);
    place(grid, 2, 2, ItemId::Leaves);
    CraftResult result = registry.craftMatch(grid);
    CHECK(!result.valid());
}

VF_TEST(shapelessRecipeMatchesIngredientMultisetIgnoringPosition) {
    CraftingRegistry registry;
    registry.addShapeless({{ItemId::Stick, ItemId::Stick, ItemId::CoalLump}, {ItemId::Torch, 8}});

    CraftGrid grid = emptyGrid();
    place(grid, 0, 0, ItemId::Stick);
    place(grid, 0, 2, ItemId::Stick);
    place(grid, 2, 1, ItemId::CoalLump);
    CraftResult result = registry.craftMatch(grid);
    CHECK(result.valid());
    CHECK(result.item == ItemId::Torch);
    CHECK_EQ(result.count, 8);

    CraftGrid tooFew = emptyGrid();
    place(tooFew, 0, 0, ItemId::Stick);
    place(tooFew, 2, 1, ItemId::CoalLump);
    CraftResult resultTooFew = registry.craftMatch(tooFew);
    CHECK(!resultTooFew.valid());
}

VF_TEST(smeltingRegistryFindsRecipeByInput) {
    CraftingRegistry registry;
    const SmeltingRecipe* ironRecipe = registry.smeltingFor(ItemId::IronOre);
    CHECK(ironRecipe != nullptr);
    CHECK(ironRecipe->result.item == ItemId::IronIngot);

    const SmeltingRecipe* none = registry.smeltingFor(ItemId::Stick);
    CHECK(none == nullptr);
}

VF_TEST(fuelBurnSecondsRanksCoalAboveWood) {
    CHECK(isFuel(ItemId::CoalLump));
    CHECK(isFuel(ItemId::Planks));
    CHECK(!isFuel(ItemId::Stone));
    CHECK(fuelBurnSeconds(ItemId::CoalLump) > fuelBurnSeconds(ItemId::Planks));
}

VF_TEST(furnaceDoesNothingWithoutFuel) {
    CraftingRegistry registry;
    Furnace furnace(registry);
    furnace.input = {ItemId::IronOre, 4, 0};
    for (int i = 0; i < 200; ++i) furnace.tick(1.0f);
    CHECK(!furnace.isLit());
    CHECK(furnace.output.empty());
    CHECK_EQ(furnace.input.count, 4);
}

VF_TEST(furnaceSmeltsOreIntoIngotsOverTime) {
    CraftingRegistry registry;
    Furnace furnace(registry);
    furnace.input = {ItemId::IronOre, 3, 0};
    furnace.fuel = {ItemId::CoalLump, 1, 0};

    bool sawLit = false;
    for (int i = 0; i < 200; ++i) {
        furnace.tick(0.5f);
        if (furnace.isLit()) sawLit = true;
    }
    CHECK(sawLit);
    CHECK(furnace.output.item == ItemId::IronIngot);
    CHECK(furnace.output.count >= 1);
    CHECK(furnace.input.count < 3);
}

VF_TEST(furnaceStopsProgressingWhenOutputHoldsADifferentItem) {
    CraftingRegistry registry;
    Furnace furnace(registry);
    furnace.input = {ItemId::IronOre, 1, 0};
    furnace.fuel = {ItemId::CoalLump, 1, 0};
    furnace.output = {ItemId::GoldIngot, 5, 0};

    for (int i = 0; i < 60; ++i) furnace.tick(0.5f);
    CHECK(furnace.output.item == ItemId::GoldIngot);
    CHECK_EQ(furnace.output.count, 5);
    CHECK_EQ(furnace.input.count, 1);
}
