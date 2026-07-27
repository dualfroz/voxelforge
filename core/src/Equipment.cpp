#include "vf/Equipment.h"

#include <algorithm>

namespace vf {

namespace {

int slotOffset(EquipSlot slot) { return static_cast<int>(slot); }

int handIndex(EquipSlot slot) { return slot == EquipSlot::OffHand ? 1 : 0; }

const MetaStack& noStack() {
    static const MetaStack nothing;
    return nothing;
}

const Armor& noArmor() {
    static const Armor nothing;
    return nothing;
}

float swordDamage(ToolTier tier) {
    switch (tier) {
        case ToolTier::Wood:    return 4.0f;
        case ToolTier::Stone:   return 5.0f;
        case ToolTier::Iron:    return 6.0f;
        case ToolTier::Gold:    return 4.0f;
        case ToolTier::Diamond: return 7.0f;
        case ToolTier::None:    break;
    }
    return unarmedAttackDamage;
}

float axeDamage(ToolTier tier) {
    switch (tier) {
        case ToolTier::Wood:    return 7.0f;
        case ToolTier::Stone:   return 9.0f;
        case ToolTier::Iron:    return 9.0f;
        case ToolTier::Gold:    return 7.0f;
        case ToolTier::Diamond: return 9.0f;
        case ToolTier::None:    break;
    }
    return unarmedAttackDamage;
}

float pickaxeDamage(ToolTier tier) {
    switch (tier) {
        case ToolTier::Wood:    return 2.0f;
        case ToolTier::Stone:   return 3.0f;
        case ToolTier::Iron:    return 4.0f;
        case ToolTier::Gold:    return 2.0f;
        case ToolTier::Diamond: return 5.0f;
        case ToolTier::None:    break;
    }
    return unarmedAttackDamage;
}

float axeSpeed(ToolTier tier) {
    switch (tier) {
        case ToolTier::Wood:    return 0.8f;
        case ToolTier::Stone:   return 0.8f;
        case ToolTier::Iron:    return 0.9f;
        case ToolTier::Gold:    return 1.0f;
        case ToolTier::Diamond: return 1.0f;
        case ToolTier::None:    break;
    }
    return unarmedAttackSpeed;
}

float hoeSpeed(ToolTier tier) {
    switch (tier) {
        case ToolTier::Wood:    return 1.0f;
        case ToolTier::Stone:   return 2.0f;
        case ToolTier::Iron:    return 3.0f;
        case ToolTier::Gold:    return 1.0f;
        case ToolTier::Diamond: return 4.0f;
        case ToolTier::None:    break;
    }
    return unarmedAttackSpeed;
}

void addModifier(EquipmentStats& stats, AttributeModifier::Attribute attribute,
                 AttributeModifier::Operation operation, float amount, EquipSlot slot) {
    if (amount == 0.0f) return;
    AttributeModifier modifier;
    modifier.attribute = attribute;
    modifier.operation = operation;
    modifier.amount = amount;
    modifier.slot = slot;
    stats.modifiers.push_back(modifier);
}

} // namespace

const char* equipSlotName(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Head:     return "head";
        case EquipSlot::Chest:    return "chest";
        case EquipSlot::Legs:     return "legs";
        case EquipSlot::Feet:     return "feet";
        case EquipSlot::MainHand: return "mainHand";
        case EquipSlot::OffHand:  return "offHand";
        case EquipSlot::Count:    break;
    }
    return "unknown";
}

bool isArmorSlot(EquipSlot slot) { return slotOffset(slot) < static_cast<int>(EquipSlot::MainHand); }

bool isHandSlot(EquipSlot slot) {
    return slot == EquipSlot::MainHand || slot == EquipSlot::OffHand;
}

