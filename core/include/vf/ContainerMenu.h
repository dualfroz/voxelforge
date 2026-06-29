#pragma once

#include <cstdint>
#include <vector>

#include "Container.h"
#include "Crafting.h"
#include "ItemStack.h"

namespace vf {

// A menu is the screen an open container puts in front of the player. It does
// not replace Container: a Container is still the storage that lives in the
// world and that hoppers, droppers and comparators read. The menu binds one for
// the duration of the screen, adds the player's own sections next to it, and
// gives every cell a role and an admission rule.

// What a slot is willing to accept. The result of a craft, a smelt or a trade
// takes Nothing, which is what makes it take-only.
struct SlotFilter {
    enum class Kind : uint8_t {
        Any,
        Nothing,
        Fuel,           // furnace fuel, per Crafting.h isFuel
        Smeltable,      // has a smelting recipe in the bound registry
        Lapis,          // enchanting table payment
        Enchantable,    // enchantabilityOf > 0
        Repairable,     // anvil input: tools carry durability, so they repair
        BrewIngredient, // the brewing ingredients that exist as items here
        Count
    };

    Kind kind = Kind::Any;

    // `registry` is only consulted by Smeltable; every other kind answers from
    // the item tables alone and accepts a null registry.
    bool accepts(const MetaStack& stack, const CraftingRegistry* registry) const;
};

const char* slotFilterKindName(SlotFilter::Kind kind);

// One storage cell: a stack plus a cap. `capacity` of zero defers to the item's
// own limit, anything else clamps below it (a brewing bottle holds exactly one).
struct StackSlot {
    MetaStack stack;
    int capacity = 0;

    bool empty() const { return stack.empty(); }
    int limitFor(const MetaStack& candidate) const;
    int room(const MetaStack& candidate) const;

    // Merges what fits from `incoming`, leaving the rest in it. Returns moved.
    int insert(MetaStack& incoming);
    MetaStack take(int amount);
    MetaStack takeAll();
    MetaStack swapWith(const MetaStack& incoming);
};

enum class MenuKind : uint8_t {
    PlayerInventory,
    CraftingTable,
    Chest,
    LargeChest,
    Furnace,
    BrewingStand,
    EnchantingTable,
    Anvil,
    Villager,
    Hopper,
    Count
};

const char* menuKindName(MenuKind kind);

struct MenuSlot {
    // Which part of the screen the slot belongs to. Shift-clicking moves items
    // between the container half and the player half, so the roles have to be
    // distinguishable at runtime, not just visually.
    enum class Role : uint8_t {
        Storage,        // the bound container, or the menu's own storage
        Inventory,      // the player's three rows
        Hotbar,         // the player's bottom row
        CraftInput,
        CraftResult,
        FuelInput,
        SmeltInput,
        SmeltOutput,
        BrewBottle,
        BrewIngredient,
        EnchantItem,
        EnchantLapis,
        AnvilInput,
        AnvilResult,
        TradeInput,
        TradeResult,
        OffHand,
        Count
    };

    StackSlot cell;
    Role role = Role::Storage;
    SlotFilter filter;
    // A take-only slot never accepts a placement, however empty it is.
    bool takeOnly = false;
    // Index into the bound Container, or -1 when the menu owns the stack.
    int backingIndex = -1;
};

const char* menuSlotRoleName(MenuSlot::Role role);

// Where each section starts and how long it is, so callers can iterate a
// section without knowing the slot order by heart.
struct MenuLayout {
    MenuKind kind = MenuKind::Chest;

    int storageFirst = -1;
    int storageCount = 0;
    int inventoryFirst = -1;
    int inventoryCount = 0;
    int hotbarFirst = -1;
    int hotbarCount = 0;
    int craftFirst = -1;
    int craftWidth = 0;
    int craftHeight = 0;
    int resultSlot = -1;
    int offHandSlot = -1;

    int totalSlots = 0;

    int craftCells() const { return craftWidth * craftHeight; }
    bool hasCrafting() const { return craftFirst >= 0 && craftCells() > 0; }
};

MenuLayout layoutFor(MenuKind kind);

constexpr int playerInventoryRows = 3;
constexpr int playerInventoryColumns = 9;
constexpr int playerInventorySlots = playerInventoryRows * playerInventoryColumns;
constexpr int playerHotbarSlots = 9;

class ContainerMenu;

// The crafting half of a menu, wired to the existing CraftingRegistry. It owns
// no stacks and no lifetime: it reads and writes the menu's craft slots in
// place, so the grid the player sees and the grid the recipe matcher sees are
// the same cells. A menu without a crafting section hands out a view whose
// cellCount is zero and whose operations therefore do nothing.
class CraftingView {
public:
    explicit CraftingView(ContainerMenu& menu);

