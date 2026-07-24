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

int CraftingView::maxCrafts() const {
    if (!hasResult()) return 0;
    int rounds = -1;
    for (int cell = 0; cell < cellCount(); ++cell) {
        const MetaStack& stack = inputAt(cell);
        if (stack.empty()) continue;
        int available = stack.meta.durability.damageable()
                            ? stack.meta.durability.remaining()
                            : stack.count;
        if (rounds < 0 || available < rounds) rounds = available;
    }
    return rounds < 0 ? 0 : rounds;
}

void CraftingView::consumeOneRound() {
    for (int cell = 0; cell < cellCount(); ++cell) {
        MetaStack& stack = inputAt(cell);
        if (stack.empty()) continue;
        MetaStack leftover = craftingRemainder(stack);
        if (!leftover.empty()) {
            stack = leftover;
            continue;
        }
        stack.count -= 1;
        if (stack.count <= 0) stack.clear();
    }
}

MetaStack CraftingView::takeResult() {
    if (!hasResult()) return MetaStack{};
    MetaStack crafted = menu->stackAt(menu->layout().resultSlot);
    consumeOneRound();
    refresh();
    return crafted;
}

int CraftingView::craftAll(int limit) {
    int crafted = 0;
    std::vector<int> targets = menu->playerSectionSlots();
    while (crafted < limit && hasResult()) {
        MetaStack batch = takeResult();
        if (batch.empty()) break;
        ++crafted;
        menu->distributeInto(targets, batch);
        if (!batch.empty()) {
            // No room left: the unplaced remainder goes back to the result slot
            // and crafting stops, exactly as a full inventory does in game.
            menu->stackAt(menu->layout().resultSlot) = batch;
            break;
        }
    }
    return crafted;
}

ContainerMenu::ContainerMenu(MenuKind kind, const CraftingRegistry* registry)
    : menuKind(kind), craftingRegistry(registry) {
    buildSlots();
}

void ContainerMenu::buildSlots() { slots = buildLayout(menuKind, menuLayout); }

bool ContainerMenu::canPlace(int index, const MetaStack& incoming) const {
    if (!validIndex(index) || incoming.empty()) return false;
    const MenuSlot& target = slot(index);
    if (target.takeOnly) return false;
    return target.filter.accepts(incoming, craftingRegistry);
}

bool ContainerMenu::canTake(int index) const {
    return validIndex(index) && !stackAt(index).empty();
}

int ContainerMenu::insertAt(int index, MetaStack& incoming) {
    if (!canPlace(index, incoming)) return 0;
    return slot(index).cell.insert(incoming);
}

MetaStack ContainerMenu::takeFrom(int index, int amount) {
    if (!validIndex(index)) return MetaStack{};
    return slot(index).cell.take(amount);
}

std::vector<int> ContainerMenu::slotsWithRole(MenuSlot::Role role) const {
    std::vector<int> found;
    for (int i = 0; i < slotCount(); ++i) {
        if (slot(i).role == role) found.push_back(i);
    }
    return found;
}

bool ContainerMenu::isPlayerSection(int index) const {
    MenuSlot::Role role = slot(index).role;
    return role == MenuSlot::Role::Inventory || role == MenuSlot::Role::Hotbar ||
           role == MenuSlot::Role::OffHand;
}

std::vector<int> ContainerMenu::playerSectionSlots() const {
    std::vector<int> found;
    for (int i = 0; i < slotCount(); ++i) {
        if (isPlayerSection(i)) found.push_back(i);
    }
    return found;
}

std::vector<int> ContainerMenu::containerSectionSlots() const {
    std::vector<int> found;
    for (int i = 0; i < slotCount(); ++i) {
        if (!isPlayerSection(i)) found.push_back(i);
    }
    return found;
}

int ContainerMenu::distributeInto(const std::vector<int>& targets, MetaStack& incoming) {
    int moved = 0;
    for (int index : targets) {
        if (incoming.empty()) break;
        if (stackAt(index).empty()) continue;
        moved += insertAt(index, incoming);
    }
    for (int index : targets) {
        if (incoming.empty()) break;
        if (!stackAt(index).empty()) continue;
        moved += insertAt(index, incoming);
    }
    return moved;
}

