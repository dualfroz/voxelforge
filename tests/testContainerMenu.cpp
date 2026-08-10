#include "testHarness.h"
#include "vf/ContainerMenu.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

const CraftingRegistry& sharedRegistry() {
    static CraftingRegistry registry;
    return registry;
}

// A recipe nothing in the default set uses: a tool sits in the grid as a proper
// ingredient, which is the only way to exercise the remainder rule.
CraftingRegistry registryWithToolRecipe() {
    CraftingRegistry registry;
    ShapedRecipe carving;
    carving.width = 1;
    carving.height = 2;
    carving.pattern = {ItemId::IronShovel, ItemId::Planks};
    carving.result = {ItemId::Stick, 4};
    carving.allowMirror = false;
    registry.addShaped(carving);
    return registry;
}

int countSlotsWithRole(const ContainerMenu& menu, MenuSlot::Role role) {
    return static_cast<int>(menu.slotsWithRole(role).size());
}

} // namespace

VF_TEST(everyMenuKindLaysOutTheSectionsItsScreenNeeds) {
    for (int i = 0; i < static_cast<int>(MenuKind::Count); ++i) {
        MenuKind kind = static_cast<MenuKind>(i);
        ContainerMenu menu(kind, &sharedRegistry());
        CHECK(menu.slotCount() > 0);
        CHECK_EQ(menu.slotCount(), menu.layout().totalSlots);
        CHECK_EQ(menu.layout().inventoryCount, playerInventorySlots);
        CHECK_EQ(menu.layout().hotbarCount, playerHotbarSlots);
        CHECK(std::string(menuKindName(kind)) != std::string("unknown"));
    }

    CHECK_EQ(ContainerMenu(MenuKind::Chest).slotCount(), 27 + 27 + 9);
    CHECK_EQ(ContainerMenu(MenuKind::LargeChest).slotCount(), 54 + 27 + 9);
    CHECK_EQ(ContainerMenu(MenuKind::Hopper).slotCount(), 5 + 27 + 9);
    CHECK_EQ(countSlotsWithRole(ContainerMenu(MenuKind::Hopper), MenuSlot::Role::Storage), 5);
}

VF_TEST(thePlayerInventoryKeepsItsTwoByTwoGridAndAnOffHandSlot) {
    ContainerMenu menu(MenuKind::PlayerInventory, &sharedRegistry());
    const MenuLayout& layout = menu.layout();

    CHECK_EQ(layout.craftWidth, 2);
    CHECK_EQ(layout.craftHeight, 2);
    CHECK_EQ(layout.craftCells(), 4);
    CHECK(layout.hasCrafting());
    CHECK(layout.resultSlot >= 0);
    CHECK(layout.offHandSlot >= 0);
    CHECK_EQ(countSlotsWithRole(menu, MenuSlot::Role::CraftInput), 4);
    CHECK_EQ(countSlotsWithRole(menu, MenuSlot::Role::OffHand), 1);
    CHECK(menu.isPlayerSection(layout.offHandSlot));
    CHECK(!menu.isPlayerSection(layout.resultSlot));
}

VF_TEST(theCraftingTableGivesTheFullThreeByThreeGrid) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    CHECK_EQ(menu.layout().craftWidth, 3);
    CHECK_EQ(menu.layout().craftHeight, 3);
    CHECK_EQ(countSlotsWithRole(menu, MenuSlot::Role::CraftInput), 9);
    CHECK(menu.hasCrafting());
    CHECK_EQ(menu.layout().offHandSlot, -1);
}

VF_TEST(theFurnaceFuelSlotRefusesAnythingThatWillNotBurn) {
    ContainerMenu menu(MenuKind::Furnace, &sharedRegistry());
    std::vector<int> fuelSlots = menu.slotsWithRole(MenuSlot::Role::FuelInput);
    CHECK_EQ(fuelSlots.size(), 1u);
    int fuel = fuelSlots[0];

    CHECK(menu.canPlace(fuel, makeStack(ItemId::CoalLump, 8)));
    CHECK(menu.canPlace(fuel, makeStack(ItemId::Planks, 8)));
    CHECK(!menu.canPlace(fuel, makeStack(ItemId::Stone, 8)));
    CHECK(!menu.canPlace(fuel, makeStack(ItemId::IronIngot, 1)));

    MetaStack stone = makeStack(ItemId::Stone, 8);
    CHECK_EQ(menu.insertAt(fuel, stone), 0);
    CHECK_EQ(stone.count, 8);

    MetaStack coal = makeStack(ItemId::CoalLump, 8);
    CHECK_EQ(menu.insertAt(fuel, coal), 8);
    CHECK(coal.empty());
    CHECK_EQ(menu.stackAt(fuel).count, 8);
}

