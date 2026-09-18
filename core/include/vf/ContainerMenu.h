#pragma once

#include <cstdint>
#include <vector>

#include "Container.h"
#include "Crafting.h"
#include "ItemStack.h"

namespace vf {

struct SlotFilter {
    enum class Kind : uint8_t {
        Any,
        Nothing,
        Fuel,
        Smeltable,
        Lapis,
        Enchantable,
        Repairable,
        BrewIngredient,
        Count
    };

    Kind kind = Kind::Any;

    bool accepts(const MetaStack& stack, const CraftingRegistry* registry) const;
};

const char* slotFilterKindName(SlotFilter::Kind kind);

struct StackSlot {
    MetaStack stack;
    int capacity = 0;

    bool empty() const { return stack.empty(); }
    int limitFor(const MetaStack& candidate) const;
    int room(const MetaStack& candidate) const;

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
    enum class Role : uint8_t {
        Storage,
        Inventory,
        Hotbar,
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
    bool takeOnly = false;
    int backingIndex = -1;
};

const char* menuSlotRoleName(MenuSlot::Role role);

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

    void refresh();

    const MetaStack& result() const;
    bool hasResult() const;

    int maxCrafts() const;

    MetaStack takeResult();

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
    bool canTake(int index) const;

    int insertAt(int index, MetaStack& incoming);
    MetaStack takeFrom(int index, int amount);

    std::vector<int> slotsWithRole(MenuSlot::Role role) const;
    std::vector<int> playerSectionSlots() const;
    std::vector<int> containerSectionSlots() const;
    bool isPlayerSection(int index) const;

    int distributeInto(const std::vector<int>& targets, MetaStack& incoming);

    int totalOf(ItemId item) const;
    int totalItems() const;
    int usedSlots() const;

    void bindContainer(Container& container);
    void syncFromContainer();
    void syncToContainer();
    bool hasContainer() const { return bound != nullptr; }
    void unbind();

    void setCreativeMode(bool enabled) { creative = enabled; }
    bool creativeMode() const { return creative; }

    bool hasCrafting() const { return menuLayout.hasCrafting() && craftingRegistry != nullptr; }
    CraftingView crafting();
    const MetaStack& resultStack() const;

    void refreshResult();

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

}
