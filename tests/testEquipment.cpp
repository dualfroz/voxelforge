#include "testHarness.h"
#include "vf/Equipment.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

const CraftingRegistry& sharedRegistry() {
    static CraftingRegistry registry;
    return registry;
}

EnchantSet withEnchant(EnchantKind kind, int level) {
    EnchantSet set;
    addEnchant(set, kind, level);
    return set;
}

Equipment fullDiamond() {
    Equipment gear;
    gear.equipArmor(makeArmor(Armor::Material::Diamond, Armor::Slot::Helmet));
    gear.equipArmor(makeArmor(Armor::Material::Diamond, Armor::Slot::Chestplate));
    gear.equipArmor(makeArmor(Armor::Material::Diamond, Armor::Slot::Leggings));
    gear.equipArmor(makeArmor(Armor::Material::Diamond, Armor::Slot::Boots));
    return gear;
}

float modifierTotal(const EquipmentStats& stats, AttributeModifier::Attribute attribute) {
    float total = 0.0f;
    for (const AttributeModifier& modifier : stats.modifiers) {
        if (modifier.attribute == attribute) total += modifier.amount;
    }
    return total;
}

CombatStats readyToSwing() {
    CombatStats stats;
    stats.attackChargeTicks = 200;
    return stats;
}

}

VF_TEST(everyEquipSlotAndAttributeHasAName) {
    for (int i = 0; i < static_cast<int>(EquipSlot::Count); ++i) {
        CHECK(std::string(equipSlotName(static_cast<EquipSlot>(i))) != std::string("unknown"));
    }
    for (int i = 0; i < static_cast<int>(AttributeModifier::Attribute::Count); ++i) {
        CHECK(std::string(attributeName(static_cast<AttributeModifier::Attribute>(i))) !=
              std::string("unknown"));
    }
    CHECK(isArmorSlot(EquipSlot::Head));
    CHECK(isArmorSlot(EquipSlot::Feet));
    CHECK(!isArmorSlot(EquipSlot::MainHand));
    CHECK(isHandSlot(EquipSlot::OffHand));
    CHECK(armorSlotFor(EquipSlot::Legs) == Armor::Slot::Leggings);
    CHECK(armorSlotFor(EquipSlot::MainHand) == Armor::Slot::Count);
    CHECK(equipSlotFor(Armor::Slot::Boots) == EquipSlot::Feet);
    CHECK(equipSlotFor(Armor::Slot::Count) == EquipSlot::Count);
}

VF_TEST(anArmorSlotRefusesAnyItemStackBecauseNoItemIsWearable) {
    Equipment gear;
    MetaStack helmetShaped = makeStack(ItemId::Stone, 1);
    CHECK(!gear.accepts(EquipSlot::Head, helmetShaped));
    CHECK(!gear.equip(EquipSlot::Head, helmetShaped));
    CHECK(!gear.equip(EquipSlot::Feet, makeTool(ItemId::DiamondSword)));
    CHECK(gear.armorPiece(EquipSlot::Head).material == Armor::Material::None);

    CHECK(gear.equip(EquipSlot::MainHand, makeTool(ItemId::DiamondSword)));
    CHECK(gear.mainHand().item == ItemId::DiamondSword);
    CHECK(!gear.equip(EquipSlot::MainHand, MetaStack{}));
    CHECK(gear.mainHand().item == ItemId::DiamondSword);

    CHECK(gear.armorPiece(EquipSlot::MainHand).material == Armor::Material::None);
    CHECK(gear.hand(EquipSlot::Head).empty());
}