VF_TEST(theFurnaceInputSlotOnlyTakesItemsWithASmeltingRecipe) {
    ContainerMenu menu(MenuKind::Furnace, &sharedRegistry());
    int input = menu.slotsWithRole(MenuSlot::Role::SmeltInput)[0];

    CHECK(menu.canPlace(input, makeStack(ItemId::IronOre, 4)));
    CHECK(menu.canPlace(input, makeStack(ItemId::RawBeef, 4)));
    CHECK(!menu.canPlace(input, makeStack(ItemId::Stick, 4)));
    CHECK(!menu.canPlace(input, makeStack(ItemId::Glowstone, 4)));

    // Without a registry there is nothing to consult, so nothing is smeltable.
    ContainerMenu blind(MenuKind::Furnace, nullptr);
    CHECK(!blind.canPlace(input, makeStack(ItemId::IronOre, 4)));
}

VF_TEST(theEnchantingTableTakesOnlyLapisForPaymentAndOnlyGearForTheItem) {
    ContainerMenu menu(MenuKind::EnchantingTable, &sharedRegistry());
    int item = menu.slotsWithRole(MenuSlot::Role::EnchantItem)[0];
    int lapis = menu.slotsWithRole(MenuSlot::Role::EnchantLapis)[0];

    CHECK(menu.canPlace(lapis, makeStack(ItemId::LapisLazuli, 3)));
    CHECK(!menu.canPlace(lapis, makeStack(ItemId::DiamondGem, 3)));
    CHECK(!menu.canPlace(lapis, makeStack(ItemId::LapisOre, 3)));

    CHECK(menu.canPlace(item, makeTool(ItemId::DiamondSword)));
    CHECK(!menu.canPlace(item, makeStack(ItemId::Stone, 1)));
    CHECK(!menu.canPlace(item, makeStack(ItemId::LapisLazuli, 1)));
}

VF_TEST(theAnvilOnlyAcceptsItemsThatCarryDurability) {
    ContainerMenu menu(MenuKind::Anvil, &sharedRegistry());
    std::vector<int> inputs = menu.slotsWithRole(MenuSlot::Role::AnvilInput);
    CHECK_EQ(inputs.size(), 2u);

    CHECK(menu.canPlace(inputs[0], makeTool(ItemId::GoldHoe)));
    CHECK(menu.canPlace(inputs[1], makeTool(ItemId::DiamondAxe)));
    CHECK(!menu.canPlace(inputs[0], makeStack(ItemId::IronIngot, 4)));
}

VF_TEST(theBrewingStandFiltersIngredientsAndHoldsOneItemPerBottleSlot) {
    ContainerMenu menu(MenuKind::BrewingStand, &sharedRegistry());
    int ingredient = menu.slotsWithRole(MenuSlot::Role::BrewIngredient)[0];
    std::vector<int> bottles = menu.slotsWithRole(MenuSlot::Role::BrewBottle);
    CHECK_EQ(bottles.size(), 3u);

    CHECK(menu.canPlace(ingredient, makeStack(ItemId::Gunpowder, 2)));
    CHECK(menu.canPlace(ingredient, makeStack(ItemId::RedstoneDust, 2)));
    CHECK(!menu.canPlace(ingredient, makeStack(ItemId::Stick, 2)));

    MetaStack pile = makeStack(ItemId::Glowstone, 20);
    int moved = menu.insertAt(bottles[0], pile);
    CHECK_EQ(moved, 1);
    CHECK_EQ(menu.stackAt(bottles[0]).count, 1);
    CHECK_EQ(pile.count, 19);
    CHECK_EQ(menu.insertAt(bottles[0], pile), 0);
}

