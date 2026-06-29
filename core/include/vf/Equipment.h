#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ContainerMenu.h"
#include "Damage.h"
#include "ItemStack.h"

namespace vf {

// What a player is wearing and holding. The armour half deliberately reuses
// Damage.h's Armor and ArmorSet instead of inventing a second description of a
// breastplate: the whole point is that EquipmentStats hands the combat code
// exactly the values it already knows how to consume. The hand half is made of
// MetaStacks, the same stacks a ContainerMenu moves around.
//
// ItemId has no wearable entries at all, which is why armour cannot be a
// MetaStack here. equip() therefore refuses every stack aimed at an armour
// slot, and equipArmor() is the way in.

enum class EquipSlot : uint8_t { Head, Chest, Legs, Feet, MainHand, OffHand, Count };

const char* equipSlotName(EquipSlot slot);
bool isArmorSlot(EquipSlot slot);
bool isHandSlot(EquipSlot slot);
Armor::Slot armorSlotFor(EquipSlot slot);
EquipSlot equipSlotFor(Armor::Slot slot);

// One line of the "hover the item and read what it does" tooltip, and the unit
// the stats sum works in.
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

// Base values a bare player has before anything is worn or held. Named for the
// unarmed case rather than "base", because CombatStats already has fields with
// those names and the two must stay visibly distinct.
constexpr float unarmedAttackDamage = 1.0f;
constexpr float unarmedAttackSpeed = 4.0f;
constexpr float unarmedAttackReach = 3.0f;

// The damage and swing speed a tool grants when held, keyed off the item
// tables so a new tool tier is picked up without touching this module.
float weaponAttackDamage(ItemId item);
float weaponAttackSpeed(ItemId item);
// Reach and armour weight are this engine's own rules, not ported constants: a
// long hafted weapon reaches further, and every armour point costs a sliver of
// movement speed. They are called out here so nobody mistakes them for vanilla.
float weaponAttackReach(ItemId item);
float armorWeightPenalty(int armorPoints);

struct EquipmentStats {
    // Ready to be assigned onto CombatStats::armor.
    ArmorSet armor;
    int armorPoints = 0;
    float armorToughness = 0.0f;

    float attackDamage = unarmedAttackDamage;
    float attackSpeed = unarmedAttackSpeed;
    float attackReach = unarmedAttackReach;
    // Tool tier times Efficiency: the one attribute that draws on both the item
    // and an enchantment without colliding with Damage.h's combat maths.
    float miningSpeed = 1.0f;
    float movementSpeed = 1.0f;

    // The held weapon's enchantments, handed to Damage.h's meleeAttackDamage
    // untouched. Sharpness and the Protection family are deliberately absent
    // from attackDamage and from armorPoints: Damage.h already owns those
    // formulas, and summing them here as well would count them twice.
    EnchantSet weaponEnchants;

    std::vector<AttributeModifier> modifiers;

    // Writes the derived values onto an existing CombatStats without touching
    // health, absorption or the invulnerability window.
    void applyTo(CombatStats& stats) const;
};

class Equipment {
public:
    Equipment() = default;

    // Hand slots only. Returns false for an armour slot or an empty stack,
    // which is what "only a matching item goes on" means with this item table.
    bool equip(EquipSlot slot, const MetaStack& stack);
    bool equipArmor(const Armor& piece);
    // Places a piece in the slot its own Armor::Slot names, so a helmet cannot
    // be forced onto the legs.
    bool equipArmorAt(EquipSlot slot, const Armor& piece);

    MetaStack unequip(EquipSlot slot);
    Armor unequipArmor(EquipSlot slot);

    bool accepts(EquipSlot slot, const MetaStack& stack) const;

    // Reading by slot is safe for every EquipSlot: an armour slot answers with
    // an empty stack and a hand slot with an empty piece, so callers iterating
    // the whole enum need no special cases. Writing goes through the named
    // accessors below, where the slot is fixed at compile time.
    const MetaStack& hand(EquipSlot slot) const;
    const Armor& armorPiece(EquipSlot slot) const;

    const MetaStack& mainHand() const { return hands[0]; }
    const MetaStack& offHand() const { return hands[1]; }
    MetaStack& mainHandStack() { return hands[0]; }
    MetaStack& offHandStack() { return hands[1]; }

    const ArmorSet& armor() const { return worn; }
    ArmorSet& armor() { return worn; }

    EquipmentStats stats() const;

    // Wears the armour by one point per four damage, honouring Unbreaking and
    // dropping a piece that hits zero. Delegates to Damage.h's wearArmor so the
    // wear curve stays in one place. Returns the durability spent.
    int wearFromHit(float incomingDamage, uint32_t& rng);

    // Wears the held weapon or tool. Returns what happened to it, including the
    // break that empties the slot.
    WearResult useHeld(EquipSlot slot, int amount, uint32_t& rng);

    // Experience picked up while wearing Mending gear repairs it, gear first
    // and the held tool after. Returns the durability restored.
    int absorbExperience(int experience);

    int wornPieces() const;
    bool empty() const;

private:
    std::array<MetaStack, 2> hands;
    ArmorSet worn;
};

// Reads the player's hands out of an open menu: the selected hotbar slot is the
// main hand, the menu's off-hand slot is the off hand. This is the seam between
// the click layer and the combat layer, and it copies stacks rather than
// aliasing them so a half-finished drag cannot arm the player.
void loadHandsFromMenu(Equipment& gear, const ContainerMenu& menu, int selectedHotbarSlot);
void storeHandsIntoMenu(Equipment& gear, ContainerMenu& menu, int selectedHotbarSlot);

// Full melee damage against a target, combining the equipment's own attack
// value with the enchantment maths Damage.h already owns.
float equippedMeleeDamage(const Equipment& gear, const CombatStats& attacker,
                          const EffectSet& attackerEffects, bool undeadTarget,
                          bool arthropodTarget, bool critical);

} // namespace vf