VF_TEST(aPieceOfArmorOnlyGoesOnTheSlotItWasMadeFor) {
    Equipment gear;
    Armor helmet = makeArmor(Armor::Material::Iron, Armor::Slot::Helmet);

    CHECK(!gear.equipArmorAt(EquipSlot::Chest, helmet));
    CHECK(!gear.equipArmorAt(EquipSlot::MainHand, helmet));
    CHECK(gear.equipArmorAt(EquipSlot::Head, helmet));
    CHECK(gear.armorPiece(EquipSlot::Head).material == Armor::Material::Iron);
    CHECK_EQ(gear.wornPieces(), 1);
    CHECK(!gear.empty());

    CHECK(!gear.equipArmor(Armor{}));

    Armor removed = gear.unequipArmor(EquipSlot::Head);
    CHECK(removed.material == Armor::Material::Iron);
    CHECK_EQ(gear.wornPieces(), 0);
    CHECK(gear.empty());
    CHECK(gear.unequipArmor(EquipSlot::OffHand).material == Armor::Material::None);
}

VF_TEST(statsSumArmorPointsAndToughnessAcrossTheWholeSet) {
    Equipment gear = fullDiamond();
    EquipmentStats stats = gear.stats();

    CHECK_EQ(stats.armorPoints, gear.armor().totalPoints());
    CHECK_EQ(stats.armorPoints, 20);
    CHECK_NEAR(stats.armorToughness, 8.0f, 0.0001f);
    CHECK_NEAR(modifierTotal(stats, AttributeModifier::Attribute::ArmorPoints), 20.0f, 0.0001f);
    CHECK_NEAR(modifierTotal(stats, AttributeModifier::Attribute::ArmorToughness), 8.0f, 0.0001f);

    Equipment bare;
    EquipmentStats naked = bare.stats();
    CHECK_EQ(naked.armorPoints, 0);
    CHECK_NEAR(naked.armorToughness, 0.0f, 0.0001f);
    CHECK(naked.modifiers.empty());
}

VF_TEST(equipmentStatsFeedTheCombatPipelineDamageAlreadyUnderstands) {
    CombatStats bare;
    CombatStats armored;
    fullDiamond().stats().applyTo(armored);

    DamageEvent blow;
    blow.source = meleeSource(1, Vec3{0.0f, 0.0f, 0.0f}, false);
    blow.amount = 12.0f;

    EffectSet none;
    uint32_t rngBare = 42u;
    uint32_t rngArmored = 42u;
    CombatResult unprotected = applyDamage(bare, none, blow, rngBare);
    CombatResult protectedHit = applyDamage(armored, none, blow, rngArmored);

    CHECK(unprotected.applied);
    CHECK(protectedHit.applied);
    CHECK(protectedHit.healthLost < unprotected.healthLost);
    CHECK(protectedHit.armorDurabilityLost > 0);
    CHECK_EQ(unprotected.armorDurabilityLost, 0);
    CHECK(armored.armor.totalPoints() > 0);
}

VF_TEST(theHeldToolDecidesAttackDamageSpeedAndReach) {
    CHECK_NEAR(weaponAttackDamage(ItemId::DiamondSword), 7.0f, 0.0001f);
    CHECK_NEAR(weaponAttackDamage(ItemId::WoodSword), 4.0f, 0.0001f);
    CHECK_NEAR(weaponAttackDamage(ItemId::DiamondAxe), 9.0f, 0.0001f);
    CHECK_NEAR(weaponAttackDamage(ItemId::Stone), unarmedAttackDamage, 0.0001f);
    CHECK_NEAR(weaponAttackSpeed(ItemId::DiamondSword), 1.6f, 0.0001f);
    CHECK(weaponAttackSpeed(ItemId::DiamondAxe) < weaponAttackSpeed(ItemId::DiamondSword));
    CHECK(weaponAttackReach(ItemId::DiamondSword) > weaponAttackReach(ItemId::DiamondPickaxe));
    CHECK_NEAR(weaponAttackReach(ItemId::Stone), unarmedAttackReach, 0.0001f);

    Equipment gear;
    gear.equip(EquipSlot::MainHand, makeTool(ItemId::DiamondSword));
    EquipmentStats stats = gear.stats();
    CHECK_NEAR(stats.attackDamage, 7.0f, 0.0001f);
    CHECK_NEAR(stats.attackSpeed, 1.6f, 0.0001f);
    CHECK_NEAR(stats.attackReach, unarmedAttackReach + 0.5f, 0.0001f);
    CHECK_NEAR(modifierTotal(stats, AttributeModifier::Attribute::AttackDamage), 6.0f, 0.0001f);

    CombatStats combat;
    stats.applyTo(combat);
    CHECK_NEAR(combat.baseAttackDamage, 7.0f, 0.0001f);
    CHECK_NEAR(combat.attackSpeed, 1.6f, 0.0001f);
}