VF_TEST(aResultSlotCanBeEmptiedButNeverFilledByHand) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    int result = menu.layout().resultSlot;

    CHECK(menu.slot(result).takeOnly);
    CHECK(!menu.canPlace(result, makeStack(ItemId::Stone, 1)));

    MetaStack stone = makeStack(ItemId::Stone, 1);
    CHECK_EQ(menu.insertAt(result, stone), 0);
    CHECK_EQ(stone.count, 1);
    CHECK(menu.stackAt(result).empty());

    // The furnace, anvil and trade outputs answer the same way.
    ContainerMenu furnace(MenuKind::Furnace, &sharedRegistry());
    int output = furnace.slotsWithRole(MenuSlot::Role::SmeltOutput)[0];
    CHECK(!furnace.canPlace(output, makeStack(ItemId::IronIngot, 1)));

    ContainerMenu villager(MenuKind::Villager, &sharedRegistry());
    int trade = villager.slotsWithRole(MenuSlot::Role::TradeResult)[0];
    CHECK(!villager.canPlace(trade, makeStack(ItemId::EmeraldGem, 1)));
}

VF_TEST(theCraftingViewRecomputesTheResultAfterEveryGridChange) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    CraftingView grid = menu.crafting();
    CHECK_EQ(grid.cellCount(), 9);
    CHECK(!grid.hasResult());

    grid.inputAt(0) = makeStack(ItemId::Wood, 1);
    grid.refresh();
    CHECK(grid.hasResult());
    CHECK(grid.result().item == ItemId::Planks);
    CHECK_EQ(grid.result().count, 4);

    grid.inputAt(0).clear();
    grid.refresh();
    CHECK(!grid.hasResult());

    grid.input(0, 0) = makeStack(ItemId::Planks, 3);
    grid.input(0, 1) = makeStack(ItemId::Planks, 3);
    grid.refresh();
    CHECK(grid.result().item == ItemId::Stick);
    CHECK_EQ(grid.result().count, 4);
}

VF_TEST(takingTheResultSpendsExactlyOneItemFromEachFilledInputSlot) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    CraftingView grid = menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Planks, 5);
    grid.input(0, 1) = makeStack(ItemId::Planks, 3);
    grid.refresh();
    CHECK_EQ(grid.maxCrafts(), 3);

    MetaStack sticks = grid.takeResult();
    CHECK(sticks.item == ItemId::Stick);
    CHECK_EQ(sticks.count, 4);
    CHECK_EQ(grid.input(0, 0).count, 4);
    CHECK_EQ(grid.input(0, 1).count, 2);
    CHECK(grid.hasResult());

    grid.takeResult();
    grid.takeResult();
    CHECK_EQ(grid.input(0, 0).count, 2);
    CHECK(grid.input(0, 1).empty());
    CHECK(!grid.hasResult());
    CHECK(grid.takeResult().empty());
}

VF_TEST(aDamageableIngredientStaysInTheGridOnePointWorse) {
    CraftingRegistry registry = registryWithToolRecipe();
    ContainerMenu menu(MenuKind::CraftingTable, &registry);
    CraftingView grid = menu.crafting();

    grid.input(0, 0) = makeTool(ItemId::IronShovel);
    grid.input(0, 1) = makeStack(ItemId::Planks, 4);
    grid.refresh();
    CHECK(grid.hasResult());
    CHECK(grid.result().item == ItemId::Stick);

    MetaStack sticks = grid.takeResult();
    CHECK_EQ(sticks.count, 4);
    CHECK(!grid.input(0, 0).empty());
    CHECK(grid.input(0, 0).item == ItemId::IronShovel);
    CHECK_EQ(grid.input(0, 0).meta.durability.used, 1);
    CHECK_EQ(grid.input(0, 1).count, 3);

    // A shovel one point from death is spent by the craft instead of surviving.
    grid.input(0, 0).meta.durability.used = grid.input(0, 0).meta.durability.max - 1;
    grid.refresh();
    grid.takeResult();
    CHECK(grid.input(0, 0).empty());
}

VF_TEST(shiftCraftingRepeatsUntilTheGridRunsDry) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    CraftingView grid = menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Planks, 6);
    grid.input(0, 1) = makeStack(ItemId::Planks, 6);
    grid.refresh();

    int rounds = grid.craftAll(grid.maxCrafts());
    CHECK_EQ(rounds, 6);
    CHECK(grid.input(0, 0).empty());
    CHECK(grid.input(0, 1).empty());
    CHECK(!grid.hasResult());
    CHECK_EQ(menu.totalOf(ItemId::Stick), 24);
    CHECK_EQ(menu.totalOf(ItemId::Planks), 0);
}

