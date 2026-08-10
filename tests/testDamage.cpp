#include "testHarness.h"
#include "vf/Damage.h"

using namespace vf;

namespace {

ArmorSet fullSet(Armor::Material material) {
    ArmorSet set;
    set.equip(makeArmor(material, Armor::Slot::Helmet));
    set.equip(makeArmor(material, Armor::Slot::Chestplate));
    set.equip(makeArmor(material, Armor::Slot::Leggings));
    set.equip(makeArmor(material, Armor::Slot::Boots));
    return set;
}

void enchantWholeSet(ArmorSet& set, EnchantKind kind, int level) {
    for (Armor& piece : set.pieces) addEnchant(piece.enchants, kind, level);
}

DamageEvent hitFor(DamageType type, float amount) {
    DamageEvent event;
    event.source = environmentSource(type);
    event.amount = amount;
    return event;
}

void idleTicks(CombatStats& stats, EffectSet& effects, int count) {
    for (int i = 0; i < count; ++i) tickCombat(stats, effects);
}

} // namespace

VF_TEST(diamondArmorReducesMeleeDamageByTheVanillaCurve) {
    ArmorSet diamond = fullSet(Armor::Material::Diamond);
    CHECK_EQ(diamond.totalPoints(), 20);
    CHECK_NEAR(diamond.totalToughness(), 8.0f, 0.001f);
    CHECK_NEAR(armorReduction(20, 8.0f, 10.0f), 3.0f, 0.001f);

    ArmorSet leather = fullSet(Armor::Material::Leather);
    CHECK_EQ(leather.totalPoints(), 7);
    CHECK_NEAR(leather.totalToughness(), 0.0f, 0.001f);
    // Weak armour hits the points/5 floor against a big blow.
    CHECK_NEAR(armorReduction(7, 0.0f, 20.0f), 20.0f * (1.0f - 1.4f / 25.0f), 0.001f);
}

VF_TEST(voidAndStarveDamageIgnoreArmorEntirely) {
    EffectSet none;
    uint32_t rng = 7u;
    CombatStats player;
    player.armor = fullSet(Armor::Material::Diamond);

    CombatResult voided = applyDamage(player, none, hitFor(DamageType::Void, 6.0f), rng);
    CHECK(voided.applied);
    CHECK_NEAR(voided.healthLost, 6.0f, 0.001f);
    CHECK_EQ(voided.armorDurabilityLost, 0);

    player.health = 20.0f;
    CombatResult melee = applyDamage(player, none, hitFor(DamageType::Melee, 6.0f), rng);
    CHECK(melee.applied);
    CHECK(melee.healthLost < 3.0f);
    CHECK(melee.armorDurabilityLost > 0);
}

VF_TEST(invulnerabilityWindowSwallowsWeakerFollowUpsButNotStrongerOnes) {
    EffectSet none;
    uint32_t rng = 11u;
    CombatStats player;

    CombatResult first = applyDamage(player, none, hitFor(DamageType::Melee, 5.0f), rng);
    CHECK(first.applied);
    CHECK_NEAR(player.health, 15.0f, 0.001f);
    CHECK_EQ(player.invulnerableTicks, invulnerabilityTicks);

    CombatResult weaker = applyDamage(player, none, hitFor(DamageType::Melee, 3.0f), rng);
    CHECK(weaker.blockedByInvulnerability);
    CHECK(!weaker.applied);
    CHECK_NEAR(player.health, 15.0f, 0.001f);

    CombatResult stronger = applyDamage(player, none, hitFor(DamageType::Melee, 8.0f), rng);
    CHECK(stronger.applied);
    CHECK_NEAR(player.health, 12.0f, 0.001f);

    idleTicks(player, none, invulnerabilityTicks);
    CHECK_EQ(player.invulnerableTicks, 0);
    CombatResult fresh = applyDamage(player, none, hitFor(DamageType::Melee, 2.0f), rng);
    CHECK(fresh.applied);
    CHECK_NEAR(player.health, 10.0f, 0.001f);
}

VF_TEST(starvationLandsEveryTickBecauseItIgnoresTheInvulnerabilityWindow) {
    EffectSet none;
    uint32_t rng = 3u;
    CombatStats player;

    for (int i = 0; i < 5; ++i) {
        CombatResult starve = applyDamage(player, none, hitFor(DamageType::Starve, 1.0f), rng);
        CHECK(starve.applied);
    }
    CHECK_NEAR(player.health, 15.0f, 0.001f);
}

VF_TEST(attackChargeRampsBackToFullAndScalesSwingDamage) {
    EffectSet none;
    CombatStats attacker;
    attacker.baseAttackDamage = 7.0f;
    attacker.attackSpeed = 4.0f;
    CHECK_EQ(attackCooldownTicks(4.0f), 5);

    registerSwing(attacker);
    CHECK_NEAR(attackCharge(attacker), 0.0f, 0.001f);
    EnchantSet bare;
    float coldSwing = meleeAttackDamage(attacker, bare, none, false, false, false);
    CHECK_NEAR(coldSwing, 7.0f * 0.2f, 0.001f);

    for (int i = 0; i < 5; ++i) tickCombat(attacker, none);
    CHECK_NEAR(attackCharge(attacker), 1.0f, 0.001f);
    float fullSwing = meleeAttackDamage(attacker, bare, none, false, false, false);
    CHECK_NEAR(fullSwing, 7.0f, 0.001f);

    // The bar caps, extra ticks change nothing.
    for (int i = 0; i < 20; ++i) tickCombat(attacker, none);
    CHECK_NEAR(attackCharge(attacker), 1.0f, 0.001f);
}

VF_TEST(sharpnessAndCriticalHitsStackOnTopOfBaseSwingDamage) {
    EffectSet none;
    CombatStats attacker;
    attacker.baseAttackDamage = 7.0f;
    attacker.attackChargeTicks = 5;

    EnchantSet plain;
    EnchantSet sharp;
    CHECK(addEnchant(sharp, EnchantKind::Sharpness, 5));

    CHECK_NEAR(meleeAttackDamage(attacker, plain, none, false, false, false), 7.0f, 0.001f);
    CHECK_NEAR(meleeAttackDamage(attacker, sharp, none, false, false, false), 10.0f, 0.001f);
    CHECK_NEAR(meleeAttackDamage(attacker, sharp, none, false, false, true), 15.0f, 0.001f);

    EnchantSet smite;
    CHECK(addEnchant(smite, EnchantKind::Smite, 4));
    CHECK_NEAR(meleeAttackDamage(attacker, smite, none, false, false, false), 7.0f, 0.001f);
    CHECK_NEAR(meleeAttackDamage(attacker, smite, none, true, false, false), 17.0f, 0.001f);
}

VF_TEST(strengthAddsAndWeaknessRemovesFlatMeleeDamage) {
    CombatStats attacker;
    attacker.baseAttackDamage = 7.0f;
    attacker.attackChargeTicks = 5;
    EnchantSet bare;

    EffectSet strong;
    applyEffect(strong, makeEffect(EffectKind::Strength, 2, 200));
    CHECK_NEAR(meleeAttackDamage(attacker, bare, strong, false, false, false), 13.0f, 0.001f);

    EffectSet weak;
    applyEffect(weak, makeEffect(EffectKind::Weakness, 1, 200));
    CHECK_NEAR(meleeAttackDamage(attacker, bare, weak, false, false, false), 3.0f, 0.001f);