VF_TEST(sharpnessReachesCombatThroughTheEnchantSetAndNotThroughAttackDamage) {
    Equipment plain;
    plain.equip(EquipSlot::MainHand, makeTool(ItemId::DiamondSword));

    Equipment sharp;
    sharp.equip(EquipSlot::MainHand,
                makeEnchantedTool(ItemId::DiamondSword, withEnchant(EnchantKind::Sharpness, 5)));

    CHECK_NEAR(plain.stats().attackDamage, sharp.stats().attackDamage, 0.0001f);
    CHECK(plain.stats().weaponEnchants.empty());
    CHECK_EQ(sharp.stats().weaponEnchants.levelOf(EnchantKind::Sharpness), 5);

    EffectSet none;
    CombatStats attacker = readyToSwing();
    float plainSwing = equippedMeleeDamage(plain, attacker, none, false, false, false);
    float sharpSwing = equippedMeleeDamage(sharp, attacker, none, false, false, false);
    CHECK(sharpSwing > plainSwing);
    CHECK_NEAR(sharpSwing - plainSwing, 3.0f, 0.05f);

    Equipment unarmed;
    CHECK(equippedMeleeDamage(unarmed, attacker, none, false, false, false) < plainSwing);
}

VF_TEST(efficiencyRaisesMiningSpeedWhileTheCombatNumbersStayPut) {
    Equipment plain;
    plain.equip(EquipSlot::MainHand, makeTool(ItemId::DiamondPickaxe));

    Equipment quick;
    quick.equip(EquipSlot::MainHand,
                makeEnchantedTool(ItemId::DiamondPickaxe, withEnchant(EnchantKind::Efficiency, 4)));

    EquipmentStats plainStats = plain.stats();
    EquipmentStats quickStats = quick.stats();

    CHECK_NEAR(plainStats.miningSpeed, toolTierSpeedMultiplier(ToolTier::Diamond), 0.0001f);
    CHECK_NEAR(quickStats.miningSpeed, plainStats.miningSpeed + 17.0f, 0.0001f);
    CHECK_NEAR(quickStats.attackDamage, plainStats.attackDamage, 0.0001f);
    CHECK(modifierTotal(quickStats, AttributeModifier::Attribute::MiningSpeed) >
          modifierTotal(plainStats, AttributeModifier::Attribute::MiningSpeed));
}

VF_TEST(armorWeightCostsMovementSpeedInProportionToItsPoints) {
    Equipment bare;
    CHECK_NEAR(bare.stats().movementSpeed, 1.0f, 0.0001f);

    Equipment leather;
    leather.equipArmor(makeArmor(Armor::Material::Leather, Armor::Slot::Helmet));
    Equipment diamond = fullDiamond();

    CHECK(leather.stats().movementSpeed < 1.0f);
    CHECK(diamond.stats().movementSpeed < leather.stats().movementSpeed);
    CHECK_NEAR(diamond.stats().movementSpeed, 1.0f - 0.20f, 0.0001f);
    CHECK_NEAR(modifierTotal(diamond.stats(), AttributeModifier::Attribute::MovementSpeed), -0.20f,
               0.0001f);
    CHECK_NEAR(armorWeightPenalty(0), 0.0f, 0.0001f);
    CHECK_NEAR(armorWeightPenalty(100), 0.35f, 0.0001f);
}

