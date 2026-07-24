#include "vf/ContainerMenu.h"

#include <algorithm>

namespace vf {

namespace {

const MetaStack& emptyStack() {
    static const MetaStack nothing;
    return nothing;
}

bool isBrewIngredient(ItemId item) {
    // Brewing.h models potions with PotionItem, not ItemId, so only the
    // ingredients that also exist as items can be filtered here.
    return item == ItemId::RedstoneDust || item == ItemId::Glowstone ||
           item == ItemId::Gunpowder;
}

void appendSection(std::vector<MenuSlot>& slots, MenuSlot::Role role, int count,
                   SlotFilter::Kind filter, bool takeOnly, int capacity) {
    for (int i = 0; i < count; ++i) {
        MenuSlot menuSlot;
        menuSlot.role = role;
        menuSlot.filter.kind = filter;
        menuSlot.takeOnly = takeOnly;
        menuSlot.cell.capacity = capacity;
        slots.push_back(menuSlot);
    }
}

void appendPlayerSections(std::vector<MenuSlot>& slots, MenuLayout& layout, bool withOffHand) {
    layout.inventoryFirst = static_cast<int>(slots.size());
    layout.inventoryCount = playerInventorySlots;
    appendSection(slots, MenuSlot::Role::Inventory, playerInventorySlots, SlotFilter::Kind::Any,
                  false, 0);

    layout.hotbarFirst = static_cast<int>(slots.size());
    layout.hotbarCount = playerHotbarSlots;
    appendSection(slots, MenuSlot::Role::Hotbar, playerHotbarSlots, SlotFilter::Kind::Any, false, 0);

    if (withOffHand) {
        layout.offHandSlot = static_cast<int>(slots.size());
        appendSection(slots, MenuSlot::Role::OffHand, 1, SlotFilter::Kind::Any, false, 0);
    }
}

} // namespace

const char* slotFilterKindName(SlotFilter::Kind kind) {
    switch (kind) {
        case SlotFilter::Kind::Any:            return "any";
        case SlotFilter::Kind::Nothing:        return "nothing";
        case SlotFilter::Kind::Fuel:           return "fuel";
        case SlotFilter::Kind::Smeltable:      return "smeltable";
        case SlotFilter::Kind::Lapis:          return "lapis";
        case SlotFilter::Kind::Enchantable:    return "enchantable";
        case SlotFilter::Kind::Repairable:     return "repairable";
        case SlotFilter::Kind::BrewIngredient: return "brewIngredient";
        case SlotFilter::Kind::Count:          break;
    }
    return "unknown";
}

bool SlotFilter::accepts(const MetaStack& stack, const CraftingRegistry* registry) const {
    if (stack.empty()) return false;
    switch (kind) {
        case Kind::Any:            return true;
        case Kind::Nothing:        return false;
        case Kind::Fuel:           return isFuel(stack.item);
        case Kind::Smeltable:
            return registry != nullptr && registry->smeltingFor(stack.item) != nullptr;
        case Kind::Lapis:          return stack.item == ItemId::LapisLazuli;
        case Kind::Enchantable:    return enchantabilityOf(stack.item) > 0;
        case Kind::Repairable:     return stack.meta.durability.damageable();
        case Kind::BrewIngredient: return isBrewIngredient(stack.item);
        case Kind::Count:          break;
    }
    return false;
}

int StackSlot::limitFor(const MetaStack& candidate) const {
    if (candidate.empty()) return 0;
    int limit = candidate.maxCount();
    if (capacity > 0 && capacity < limit) limit = capacity;
    return limit;
}

int StackSlot::room(const MetaStack& candidate) const {
    if (candidate.empty()) return 0;
    if (stack.empty()) return limitFor(candidate);
    if (!stacksMergeable(stack, candidate)) return 0;
    int left = limitFor(stack) - stack.count;
    return left > 0 ? left : 0;
}

int StackSlot::insert(MetaStack& incoming) {
    int allowed = room(incoming);
    if (allowed <= 0) return 0;
    return transferStack(incoming, stack, allowed).moved;
}

MetaStack StackSlot::take(int amount) { return splitOff(stack, amount); }

MetaStack StackSlot::takeAll() { return splitOff(stack, stack.count); }

MetaStack StackSlot::swapWith(const MetaStack& incoming) {
    MetaStack previous = stack;
    stack = incoming;
    return previous;
}

const char* menuKindName(MenuKind kind) {
    switch (kind) {
        case MenuKind::PlayerInventory: return "playerInventory";
        case MenuKind::CraftingTable:   return "craftingTable";
        case MenuKind::Chest:           return "chest";
        case MenuKind::LargeChest:      return "largeChest";
        case MenuKind::Furnace:         return "furnace";
        case MenuKind::BrewingStand:    return "brewingStand";
        case MenuKind::EnchantingTable: return "enchantingTable";
        case MenuKind::Anvil:           return "anvil";
        case MenuKind::Villager:        return "villager";
        case MenuKind::Hopper:          return "hopper";
        case MenuKind::Count:           break;
    }
    return "unknown";
}

const char* menuSlotRoleName(MenuSlot::Role role) {
    switch (role) {
        case MenuSlot::Role::Storage:        return "storage";
        case MenuSlot::Role::Inventory:      return "inventory";
        case MenuSlot::Role::Hotbar:         return "hotbar";
        case MenuSlot::Role::CraftInput:     return "craftInput";
        case MenuSlot::Role::CraftResult:    return "craftResult";
        case MenuSlot::Role::FuelInput:      return "fuelInput";
        case MenuSlot::Role::SmeltInput:     return "smeltInput";
        case MenuSlot::Role::SmeltOutput:    return "smeltOutput";
        case MenuSlot::Role::BrewBottle:     return "brewBottle";
        case MenuSlot::Role::BrewIngredient: return "brewIngredient";
        case MenuSlot::Role::EnchantItem:    return "enchantItem";
        case MenuSlot::Role::EnchantLapis:   return "enchantLapis";
        case MenuSlot::Role::AnvilInput:     return "anvilInput";
        case MenuSlot::Role::AnvilResult:    return "anvilResult";
        case MenuSlot::Role::TradeInput:     return "tradeInput";
        case MenuSlot::Role::TradeResult:    return "tradeResult";
        case MenuSlot::Role::OffHand:        return "offHand";
        case MenuSlot::Role::Count:          break;
    }
    return "unknown";
}

namespace {

std::vector<MenuSlot> buildLayout(MenuKind kind, MenuLayout& layout) {
    std::vector<MenuSlot> slots;
    layout = MenuLayout{};
    layout.kind = kind;

    switch (kind) {
        case MenuKind::PlayerInventory: {
            layout.craftFirst = 0;
            layout.craftWidth = 2;
            layout.craftHeight = 2;
            appendSection(slots, MenuSlot::Role::CraftInput, 4, SlotFilter::Kind::Any, false, 0);
            layout.resultSlot = static_cast<int>(slots.size());
            appendSection(slots, MenuSlot::Role::CraftResult, 1, SlotFilter::Kind::Nothing, true, 0);
            appendPlayerSections(slots, layout, true);
            break;
        }
        case MenuKind::CraftingTable: {
            layout.craftFirst = 0;
            layout.craftWidth = 3;
            layout.craftHeight = 3;
            appendSection(slots, MenuSlot::Role::CraftInput, 9, SlotFilter::Kind::Any, false, 0);
            layout.resultSlot = static_cast<int>(slots.size());
            appendSection(slots, MenuSlot::Role::CraftResult, 1, SlotFilter::Kind::Nothing, true, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Chest:
        case MenuKind::LargeChest: {
            int size = kind == MenuKind::Chest ? 27 : 54;
            layout.storageFirst = 0;
            layout.storageCount = size;
            appendSection(slots, MenuSlot::Role::Storage, size, SlotFilter::Kind::Any, false, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Furnace: {
            layout.storageFirst = 0;
            layout.storageCount = 3;
            appendSection(slots, MenuSlot::Role::SmeltInput, 1, SlotFilter::Kind::Smeltable, false, 0);
            appendSection(slots, MenuSlot::Role::FuelInput, 1, SlotFilter::Kind::Fuel, false, 0);
            appendSection(slots, MenuSlot::Role::SmeltOutput, 1, SlotFilter::Kind::Nothing, true, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::BrewingStand: {
            layout.storageFirst = 0;
            layout.storageCount = 5;
            appendSection(slots, MenuSlot::Role::BrewBottle, 3, SlotFilter::Kind::Any, false, 1);
            appendSection(slots, MenuSlot::Role::BrewIngredient, 1,
                          SlotFilter::Kind::BrewIngredient, false, 0);
            appendSection(slots, MenuSlot::Role::FuelInput, 1, SlotFilter::Kind::Fuel, false, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::EnchantingTable: {
            layout.storageFirst = 0;
            layout.storageCount = 2;
            appendSection(slots, MenuSlot::Role::EnchantItem, 1, SlotFilter::Kind::Enchantable,
                          false, 0);
            appendSection(slots, MenuSlot::Role::EnchantLapis, 1, SlotFilter::Kind::Lapis, false, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Anvil: {
            layout.storageFirst = 0;
            layout.storageCount = 3;
            appendSection(slots, MenuSlot::Role::AnvilInput, 2, SlotFilter::Kind::Repairable,
                          false, 0);
            appendSection(slots, MenuSlot::Role::AnvilResult, 1, SlotFilter::Kind::Nothing, true, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Villager: {
            layout.storageFirst = 0;
            layout.storageCount = 3;
            appendSection(slots, MenuSlot::Role::TradeInput, 2, SlotFilter::Kind::Any, false, 0);
            appendSection(slots, MenuSlot::Role::TradeResult, 1, SlotFilter::Kind::Nothing, true, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Hopper: {
            layout.storageFirst = 0;
            layout.storageCount = 5;
            appendSection(slots, MenuSlot::Role::Storage, 5, SlotFilter::Kind::Any, false, 0);
            appendPlayerSections(slots, layout, false);
            break;
        }
        case MenuKind::Count:
            break;
    }

    layout.totalSlots = static_cast<int>(slots.size());
    return slots;
}

} // namespace

MenuLayout layoutFor(MenuKind kind) {
    MenuLayout layout;
    buildLayout(kind, layout);
    return layout;
}

CraftingView::CraftingView(ContainerMenu& menu) : menu(&menu) {}

int CraftingView::width() const { return menu->layout().craftWidth; }
int CraftingView::height() const { return menu->layout().craftHeight; }
int CraftingView::cellCount() const { return menu->hasCrafting() ? menu->layout().craftCells() : 0; }

MetaStack& CraftingView::input(int x, int y) { return inputAt(y * width() + x); }
const MetaStack& CraftingView::input(int x, int y) const { return inputAt(y * width() + x); }

MetaStack& CraftingView::inputAt(int cell) {
    return menu->stackAt(menu->layout().craftFirst + cell);
}

const MetaStack& CraftingView::inputAt(int cell) const {
    return menu->stackAt(menu->layout().craftFirst + cell);
}

CraftGrid CraftingView::gridOfIds() const {
    CraftGrid grid{};
    int w = width();
    for (int cell = 0; cell < cellCount(); ++cell) {
        const MetaStack& stack = inputAt(cell);
        if (stack.empty()) continue;
        int row = cell / w;
        int column = cell % w;
        grid[static_cast<std::size_t>(row * craftGridWidth + column)] = stack.item;
    }
    return grid;
}

void CraftingView::refresh() {
    int resultSlot = menu->layout().resultSlot;
    if (resultSlot < 0) return;
    MetaStack& target = menu->stackAt(resultSlot);
    if (!menu->hasCrafting()) {
        target.clear();
        return;
    }
    CraftResult match = menu->registry()->craftMatch(gridOfIds());
    if (!match.valid()) {
        target.clear();
        return;
    }
    target = makeStack(match.item, match.count);
}

const MetaStack& CraftingView::result() const {
    return menu->resultStack();
}

bool CraftingView::hasResult() const { return !result().empty(); }