Armor::Slot armorSlotFor(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Head:  return Armor::Slot::Helmet;
        case EquipSlot::Chest: return Armor::Slot::Chestplate;
        case EquipSlot::Legs:  return Armor::Slot::Leggings;
        case EquipSlot::Feet:  return Armor::Slot::Boots;
        case EquipSlot::MainHand:
        case EquipSlot::OffHand:
        case EquipSlot::Count:
            break;
    }
    return Armor::Slot::Count;
}

EquipSlot equipSlotFor(Armor::Slot slot) {
    switch (slot) {
        case Armor::Slot::Helmet:     return EquipSlot::Head;
        case Armor::Slot::Chestplate: return EquipSlot::Chest;
        case Armor::Slot::Leggings:   return EquipSlot::Legs;
        case Armor::Slot::Boots:      return EquipSlot::Feet;
        case Armor::Slot::Count:      break;
    }
    return EquipSlot::Count;
}

const char* attributeName(AttributeModifier::Attribute attribute) {
    switch (attribute) {
        case AttributeModifier::Attribute::AttackDamage:   return "attackDamage";
        case AttributeModifier::Attribute::AttackSpeed:    return "attackSpeed";
        case AttributeModifier::Attribute::AttackReach:    return "attackReach";
        case AttributeModifier::Attribute::MiningSpeed:    return "miningSpeed";
        case AttributeModifier::Attribute::MovementSpeed:  return "movementSpeed";
        case AttributeModifier::Attribute::ArmorPoints:    return "armorPoints";
        case AttributeModifier::Attribute::ArmorToughness: return "armorToughness";
        case AttributeModifier::Attribute::Count:          break;
    }
    return "unknown";
}

float weaponAttackDamage(ItemId item) {
    if (!isTool(item)) return unarmedAttackDamage;
    const ItemInfo& info = itemInfo(item);
    switch (info.toolType) {
        case ToolType::Sword:   return swordDamage(info.toolTier);
        case ToolType::Axe:     return axeDamage(info.toolTier);
        case ToolType::Pickaxe: return pickaxeDamage(info.toolTier);
        case ToolType::Shovel:  return pickaxeDamage(info.toolTier) + 0.5f;
        case ToolType::Hoe:     return unarmedAttackDamage;
        case ToolType::None:    break;
    }
    return unarmedAttackDamage;
}

float weaponAttackSpeed(ItemId item) {
    if (!isTool(item)) return unarmedAttackSpeed;
    const ItemInfo& info = itemInfo(item);
    switch (info.toolType) {
        case ToolType::Sword:   return 1.6f;
        case ToolType::Axe:     return axeSpeed(info.toolTier);
        case ToolType::Pickaxe: return 1.2f;
        case ToolType::Shovel:  return 1.0f;
        case ToolType::Hoe:     return hoeSpeed(info.toolTier);
        case ToolType::None:    break;
    }
    return unarmedAttackSpeed;
}

float weaponAttackReach(ItemId item) {
    if (!isTool(item)) return unarmedAttackReach;
    switch (itemInfo(item).toolType) {
        case ToolType::Sword: return unarmedAttackReach + 0.5f;
        case ToolType::Axe:
        case ToolType::Hoe:   return unarmedAttackReach + 0.25f;
        case ToolType::Pickaxe:
        case ToolType::Shovel:
        case ToolType::None:  break;
    }
    return unarmedAttackReach;
}

float armorWeightPenalty(int armorPoints) {
    if (armorPoints <= 0) return 0.0f;
    float penalty = 0.01f * static_cast<float>(armorPoints);
    return penalty > 0.35f ? 0.35f : penalty;
}

void EquipmentStats::applyTo(CombatStats& stats) const {
    stats.armor = armor;
    stats.baseAttackDamage = attackDamage;
    stats.attackSpeed = attackSpeed;
}

bool Equipment::accepts(EquipSlot slot, const MetaStack& stack) const {
    if (stack.empty()) return false;
    if (!isHandSlot(slot)) return false;
    return true;
}