    int width() const;
    int height() const;
    int cellCount() const;

    MetaStack& input(int x, int y);
    const MetaStack& input(int x, int y) const;
    MetaStack& inputAt(int cell);
    const MetaStack& inputAt(int cell) const;

    // Recomputes the result slot from the current grid. Cheap enough to call
    // after every single change, which is exactly what the menu does.
    void refresh();

    const MetaStack& result() const;
    bool hasResult() const;

    // How many times the current grid could be crafted before running dry.
    int maxCrafts() const;

    // Takes the result once. Each input loses one item; a damageable ingredient
    // is not eaten but comes back one durability point worse, and a broken one
    // vanishes. Returns the crafted stack, empty when there was no recipe.
    MetaStack takeResult();

    // Shift-clicking the result: crafts until the grid runs out or the player
    // has no room, pushing every batch into the player's sections.
    int craftAll(int limit);

private:
    void consumeOneRound();
    CraftGrid gridOfIds() const;

    ContainerMenu* menu;
};

class ContainerMenu {
public:
    explicit ContainerMenu(MenuKind kind, const CraftingRegistry* registry = nullptr);

    MenuKind kind() const { return menuKind; }
    const MenuLayout& layout() const { return menuLayout; }
    const CraftingRegistry* registry() const { return craftingRegistry; }

    int slotCount() const { return static_cast<int>(slots.size()); }
    MenuSlot& slot(int index) { return slots[static_cast<std::size_t>(index)]; }
    const MenuSlot& slot(int index) const { return slots[static_cast<std::size_t>(index)]; }
    MetaStack& stackAt(int index) { return slot(index).cell.stack; }
    const MetaStack& stackAt(int index) const { return slot(index).cell.stack; }

    bool validIndex(int index) const { return index >= 0 && index < slotCount(); }
    bool canPlace(int index, const MetaStack& incoming) const;
    // True when the slot's contents may leave it. Every slot allows this; the
    // asymmetry is deliberate, a result slot gives but never takes.
    bool canTake(int index) const;

    // Puts what fits into the slot, honouring the filter and the cap.
    int insertAt(int index, MetaStack& incoming);
    MetaStack takeFrom(int index, int amount);

    // Section walkers used by shift-click and by the double-click gather.
    std::vector<int> slotsWithRole(MenuSlot::Role role) const;
    std::vector<int> playerSectionSlots() const;
    std::vector<int> containerSectionSlots() const;
    bool isPlayerSection(int index) const;

    // Fills matching stacks first and only then empty slots, which is the order
    // a shift-click has to follow to feel right.
    int distributeInto(const std::vector<int>& targets, MetaStack& incoming);

    int totalOf(ItemId item) const;
    int totalItems() const;
    int usedSlots() const;

    // Binds a world Container as the storage section. The menu copies the
    // container in, and syncToContainer writes it back. ItemSlot has no room
    // for enchantments or names, so anything richer than durability survives
    // only inside the menu (or through stackListToNbt).
    void bindContainer(Container& container);
    void syncFromContainer();
    void syncToContainer();
    bool hasContainer() const { return bound != nullptr; }
    void unbind();

    // Creative mode is the one thing allowed to conjure items: a middle click
    // clones a stack instead of doing nothing.
    void setCreativeMode(bool enabled) { creative = enabled; }
    bool creativeMode() const { return creative; }

    bool hasCrafting() const { return menuLayout.hasCrafting() && craftingRegistry != nullptr; }
    // A two-pointer handle, cheap to hand out by value and never stale.
    CraftingView crafting();
    const MetaStack& resultStack() const;

    // Called by CraftingView and by every slot action that touched a craft
    // input, so the result slot is never stale.
    void refreshResult();

    // Persists every slot losslessly, unlike the Container projection.
    NbtTag toNbt() const;
    bool fromNbt(const NbtTag& tag);

private:
    void buildSlots();

    MenuKind menuKind;
    MenuLayout menuLayout;
    const CraftingRegistry* craftingRegistry = nullptr;
    std::vector<MenuSlot> slots;
    Container* bound = nullptr;
    bool creative = false;
};

} // namespace vf
