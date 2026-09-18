#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ContainerMenu.h"
#include "Damage.h"
#include "ItemStack.h"

namespace vf {

enum class EquipSlot : uint8_t { Head, Chest, Legs, Feet, MainHand, OffHand, Count };

const char* equipSlotName(EquipSlot slot);
bool isArmorSlot(EquipSlot slot);
bool isHandSlot(EquipSlot slot);
Armor::Slot armorSlotFor(EquipSlot slot);
EquipSlot equipSlotFor(Armor::Slot slot);

struct AttributeModifier {
    enum class Attribute : uint8_t {
        AttackDamage,
        AttackSpeed,
        AttackReach,
        MiningSpeed,
        MovementSpeed,
        ArmorPoints,
        ArmorToughness,
        Count
    };

    enum class Operation : uint8_t { Add, MultiplyTotal };

    Attribute attribute = Attribute::AttackDamage;
    Operation operation = Operation::Add;
    float amount = 0.0f;
    EquipSlot slot = EquipSlot::MainHand;
};

const char* attributeName(AttributeModifier::Attribute attribute);

constexpr float unarmedAttackDamage = 1.0f;
constexpr float unarmedAttackSpeed = 4.0f;
constexpr float unarmedAttackReach = 3.0f;

float weaponAttackDamage(ItemId item);
float weaponAttackSpeed(ItemId item);
float weaponAttackReach(ItemId item);
float armorWeightPenalty(int armorPoints);

struct EquipmentStats {
    ArmorSet armor;
    int armorPoints = 0;
    float armorToughness = 0.0f;

    float attackDamage = unarmedAttackDamage;
    float attackSpeed = unarmedAttackSpeed;
    float attackReach = unarmedAttackReach;
    float miningSpeed = 1.0f;
    float movementSpeed = 1.0f;

    EnchantSet weaponEnchants;

    std::vector<AttributeModifier> modifiers;

    void applyTo(CombatStats& stats) const;
};

class Equipment {
public:
    Equipment() = default;

    bool equip(EquipSlot slot, const MetaStack& stack);
    bool equipArmor(const Armor& piece);
    bool equipArmorAt(EquipSlot slot, const Armor& piece);

    MetaStack unequip(EquipSlot slot);
    Armor unequipArmor(EquipSlot slot);

    bool accepts(EquipSlot slot, const MetaStack& stack) const;

    const MetaStack& hand(EquipSlot slot) const;
    const Armor& armorPiece(EquipSlot slot) const;

    const MetaStack& mainHand() const { return hands[0]; }
    const MetaStack& offHand() const { return hands[1]; }
    MetaStack& mainHandStack() { return hands[0]; }
    MetaStack& offHandStack() { return hands[1]; }

    const ArmorSet& armor() const { return worn; }
    ArmorSet& armor() { return worn; }

    EquipmentStats stats() const;

    int wearFromHit(float incomingDamage, uint32_t& rng);

    WearResult useHeld(EquipSlot slot, int amount, uint32_t& rng);

    int absorbExperience(int experience);

    int wornPieces() const;
    bool empty() const;

private:
    std::array<MetaStack, 2> hands;
    ArmorSet worn;
};

void loadHandsFromMenu(Equipment& gear, const ContainerMenu& menu, int selectedHotbarSlot);
void storeHandsIntoMenu(Equipment& gear, ContainerMenu& menu, int selectedHotbarSlot);

float equippedMeleeDamage(const Equipment& gear, const CombatStats& attacker,
                          const EffectSet& attackerEffects, bool undeadTarget,
                          bool arthropodTarget, bool critical);

}