bool Equipment::equip(EquipSlot slot, const MetaStack& stack) {
    if (!accepts(slot, stack)) return false;
    hands[static_cast<std::size_t>(handIndex(slot))] = stack;
    return true;
}

bool Equipment::equipArmor(const Armor& piece) {
    return equipArmorAt(equipSlotFor(piece.slot), piece);
}

bool Equipment::equipArmorAt(EquipSlot slot, const Armor& piece) {
    if (!isArmorSlot(slot)) return false;
    if (piece.material == Armor::Material::None) return false;
    if (armorSlotFor(slot) != piece.slot) return false;
    worn.at(piece.slot) = piece;
    return true;
}

MetaStack Equipment::unequip(EquipSlot slot) {
    if (!isHandSlot(slot)) return MetaStack{};
    MetaStack removed = hands[static_cast<std::size_t>(handIndex(slot))];
    hands[static_cast<std::size_t>(handIndex(slot))].clear();
    return removed;
}

Armor Equipment::unequipArmor(EquipSlot slot) {
    if (!isArmorSlot(slot)) return Armor{};
    Armor& piece = worn.at(armorSlotFor(slot));
    Armor removed = piece;
    piece = Armor{};
    return removed;
}

const MetaStack& Equipment::hand(EquipSlot slot) const {
    if (!isHandSlot(slot)) return noStack();
    return hands[static_cast<std::size_t>(handIndex(slot))];
}

const Armor& Equipment::armorPiece(EquipSlot slot) const {
    Armor::Slot mapped = armorSlotFor(slot);
    if (mapped == Armor::Slot::Count) return noArmor();
    return worn.at(mapped);
}

EquipmentStats Equipment::stats() const {
    EquipmentStats stats;
    stats.armor = worn;
    stats.armorPoints = worn.totalPoints();
    stats.armorToughness = worn.totalToughness();

    for (int i = 0; i < static_cast<int>(Armor::Slot::Count); ++i) {
        Armor::Slot armorSlot = static_cast<Armor::Slot>(i);
        const Armor& piece = worn.at(armorSlot);
        if (!piece.worn()) continue;
        EquipSlot slot = equipSlotFor(armorSlot);
        addModifier(stats, AttributeModifier::Attribute::ArmorPoints,
                    AttributeModifier::Operation::Add, static_cast<float>(armorPoints(piece)), slot);
        addModifier(stats, AttributeModifier::Attribute::ArmorToughness,
                    AttributeModifier::Operation::Add, armorToughness(piece), slot);
    }

    float weight = armorWeightPenalty(stats.armorPoints);
    stats.movementSpeed = 1.0f - weight;
    addModifier(stats, AttributeModifier::Attribute::MovementSpeed,
                AttributeModifier::Operation::MultiplyTotal, -weight, EquipSlot::Chest);

    const MetaStack& weapon = hands[0];
    if (!weapon.empty()) {
        stats.weaponEnchants = weapon.meta.enchants;
        stats.attackDamage = weaponAttackDamage(weapon.item);
        stats.attackSpeed = weaponAttackSpeed(weapon.item);
        stats.attackReach = weaponAttackReach(weapon.item);
        stats.miningSpeed = toolTierSpeedMultiplier(itemInfo(weapon.item).toolTier);

        int efficiency = weapon.meta.enchants.levelOf(EnchantKind::Efficiency);
        if (efficiency > 0) {
            float bonus = static_cast<float>(efficiency * efficiency + 1);
            stats.miningSpeed += bonus;
            addModifier(stats, AttributeModifier::Attribute::MiningSpeed,
                        AttributeModifier::Operation::Add, bonus, EquipSlot::MainHand);
        }

        addModifier(stats, AttributeModifier::Attribute::AttackDamage,
                    AttributeModifier::Operation::Add, stats.attackDamage - unarmedAttackDamage,
                    EquipSlot::MainHand);
        addModifier(stats, AttributeModifier::Attribute::AttackSpeed,
                    AttributeModifier::Operation::Add, stats.attackSpeed - unarmedAttackSpeed,
                    EquipSlot::MainHand);
        addModifier(stats, AttributeModifier::Attribute::AttackReach,
                    AttributeModifier::Operation::Add, stats.attackReach - unarmedAttackReach,
                    EquipSlot::MainHand);
        addModifier(stats, AttributeModifier::Attribute::MiningSpeed,
                    AttributeModifier::Operation::Add,
                    toolTierSpeedMultiplier(itemInfo(weapon.item).toolTier) - 1.0f,
                    EquipSlot::MainHand);
    }

    return stats;
}