VF_TEST(armorWearsDownWhenHitsLandAndAPieceVanishesAtZero) {
    Equipment gear;
    Armor helmet = makeArmor(Armor::Material::Leather, Armor::Slot::Helmet);
    gear.equipArmor(helmet);
    int startingDurability = gear.armorPiece(EquipSlot::Head).durability;
    CHECK(startingDurability > 0);

    uint32_t rng = 8u;
    int spent = gear.wearFromHit(8.0f, rng);
    CHECK(spent > 0);
    CHECK_EQ(gear.armorPiece(EquipSlot::Head).durability, startingDurability - spent);

    int guard = 0;
    while (gear.wornPieces() > 0 && guard < 5000) {
        gear.wearFromHit(20.0f, rng);
        ++guard;
    }
    CHECK(guard < 5000);
    CHECK_EQ(gear.wornPieces(), 0);
    CHECK(gear.armorPiece(EquipSlot::Head).material == Armor::Material::None);
    CHECK_EQ(gear.stats().armorPoints, 0);
}

VF_TEST(unbreakingArmorSurvivesNoticeablyMoreHits) {
    uint32_t plainRng = 31u;
    Equipment plain;
    plain.equipArmor(makeArmor(Armor::Material::Iron, Armor::Slot::Chestplate));
    int plainHits = 0;
    while (plain.wornPieces() > 0 && plainHits < 20000) {
        plain.wearFromHit(4.0f, plainRng);
        ++plainHits;
    }

    uint32_t toughRng = 31u;
    Equipment tough;
    Armor reinforced = makeArmor(Armor::Material::Iron, Armor::Slot::Chestplate);
    addEnchant(reinforced.enchants, EnchantKind::Unbreaking, 3);
    tough.equipArmor(reinforced);
    int toughHits = 0;
    while (tough.wornPieces() > 0 && toughHits < 20000) {
        tough.wearFromHit(4.0f, toughRng);
        ++toughHits;
    }

    CHECK(plainHits > 0);
    CHECK(toughHits > plainHits * 2);
    CHECK(toughHits < 20000);
}

VF_TEST(theHeldToolBreaksAtZeroUnlessItIsUnbreakable) {
    uint32_t rng = 17u;
    Equipment gear;
    gear.equip(EquipSlot::MainHand, makeTool(ItemId::WoodShovel));
    int max = gear.mainHand().meta.durability.max;

    for (int hit = 1; hit < max; ++hit) {
        CHECK(gear.useHeld(EquipSlot::MainHand, 1, rng) == WearResult::Damaged);
    }
    CHECK(gear.useHeld(EquipSlot::MainHand, 1, rng) == WearResult::Broke);
    CHECK(gear.mainHand().empty());

    MetaStack indestructible = makeTool(ItemId::WoodShovel);
    indestructible.meta.unbreakable = true;
    gear.equip(EquipSlot::OffHand, indestructible);
    CHECK(gear.useHeld(EquipSlot::OffHand, 5000, rng) == WearResult::Absorbed);
    CHECK(!gear.offHand().empty());
    CHECK(gear.useHeld(EquipSlot::Head, 1, rng) == WearResult::NotDamageable);
}

VF_TEST(experiencePickedUpRepairsMendingGearBeforeTheHeldTool) {
    Equipment gear;
    Armor boots = makeArmor(Armor::Material::Iron, Armor::Slot::Boots);
    addEnchant(boots.enchants, EnchantKind::Mending, 1);
    boots.durability = boots.maxDurability - 30;
    gear.equipArmor(boots);

    MetaStack pickaxe = makeEnchantedTool(ItemId::IronPickaxe, withEnchant(EnchantKind::Mending, 1));
    pickaxe.meta.durability.used = 100;
    gear.equip(EquipSlot::MainHand, pickaxe);

    int repaired = gear.absorbExperience(10);
    CHECK_EQ(repaired, 20);
    CHECK_EQ(gear.armorPiece(EquipSlot::Feet).durability, boots.maxDurability - 10);
    CHECK_EQ(gear.mainHand().meta.durability.used, 100);

    int spilled = gear.absorbExperience(100);
    CHECK_EQ(gear.armorPiece(EquipSlot::Feet).durability, boots.maxDurability);
    CHECK(gear.mainHand().meta.durability.used < 100);
    CHECK(spilled > 10);

    Equipment plain;
    plain.equipArmor(makeArmor(Armor::Material::Iron, Armor::Slot::Boots));
    CHECK_EQ(plain.absorbExperience(50), 0);
    CHECK_EQ(plain.absorbExperience(0), 0);
}