VF_TEST(craftingStopsWhenThePlayerSectionsHaveNoRoomLeft) {
    ContainerMenu menu(MenuKind::CraftingTable, &sharedRegistry());
    for (int index : menu.playerSectionSlots()) {
        menu.slot(index).cell.stack = makeStack(ItemId::Obsidian, 64);
    }

    CraftingView grid = menu.crafting();
    grid.input(0, 0) = makeStack(ItemId::Planks, 4);
    grid.input(0, 1) = makeStack(ItemId::Planks, 4);
    grid.refresh();

    int rounds = grid.craftAll(grid.maxCrafts());
    CHECK_EQ(rounds, 1);
    CHECK_EQ(menu.totalOf(ItemId::Stick), 4);
    CHECK_EQ(menu.stackAt(menu.layout().resultSlot).count, 4);
    CHECK_EQ(menu.totalOf(ItemId::Planks), 6);
}

VF_TEST(distributingFillsPartialStacksBeforeReachingForAnEmptySlot) {
    ContainerMenu menu(MenuKind::Chest, &sharedRegistry());
    std::vector<int> storage = menu.containerSectionSlots();
    menu.slot(storage[0]).cell.stack = makeStack(ItemId::Sand, 60);
    menu.slot(storage[3]).cell.stack = makeStack(ItemId::Sand, 50);

    MetaStack incoming = makeStack(ItemId::Sand, 20);
    int moved = menu.distributeInto(storage, incoming);
    CHECK_EQ(moved, 20);
    CHECK(incoming.empty());
    CHECK_EQ(menu.stackAt(storage[0]).count, 64);
    CHECK_EQ(menu.stackAt(storage[3]).count, 64);
    CHECK_EQ(menu.stackAt(storage[1]).count, 2);
    CHECK(menu.stackAt(storage[2]).empty());
}

VF_TEST(distributingReportsWhatItCouldNotPlace) {
    ContainerMenu menu(MenuKind::Hopper, &sharedRegistry());
    std::vector<int> storage = menu.containerSectionSlots();
    for (int index : storage) menu.slot(index).cell.stack = makeStack(ItemId::Obsidian, 64);

    MetaStack incoming = makeStack(ItemId::Sand, 30);
    CHECK_EQ(menu.distributeInto(storage, incoming), 0);
    CHECK_EQ(incoming.count, 30);
}

VF_TEST(aBoundContainerLoadsIntoTheStorageSectionAndIsWrittenBackOnClose) {
    Container chest(27);
    chest.insert(ItemId::Cobblestone, 90);
    chest.setSlot(5, ItemSlot{ItemId::IronPickaxe, 1, 100});

    ContainerMenu menu(MenuKind::Chest, &sharedRegistry());
    menu.bindContainer(chest);
    CHECK(menu.hasContainer());
    CHECK_EQ(menu.totalOf(ItemId::Cobblestone), 90);

    std::vector<int> storage = menu.containerSectionSlots();
    CHECK_EQ(menu.stackAt(storage[5]).item, ItemId::IronPickaxe);
    CHECK_EQ(menu.stackAt(storage[5]).meta.durability.remaining(), 100);
    CHECK_EQ(menu.stackAt(storage[5]).meta.durability.max, toolMaxDurability(ItemId::IronPickaxe));

    menu.slot(storage[10]).cell.stack = makeStack(ItemId::EmeraldGem, 7);
    MetaStack pulled = menu.takeFrom(storage[0], 30);
    CHECK_EQ(pulled.count, 30);

    menu.unbind();
    CHECK(!menu.hasContainer());
    CHECK_EQ(chest.slot(10).item, ItemId::EmeraldGem);
    CHECK_EQ(chest.slot(10).count, 7);
    CHECK_EQ(chest.slot(0).count, 34);
    CHECK_EQ(chest.slot(5).durability, 100);
}

VF_TEST(bindingNeverReachesPastTheContainersOwnSize) {
    Container tiny(3);
    tiny.insert(ItemId::Stick, 5);

    ContainerMenu menu(MenuKind::Chest, &sharedRegistry());
    menu.bindContainer(tiny);
    CHECK_EQ(menu.totalOf(ItemId::Stick), 5);

    std::vector<int> storage = menu.containerSectionSlots();
    CHECK_EQ(menu.slot(storage[2]).backingIndex, 2);
    CHECK_EQ(menu.slot(storage[3]).backingIndex, -1);

    menu.slot(storage[9]).cell.stack = makeStack(ItemId::Stone, 4);
    menu.unbind();
    CHECK_EQ(tiny.totalItems(), 5);
}