int ContainerMenu::totalOf(ItemId item) const {
    int total = 0;
    for (const MenuSlot& menuSlot : slots) {
        if (!menuSlot.cell.stack.empty() && menuSlot.cell.stack.item == item) {
            total += menuSlot.cell.stack.count;
        }
    }
    return total;
}

int ContainerMenu::totalItems() const {
    int total = 0;
    for (const MenuSlot& menuSlot : slots) total += menuSlot.cell.stack.count;
    return total;
}

int ContainerMenu::usedSlots() const {
    int used = 0;
    for (const MenuSlot& menuSlot : slots) {
        if (!menuSlot.cell.stack.empty()) ++used;
    }
    return used;
}

void ContainerMenu::bindContainer(Container& container) {
    bound = &container;
    std::vector<int> section = containerSectionSlots();
    int limit = std::min(static_cast<int>(section.size()), container.size());
    for (int i = 0; i < limit; ++i) slot(section[static_cast<std::size_t>(i)]).backingIndex = i;
    syncFromContainer();
}

void ContainerMenu::syncFromContainer() {
    if (bound == nullptr) return;
    for (MenuSlot& menuSlot : slots) {
        if (menuSlot.backingIndex < 0) continue;
        menuSlot.cell.stack = fromItemSlot(bound->slot(menuSlot.backingIndex));
    }
    refreshResult();
}

void ContainerMenu::syncToContainer() {
    if (bound == nullptr) return;
    for (const MenuSlot& menuSlot : slots) {
        if (menuSlot.backingIndex < 0) continue;
        bound->setSlot(menuSlot.backingIndex, toItemSlot(menuSlot.cell.stack));
    }
}

void ContainerMenu::unbind() {
    syncToContainer();
    for (MenuSlot& menuSlot : slots) menuSlot.backingIndex = -1;
    bound = nullptr;
}

CraftingView ContainerMenu::crafting() { return CraftingView(*this); }

const MetaStack& ContainerMenu::resultStack() const {
    if (menuLayout.resultSlot < 0) return emptyStack();
    return stackAt(menuLayout.resultSlot);
}

void ContainerMenu::refreshResult() {
    if (menuLayout.resultSlot < 0) return;
    CraftingView view(*this);
    view.refresh();
}

NbtTag ContainerMenu::toNbt() const {
    NbtTag tag = NbtTag::makeCompound();
    tag.put("Kind", NbtTag::makeShort(static_cast<int16_t>(menuKind)));
    tag.put("Creative", NbtTag::makeByte(creative ? 1 : 0));
    NbtTag list = NbtTag::makeList(NbtType::Compound);
    for (const MenuSlot& menuSlot : slots) list.listAppend(stackToNbt(menuSlot.cell.stack));
    tag.put("Slots", list);
    return tag;
}

bool ContainerMenu::fromNbt(const NbtTag& tag) {
    if (tag.kind() != NbtType::Compound) return false;
    const NbtTag* kindTag = tag.find("Kind");
    if (kindTag == nullptr || kindTag->kind() != NbtType::Short) return false;
    int32_t rawKind = kindTag->asInt();
    if (rawKind < 0 || rawKind >= static_cast<int32_t>(MenuKind::Count)) return false;

    const NbtTag* list = tag.find("Slots");
    if (list == nullptr || list->kind() != NbtType::List) return false;

    menuKind = static_cast<MenuKind>(rawKind);
    buildSlots();
    bound = nullptr;

    const NbtTag* creativeTag = tag.find("Creative");
    creative = creativeTag != nullptr && creativeTag->kind() == NbtType::Byte &&
               creativeTag->asByte() != 0;

    const NbtList& rows = list->asList();
    if (static_cast<int>(rows.size()) != slotCount()) return false;
    for (int i = 0; i < slotCount(); ++i) {
        if (!stackFromNbt(rows[static_cast<std::size_t>(i)], slot(i).cell.stack)) return false;
    }
    return true;
}

} // namespace vf
