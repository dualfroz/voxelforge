#include "vf/ItemStack.h"

#include <algorithm>

namespace vf {

namespace {

int16_t enchantCode(EnchantKind kind) { return static_cast<int16_t>(kind); }

bool validEnchantKind(int32_t raw) {
    return raw >= 0 && raw < static_cast<int32_t>(EnchantKind::Count);
}

bool validItemId(int32_t raw) { return raw > 0 && raw < static_cast<int32_t>(ItemId::Count); }

const NbtTag* findTyped(const NbtTag& compound, const std::string& key, NbtType type) {
    const NbtTag* found = compound.find(key);
    if (found == nullptr || found->kind() != type) return nullptr;
    return found;
}

int32_t intField(const NbtTag& compound, const std::string& key, NbtType type, int32_t fallback) {
    const NbtTag* found = findTyped(compound, key, type);
    return found == nullptr ? fallback : found->asInt();
}

} // namespace

float Durability::fraction() const {
    if (max <= 0) return 1.0f;
    int left = remaining();
    return static_cast<float>(left) / static_cast<float>(max);
}

bool sameEnchants(const EnchantSet& a, const EnchantSet& b) {
    if (a.entries.size() != b.entries.size()) return false;
    for (const Enchantment& entry : a.entries) {
        if (b.levelOf(entry.kind) != entry.level) return false;
    }
    return true;
}

bool StackMeta::pristine() const {
    return durability.used == 0 && enchants.empty() && customName.empty() && !unbreakable &&
           repairCost == 0;
}

bool StackMeta::operator==(const StackMeta& other) const {
    return durability == other.durability && unbreakable == other.unbreakable &&
           repairCost == other.repairCost && customName == other.customName &&
           sameEnchants(enchants, other.enchants);
}

int stackLimitFor(ItemId item) {
    if (item == ItemId::None) return 0;
    return maxStackSize(item);
}

int MetaStack::maxCount() const {
    if (item == ItemId::None) return 0;
    if (meta.durability.damageable() || !meta.enchants.empty() || meta.unbreakable ||
        !meta.customName.empty()) {
        return 1;
    }
    return stackLimitFor(item);
}

int MetaStack::room() const {
    if (item == ItemId::None) return 0;
    int left = maxCount() - count;
    return left > 0 ? left : 0;
}

void MetaStack::clear() {
    item = ItemId::None;
    count = 0;
    meta = StackMeta{};
}

MetaStack makeStack(ItemId item, int count) {
    MetaStack stack;
    if (item == ItemId::None || count <= 0) return stack;
    stack.item = item;
    stack.count = count;
    if (isTool(item)) stack.meta.durability = {0, toolMaxDurability(item)};
    int cap = stack.maxCount();
    if (stack.count > cap) stack.count = cap;
    return stack;
}

MetaStack makeTool(ItemId toolItem) {
    MetaStack stack;
    if (!isTool(toolItem)) return stack;
    stack.item = toolItem;
    stack.count = 1;
    stack.meta.durability = {0, toolMaxDurability(toolItem)};
    return stack;
}

MetaStack makeNamedStack(ItemId item, int count, const std::string& name) {
    MetaStack stack = makeStack(item, count);
    if (stack.empty()) return stack;
    stack.meta.customName = name;
    int cap = stack.maxCount();
    if (stack.count > cap) stack.count = cap;
    return stack;
}

MetaStack makeEnchantedTool(ItemId toolItem, const EnchantSet& enchants) {
    MetaStack stack = makeTool(toolItem);
    if (stack.empty()) return stack;
    stack.meta.enchants = enchants;
    return stack;
}

bool stacksMergeable(const MetaStack& a, const MetaStack& b) {
    if (a.empty() || b.empty()) return false;
    if (a.item != b.item) return false;
    return a.meta == b.meta;
}

bool stacksIdentical(const MetaStack& a, const MetaStack& b) {
    if (a.empty() && b.empty()) return true;
    return stacksMergeable(a, b) && a.count == b.count;
}

StackTransfer transferStack(MetaStack& from, MetaStack& into, int amount) {
    StackTransfer transfer;
    if (from.empty() || amount <= 0) {
        transfer.sourceEmptied = from.empty();
        return transfer;
    }

    if (into.empty()) {
        into.item = from.item;
        into.meta = from.meta;
        into.count = 0;
    } else if (!stacksMergeable(from, into)) {
        transfer.incompatible = true;
        return transfer;
    }

    int movable = std::min({amount, from.count, into.room()});
    if (movable <= 0) {
        transfer.destinationFull = true;
        return transfer;
    }

    into.count += movable;
    from.count -= movable;
    transfer.moved = movable;
    if (from.count <= 0) {
        from.clear();
        transfer.sourceEmptied = true;
    }
    transfer.destinationFull = into.full();
    return transfer;
}

StackTransfer transferAll(MetaStack& from, MetaStack& into) {
    return transferStack(from, into, from.count);
}

MetaStack splitOff(MetaStack& stack, int amount) {
    MetaStack taken;
    if (stack.empty() || amount <= 0) return taken;
    int moved = std::min(amount, stack.count);
    taken.item = stack.item;
    taken.meta = stack.meta;
    taken.count = moved;
    stack.count -= moved;
    if (stack.count <= 0) stack.clear();
    return taken;
}

MetaStack splitHalf(MetaStack& stack) {
    if (stack.empty()) return MetaStack{};
    return splitOff(stack, (stack.count + 1) / 2);
}

int defragmentStacks(std::vector<MetaStack>& stacks) {
    int freedBefore = 0;
    for (const MetaStack& stack : stacks) {
        if (stack.empty()) ++freedBefore;
    }

    for (std::size_t target = 0; target < stacks.size(); ++target) {
        if (stacks[target].empty() || stacks[target].full()) continue;
        for (std::size_t source = target + 1; source < stacks.size(); ++source) {
            if (stacks[source].empty()) continue;
            if (!stacksMergeable(stacks[target], stacks[source])) continue;
            transferAll(stacks[source], stacks[target]);
            if (stacks[target].full()) break;
        }
    }

    std::stable_partition(stacks.begin(), stacks.end(),
                          [](const MetaStack& stack) { return !stack.empty(); });

    int freedAfter = 0;
    for (const MetaStack& stack : stacks) {
        if (stack.empty()) ++freedAfter;
    }
    return freedAfter - freedBefore;
}

int totalCountOf(const std::vector<MetaStack>& stacks, ItemId item) {
    int total = 0;
    for (const MetaStack& stack : stacks) {
        if (!stack.empty() && stack.item == item) total += stack.count;
    }
    return total;
}

bool stackOrderLess(const MetaStack& a, const MetaStack& b) {
    if (a.empty() != b.empty()) return b.empty();
    if (a.empty()) return false;
    if (a.item != b.item) return a.item < b.item;
    bool aWorn = a.meta.durability.used > 0;
    bool bWorn = b.meta.durability.used > 0;
    if (aWorn != bWorn) return bWorn;
    if (a.count != b.count) return a.count > b.count;
    return a.meta.customName < b.meta.customName;
}

void sortStacks(std::vector<MetaStack>& stacks) {
    std::stable_sort(stacks.begin(), stacks.end(), stackOrderLess);
}

uint32_t stackRandom(uint32_t& state) {
    if (state == 0) state = 0x9e3779b9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool unbreakingSkipsWear(int unbreakingLevel, bool armorPiece, uint32_t& rng) {
    if (unbreakingLevel <= 0) return false;
    float levels = static_cast<float>(unbreakingLevel + 1);
    float ignoreChance = armorPiece ? 0.6f + 0.4f / levels
                                    : static_cast<float>(unbreakingLevel) / levels;
    float roll = static_cast<float>(stackRandom(rng)) / 4294967295.0f;
    return roll < ignoreChance;
}

WearResult damageStack(MetaStack& stack, int amount, uint32_t& rng) {
    if (stack.empty() || !stack.meta.durability.damageable()) return WearResult::NotDamageable;
    if (stack.meta.unbreakable || amount <= 0) return WearResult::Absorbed;

    int unbreaking = stack.meta.enchants.levelOf(EnchantKind::Unbreaking);
    int applied = 0;
    for (int i = 0; i < amount; ++i) {
        if (unbreakingSkipsWear(unbreaking, false, rng)) continue;
        ++applied;
    }
    if (applied == 0) return WearResult::Absorbed;

    stack.meta.durability.used += applied;
    if (!stack.meta.durability.broken()) return WearResult::Damaged;

    if (stack.meta.enchants.has(EnchantKind::Mending)) {
        stack.meta.durability.used = stack.meta.durability.max - 1;
        return WearResult::Survived;
    }
    stack.clear();
    return WearResult::Broke;
}

int mendingRepair(MetaStack& stack, int experience) {
    if (stack.empty() || experience <= 0) return 0;
    if (!stack.meta.durability.damageable()) return 0;
    if (!stack.meta.enchants.has(EnchantKind::Mending)) return 0;
    int repair = std::min(experience * 2, stack.meta.durability.used);
    stack.meta.durability.used -= repair;
    return repair;
}

MetaStack craftingRemainder(const MetaStack& ingredient) {
    if (ingredient.empty()) return MetaStack{};
    if (!ingredient.meta.durability.damageable()) return MetaStack{};

    MetaStack leftover = ingredient;
    leftover.count = 1;
    if (leftover.meta.unbreakable) return leftover;
    leftover.meta.durability.used += 1;
    if (leftover.meta.durability.broken()) return MetaStack{};
    return leftover;
}

ItemSlot toItemSlot(const MetaStack& stack) {
    if (stack.empty()) return ItemSlot{};
    ItemSlot slot;
    slot.item = stack.item;
    slot.count = stack.count;
    slot.durability = stack.meta.durability.remaining();
    return slot;
}

MetaStack fromItemSlot(const ItemSlot& slot) {
    MetaStack stack;
    if (slot.empty()) return stack;
    stack.item = slot.item;
    stack.count = slot.count;
    int max = isTool(slot.item) ? toolMaxDurability(slot.item) : 0;
    if (max > 0) {
        int left = std::min(std::max(slot.durability, 0), max);
        stack.meta.durability = {max - left, max};
    }
    return stack;
}

NbtTag stackToNbt(const MetaStack& stack) {
    NbtTag tag = NbtTag::makeCompound();
    if (stack.empty()) {
        tag.put("Id", NbtTag::makeShort(0));
        return tag;
    }

    tag.put("Id", NbtTag::makeShort(static_cast<int16_t>(stack.item)));
    tag.put("Count", NbtTag::makeShort(static_cast<int16_t>(stack.count)));
    tag.put("DamageUsed", NbtTag::makeInt(stack.meta.durability.used));
    tag.put("DamageMax", NbtTag::makeInt(stack.meta.durability.max));
    tag.put("Unbreakable", NbtTag::makeByte(stack.meta.unbreakable ? 1 : 0));
    tag.put("RepairCost", NbtTag::makeInt(stack.meta.repairCost));
    if (!stack.meta.customName.empty()) {
        tag.put("Name", NbtTag::makeString(stack.meta.customName));
    }
    if (!stack.meta.enchants.empty()) {
        NbtTag list = NbtTag::makeList(NbtType::Compound);
        for (const Enchantment& entry : stack.meta.enchants.entries) {
            NbtTag row = NbtTag::makeCompound();
            row.put("Kind", NbtTag::makeShort(enchantCode(entry.kind)));
            row.put("Level", NbtTag::makeShort(static_cast<int16_t>(entry.level)));
            list.listAppend(row);
        }
        tag.put("Enchants", list);
    }
    return tag;
}

bool stackFromNbt(const NbtTag& tag, MetaStack& out) {
    out = MetaStack{};
    if (tag.kind() != NbtType::Compound) return false;

    int32_t rawItem = intField(tag, "Id", NbtType::Short, 0);
    if (rawItem == 0) return true;
    if (!validItemId(rawItem)) return false;

    int32_t count = intField(tag, "Count", NbtType::Short, 0);
    if (count <= 0) return true;

    out.item = static_cast<ItemId>(rawItem);
    out.meta.durability.used = intField(tag, "DamageUsed", NbtType::Int, 0);
    out.meta.durability.max = intField(tag, "DamageMax", NbtType::Int, 0);
    out.meta.unbreakable = intField(tag, "Unbreakable", NbtType::Byte, 0) != 0;
    out.meta.repairCost = intField(tag, "RepairCost", NbtType::Int, 0);

    const NbtTag* name = findTyped(tag, "Name", NbtType::String);
    if (name != nullptr) out.meta.customName = name->asString();

    const NbtTag* enchants = findTyped(tag, "Enchants", NbtType::List);
    if (enchants != nullptr && enchants->listElementType() == NbtType::Compound) {
        for (const NbtTag& row : enchants->asList()) {
            int32_t kind = intField(row, "Kind", NbtType::Short, -1);
            int32_t level = intField(row, "Level", NbtType::Short, 0);
            if (!validEnchantKind(kind) || level <= 0) continue;
            out.meta.enchants.entries.push_back({static_cast<EnchantKind>(kind), level});
        }
    }

    out.count = std::min(count, out.maxCount());
    if (out.count <= 0) out.clear();
    return true;
}

NbtTag stackListToNbt(const std::vector<MetaStack>& stacks) {
    NbtTag list = NbtTag::makeList(NbtType::Compound);
    for (const MetaStack& stack : stacks) list.listAppend(stackToNbt(stack));
    return list;
}

bool stackListFromNbt(const NbtTag& tag, std::vector<MetaStack>& out) {
    out.clear();
    if (tag.kind() != NbtType::List || tag.listElementType() != NbtType::Compound) return false;
    out.reserve(tag.asList().size());
    for (const NbtTag& row : tag.asList()) {
        MetaStack stack;
        if (!stackFromNbt(row, stack)) return false;
        out.push_back(stack);
    }
    return true;
}

} // namespace vf