int Equipment::wearFromHit(float incomingDamage, uint32_t& rng) {
    return wearArmor(worn, incomingDamage, rng);
}

WearResult Equipment::useHeld(EquipSlot slot, int amount, uint32_t& rng) {
    if (!isHandSlot(slot)) return WearResult::NotDamageable;
    return damageStack(hands[static_cast<std::size_t>(handIndex(slot))], amount, rng);
}

int Equipment::absorbExperience(int experience) {
    if (experience <= 0) return 0;
    int budget = experience;
    int repaired = 0;

    for (Armor& piece : worn.pieces) {
        if (budget <= 0) break;
        if (!piece.worn() || !piece.enchants.has(EnchantKind::Mending)) continue;
        int missing = piece.maxDurability - piece.durability;
        if (missing <= 0) continue;
        int restore = std::min(budget * 2, missing);
        piece.durability += restore;
        repaired += restore;
        budget -= (restore + 1) / 2;
    }

    for (MetaStack& held : hands) {
        if (budget <= 0) break;
        int restore = mendingRepair(held, budget);
        if (restore <= 0) continue;
        repaired += restore;
        budget -= (restore + 1) / 2;
    }

    return repaired;
}

int Equipment::wornPieces() const {
    int count = 0;
    for (const Armor& piece : worn.pieces) {
        if (piece.worn()) ++count;
    }
    return count;
}

bool Equipment::empty() const {
    return wornPieces() == 0 && hands[0].empty() && hands[1].empty();
}

void loadHandsFromMenu(Equipment& gear, const ContainerMenu& menu, int selectedHotbarSlot) {
    const MenuLayout& layout = menu.layout();
    if (layout.hotbarFirst >= 0 && selectedHotbarSlot >= 0 &&
        selectedHotbarSlot < layout.hotbarCount) {
        gear.mainHandStack() = menu.stackAt(layout.hotbarFirst + selectedHotbarSlot);
    }
    if (layout.offHandSlot >= 0) {
        gear.offHandStack() = menu.stackAt(layout.offHandSlot);
    }
}

void storeHandsIntoMenu(Equipment& gear, ContainerMenu& menu, int selectedHotbarSlot) {
    const MenuLayout& layout = menu.layout();
    if (layout.hotbarFirst >= 0 && selectedHotbarSlot >= 0 &&
        selectedHotbarSlot < layout.hotbarCount) {
        menu.stackAt(layout.hotbarFirst + selectedHotbarSlot) = gear.mainHand();
    }
    if (layout.offHandSlot >= 0) {
        menu.stackAt(layout.offHandSlot) = gear.offHand();
    }
}

float equippedMeleeDamage(const Equipment& gear, const CombatStats& attacker,
                          const EffectSet& attackerEffects, bool undeadTarget,
                          bool arthropodTarget, bool critical) {
    EquipmentStats stats = gear.stats();
    CombatStats armed = attacker;
    armed.baseAttackDamage = stats.attackDamage;
    armed.attackSpeed = stats.attackSpeed;
    return meleeAttackDamage(armed, stats.weaponEnchants, attackerEffects, undeadTarget,
                             arthropodTarget, critical);
}

} // namespace vf