VF_TEST(theHandsRoundTripThroughAnOpenMenu) {
    ContainerMenu menu(MenuKind::PlayerInventory, &sharedRegistry());
    const MenuLayout& layout = menu.layout();

    MetaStack sword = makeEnchantedTool(ItemId::IronSword, withEnchant(EnchantKind::FireAspect, 2));
    sword.meta.customName = "Emberfang";
    menu.slot(layout.hotbarFirst + 4).cell.stack = sword;
    menu.slot(layout.offHandSlot).cell.stack = makeStack(ItemId::Bread, 5);

    Equipment gear;
    loadHandsFromMenu(gear, menu, 4);
    CHECK(stacksIdentical(gear.mainHand(), sword));
    CHECK_EQ(gear.offHand().count, 5);
    CHECK_EQ(gear.stats().weaponEnchants.levelOf(EnchantKind::FireAspect), 2);

    uint32_t rng = 4u;
    gear.useHeld(EquipSlot::MainHand, 3, rng);
    storeHandsIntoMenu(gear, menu, 4);
    CHECK_EQ(menu.stackAt(layout.hotbarFirst + 4).meta.durability.used, 3);
    CHECK(menu.stackAt(layout.hotbarFirst + 4).meta.customName == std::string("Emberfang"));

    ContainerMenu chest(MenuKind::Chest, &sharedRegistry());
    chest.slot(chest.layout().hotbarFirst).cell.stack = makeStack(ItemId::Stone, 2);
    loadHandsFromMenu(gear, chest, 0);
    CHECK(gear.mainHand().item == ItemId::Stone);
    CHECK_EQ(gear.offHand().count, 5);

    loadHandsFromMenu(gear, chest, 99);
    CHECK(gear.mainHand().item == ItemId::Stone);
}

VF_TEST(theModifierListAddsUpToTheStatsItIsDescribing) {
    Equipment gear = fullDiamond();
    gear.equip(EquipSlot::MainHand,
               makeEnchantedTool(ItemId::IronAxe, withEnchant(EnchantKind::Efficiency, 2)));
    EquipmentStats stats = gear.stats();

    CHECK_NEAR(unarmedAttackDamage +
                   modifierTotal(stats, AttributeModifier::Attribute::AttackDamage),
               stats.attackDamage, 0.0001f);
    CHECK_NEAR(unarmedAttackSpeed + modifierTotal(stats, AttributeModifier::Attribute::AttackSpeed),
               stats.attackSpeed, 0.0001f);
    CHECK_NEAR(unarmedAttackReach + modifierTotal(stats, AttributeModifier::Attribute::AttackReach),
               stats.attackReach, 0.0001f);
    CHECK_NEAR(1.0f + modifierTotal(stats, AttributeModifier::Attribute::MiningSpeed),
               stats.miningSpeed, 0.0001f);
    CHECK_NEAR(1.0f + modifierTotal(stats, AttributeModifier::Attribute::MovementSpeed),
               stats.movementSpeed, 0.0001f);
    CHECK_NEAR(modifierTotal(stats, AttributeModifier::Attribute::ArmorPoints),
               static_cast<float>(stats.armorPoints), 0.0001f);

    int handModifiers = 0;
    for (const AttributeModifier& modifier : stats.modifiers) {
        if (modifier.slot == EquipSlot::MainHand) ++handModifiers;
    }
    CHECK(handModifiers >= 4);
}