VF_TEST(theFurnaceMenuBindsAThreeSlotContainerOntoItsOwnRoles) {
    Container firebox(3);
    firebox.setSlot(0, ItemSlot{ItemId::IronOre, 8, 0});
    firebox.setSlot(1, ItemSlot{ItemId::CoalLump, 4, 0});
    firebox.setSlot(2, ItemSlot{ItemId::IronIngot, 2, 0});

    ContainerMenu menu(MenuKind::Furnace, &sharedRegistry());
    menu.bindContainer(firebox);
    CHECK_EQ(menu.stackAt(menu.slotsWithRole(MenuSlot::Role::SmeltInput)[0]).count, 8);
    CHECK_EQ(menu.stackAt(menu.slotsWithRole(MenuSlot::Role::FuelInput)[0]).count, 4);
    CHECK_EQ(menu.stackAt(menu.slotsWithRole(MenuSlot::Role::SmeltOutput)[0]).count, 2);

    menu.takeFrom(menu.slotsWithRole(MenuSlot::Role::SmeltOutput)[0], 2);
    menu.syncToContainer();
    CHECK(firebox.slot(2).empty());
}

VF_TEST(menuNbtRoundTripKeepsEnchantedAndNamedStacksThatAContainerWouldLose) {
    ContainerMenu menu(MenuKind::LargeChest, &sharedRegistry());
    EnchantSet enchants;
    addEnchant(enchants, EnchantKind::Fortune, 3);
    addEnchant(enchants, EnchantKind::Unbreaking, 2);

    MetaStack pickaxe = makeEnchantedTool(ItemId::DiamondPickaxe, enchants);
    pickaxe.meta.customName = "Deep Cut";
    pickaxe.meta.durability.used = 220;

    menu.slot(0).cell.stack = pickaxe;
    menu.slot(7).cell.stack = makeStack(ItemId::Gravel, 40);
    menu.setCreativeMode(true);

    ContainerMenu restored(MenuKind::Chest, &sharedRegistry());
    CHECK(restored.fromNbt(menu.toNbt()));
    CHECK(restored.kind() == MenuKind::LargeChest);
    CHECK_EQ(restored.slotCount(), menu.slotCount());
    CHECK(restored.creativeMode());
    CHECK(stacksIdentical(restored.stackAt(0), pickaxe));
    CHECK(restored.stackAt(0).meta.customName == std::string("Deep Cut"));
    CHECK_EQ(restored.stackAt(0).meta.enchants.levelOf(EnchantKind::Fortune), 3);
    CHECK_EQ(restored.stackAt(7).count, 40);
}

VF_TEST(menuNbtRejectsATagWhoseSlotCountDoesNotMatchTheKind) {
    ContainerMenu menu(MenuKind::Chest, &sharedRegistry());
    NbtTag tag = menu.toNbt();
    NbtTag* slots = nullptr;
    for (auto& entry : tag.asCompound()) {
        if (entry.first == "Slots") slots = &entry.second;
    }
    CHECK(slots != nullptr);
    if (slots == nullptr) return;
    slots->asList().pop_back();

    ContainerMenu target(MenuKind::Chest, &sharedRegistry());
    CHECK(!target.fromNbt(tag));

    NbtTag notAMenu = NbtTag::makeCompound();
    notAMenu.put("Kind", NbtTag::makeShort(99));
    CHECK(!target.fromNbt(notAMenu));
}

VF_TEST(slotAccountingAddsUpAcrossTheWholeMenu) {
    ContainerMenu menu(MenuKind::Chest, &sharedRegistry());
    CHECK_EQ(menu.totalItems(), 0);
    CHECK_EQ(menu.usedSlots(), 0);

    menu.slot(0).cell.stack = makeStack(ItemId::Stone, 64);
    menu.slot(1).cell.stack = makeStack(ItemId::Stone, 12);
    menu.slot(2).cell.stack = makeTool(ItemId::GoldAxe);

    CHECK_EQ(menu.totalOf(ItemId::Stone), 76);
    CHECK_EQ(menu.totalOf(ItemId::GoldAxe), 1);
    CHECK_EQ(menu.totalOf(ItemId::Sand), 0);
    CHECK_EQ(menu.totalItems(), 77);
    CHECK_EQ(menu.usedSlots(), 3);
    CHECK(!menu.validIndex(-1));
    CHECK(!menu.validIndex(menu.slotCount()));
}

VF_TEST(everyRoleAndFilterNameIsSpelledOut) {
    for (int i = 0; i < static_cast<int>(MenuSlot::Role::Count); ++i) {
        CHECK(std::string(menuSlotRoleName(static_cast<MenuSlot::Role>(i))) !=
              std::string("unknown"));
    }
    for (int i = 0; i < static_cast<int>(SlotFilter::Kind::Count); ++i) {
        CHECK(std::string(slotFilterKindName(static_cast<SlotFilter::Kind>(i))) !=
              std::string("unknown"));
    }
}

// A result slot is guarded twice over: the takeOnly flag and a filter that lets
// nothing through. Because canPlace checks takeOnly first, the filter would go
// untested through the menu, so it is pinned here on its own.
VF_TEST(everySlotFilterAnswersForItselfWithoutHelpFromTheTakeOnlyFlag) {
    const CraftingRegistry& registry = sharedRegistry();

    SlotFilter nothing;
    nothing.kind = SlotFilter::Kind::Nothing;
    CHECK(!nothing.accepts(makeStack(ItemId::Stone, 1), &registry));
    CHECK(!nothing.accepts(makeStack(ItemId::CoalLump, 1), &registry));
    CHECK(!nothing.accepts(makeTool(ItemId::DiamondSword), &registry));
    CHECK(!nothing.accepts(makeStack(ItemId::LapisLazuli, 1), &registry));

    SlotFilter any;
    any.kind = SlotFilter::Kind::Any;
    CHECK(any.accepts(makeStack(ItemId::Stone, 1), nullptr));
    CHECK(any.accepts(makeTool(ItemId::GoldHoe), nullptr));

    SlotFilter fuel;
    fuel.kind = SlotFilter::Kind::Fuel;
    CHECK(fuel.accepts(makeStack(ItemId::CoalLump, 1), nullptr));
    CHECK(fuel.accepts(makeStack(ItemId::Stick, 1), nullptr));
    CHECK(!fuel.accepts(makeStack(ItemId::IronIngot, 1), nullptr));

    SlotFilter smeltable;
    smeltable.kind = SlotFilter::Kind::Smeltable;
    CHECK(smeltable.accepts(makeStack(ItemId::GoldOre, 1), &registry));
    CHECK(!smeltable.accepts(makeStack(ItemId::GoldIngot, 1), &registry));
    CHECK(!smeltable.accepts(makeStack(ItemId::GoldOre, 1), nullptr));

    SlotFilter lapis;
    lapis.kind = SlotFilter::Kind::Lapis;
    CHECK(lapis.accepts(makeStack(ItemId::LapisLazuli, 1), nullptr));
    CHECK(!lapis.accepts(makeStack(ItemId::LapisOre, 1), nullptr));

    SlotFilter enchantable;
    enchantable.kind = SlotFilter::Kind::Enchantable;
    CHECK(enchantable.accepts(makeTool(ItemId::GoldSword), nullptr));
    CHECK(!enchantable.accepts(makeStack(ItemId::Obsidian, 1), nullptr));

    SlotFilter repairable;
    repairable.kind = SlotFilter::Kind::Repairable;
    CHECK(repairable.accepts(makeTool(ItemId::StoneHoe), nullptr));
    CHECK(!repairable.accepts(makeStack(ItemId::Planks, 1), nullptr));

    SlotFilter brewing;
    brewing.kind = SlotFilter::Kind::BrewIngredient;
    CHECK(brewing.accepts(makeStack(ItemId::Gunpowder, 1), nullptr));
    CHECK(brewing.accepts(makeStack(ItemId::Glowstone, 1), nullptr));
    CHECK(brewing.accepts(makeStack(ItemId::RedstoneDust, 1), nullptr));
    CHECK(!brewing.accepts(makeStack(ItemId::RedstoneBlock, 1), nullptr));
}

VF_TEST(aFilterNeverAcceptsAnEmptyStack) {
    for (int i = 0; i < static_cast<int>(SlotFilter::Kind::Count); ++i) {
        SlotFilter filter;
        filter.kind = static_cast<SlotFilter::Kind>(i);
        CHECK(!filter.accepts(MetaStack{}, &sharedRegistry()));
    }
}
