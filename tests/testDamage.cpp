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

}

VF_TEST(diamondArmorReducesMeleeDamageByTheVanillaCurve) {
    ArmorSet diamond = fullSet(Armor::Material::Diamond);
    CHECK_EQ(diamond.totalPoints(), 20);
    CHECK_NEAR(diamond.totalToughness(), 8.0f, 0.001f);
    CHECK_NEAR(armorReduction(20, 8.0f, 10.0f), 3.0f, 0.001f);

    ArmorSet leather = fullSet(Armor::Material::Leather);
    CHECK_EQ(leather.totalPoints(), 7);
    CHECK_NEAR(leather.totalToughness(), 0.0f, 0.001f);
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

    EffectSet crippled;
    applyEffect(crippled, makeEffect(EffectKind::Weakness, 3, 200));
    CHECK_NEAR(meleeAttackDamage(attacker, bare, crippled, false, false, false), 0.0f, 0.001f);
}

VF_TEST(resistanceAndProtectionMultiplyOntoTheArmorReducedHit) {
    CHECK_NEAR(protectionFactor(0), 1.0f, 0.001f);
    CHECK_NEAR(protectionFactor(16), 0.36f, 0.001f);
    CHECK_NEAR(protectionFactor(40), 0.2f, 0.001f);

    uint32_t rng = 21u;
    CombatStats player;
    player.armor = fullSet(Armor::Material::Leather);
    enchantWholeSet(player.armor, EnchantKind::Protection, 4);
    CHECK_EQ(player.armor.protectionPoints(DamageType::Melee), 16);

    EffectSet resisted;
    applyEffect(resisted, makeEffect(EffectKind::Resistance, 2, 400));

    CombatResult hit = applyDamage(player, resisted, hitFor(DamageType::Melee, 12.0f), rng);
    CHECK(hit.applied);
    CHECK(hit.afterArmor < 12.0f);
    CHECK_NEAR(hit.afterResistance, hit.afterArmor * 0.6f, 0.001f);
    CHECK_NEAR(hit.afterProtection, hit.afterResistance * 0.36f, 0.001f);
    CHECK_NEAR(hit.healthLost, hit.afterProtection, 0.001f);
}

VF_TEST(featherFallingOnlyHelpsAgainstFallDamage) {
    ArmorSet boots;
    Armor feet = makeArmor(Armor::Material::Iron, Armor::Slot::Boots);
    CHECK(addEnchant(feet.enchants, EnchantKind::FeatherFalling, 4));
    boots.equip(feet);

    CHECK_EQ(boots.protectionPoints(DamageType::Fall), 12);
    CHECK_EQ(boots.protectionPoints(DamageType::Melee), 0);
    CHECK_EQ(boots.protectionPoints(DamageType::Explosion), 0);
}

VF_TEST(fallDamageScalesWithDistanceAndJumpBoostRaisesTheSafeDrop) {
    EffectSet none;
    CHECK_NEAR(fallDamage(2.0f, none), 0.0f, 0.001f);
    CHECK_NEAR(fallDamage(3.0f, none), 0.0f, 0.001f);
    CHECK_NEAR(fallDamage(10.0f, none), 7.0f, 0.001f);
    CHECK_NEAR(fallDamage(23.5f, none), 20.0f, 0.001f);

    EffectSet springy;
    applyEffect(springy, makeEffect(EffectKind::JumpBoost, 2, 200));
    CHECK_NEAR(fallDamage(10.0f, springy), 5.0f, 0.001f);
    CHECK_NEAR(fallDamage(5.0f, springy), 0.0f, 0.001f);
}

VF_TEST(armorWearsDownAndFallsOffAfterSustainedPunishment) {
    EffectSet none;
    uint32_t rng = 4242u;
    CombatStats player;
    player.armor = fullSet(Armor::Material::Iron);
    int startingPoints = player.armor.totalPoints();
    CHECK_EQ(startingPoints, 15);

    int helmetStart = player.armor.at(Armor::Slot::Helmet).durability;
    CHECK_EQ(helmetStart, 165);

    for (int hit = 0; hit < 200; ++hit) {
        player.health = 20.0f;
        player.alive = true;
        applyDamage(player, none, hitFor(DamageType::Melee, 8.0f), rng);
        idleTicks(player, none, invulnerabilityTicks);
    }
    CHECK(!player.armor.at(Armor::Slot::Helmet).worn());
    CHECK_EQ(player.armor.totalPoints(), 0);
}

VF_TEST(unbreakingStretchesArmorDurabilityOverManyHits) {
    uint32_t plainRng = 99u;
    ArmorSet plain;
    plain.equip(makeArmor(Armor::Material::Iron, Armor::Slot::Chestplate));

    uint32_t toughRng = 99u;
    ArmorSet tough;
    Armor chest = makeArmor(Armor::Material::Iron, Armor::Slot::Chestplate);
    CHECK(addEnchant(chest.enchants, EnchantKind::Unbreaking, 3));
    tough.equip(chest);

    int plainSpent = 0;
    int toughSpent = 0;
    for (int hit = 0; hit < 40; ++hit) {
        plainSpent += wearArmor(plain, 8.0f, plainRng);
        toughSpent += wearArmor(tough, 8.0f, toughRng);
    }
    CHECK_EQ(plainSpent, 80);
    CHECK(toughSpent > 0);
    CHECK(toughSpent * 2 < plainSpent);
}

VF_TEST(absorptionSoaksDamageBeforeHealthDoes) {
    EffectSet none;
    uint32_t rng = 5u;
    CombatStats player;
    player.absorption = 4.0f;

    CombatResult hit = applyDamage(player, none, hitFor(DamageType::Magic, 6.0f), rng);
    CHECK_NEAR(hit.absorbedByAbsorption, 4.0f, 0.001f);
    CHECK_NEAR(hit.healthLost, 2.0f, 0.001f);
    CHECK_NEAR(player.absorption, 0.0f, 0.001f);
    CHECK_NEAR(player.health, 18.0f, 0.001f);
}

VF_TEST(absorptionEffectRefillsTheShieldOnACombatTick) {
    CombatStats player;
    EffectSet effects;
    applyEffect(effects, makeEffect(EffectKind::Absorption, 2, 400));
    applyEffect(effects, makeEffect(EffectKind::HealthBoost, 1, 400));

    tickCombat(player, effects);
    CHECK_NEAR(player.absorption, 8.0f, 0.001f);
    CHECK_NEAR(effectiveMaxHealth(player, effects), 24.0f, 0.001f);
}

VF_TEST(knockbackPushesAwayFromTheAttackerAndScalesWithTheEnchant) {
    Vec3 gentle = knockbackVector({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, 0.0f);
    CHECK_NEAR(gentle.x, 0.4f, 0.001f);
    CHECK_NEAR(gentle.y, 0.4f, 0.001f);
    CHECK_NEAR(gentle.z, 0.0f, 0.001f);

    Vec3 hard = knockbackVector({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -2.0f}, 2.0f);
    CHECK_NEAR(hard.z, -1.4f, 0.001f);
    CHECK_NEAR(hard.x, 0.0f, 0.001f);

    EffectSet none;
    uint32_t rng = 8u;
    CombatStats target;
    DamageEvent event;
    event.source = meleeSource(1, {0.0f, 0.0f, 0.0f}, true);
    event.amount = 4.0f;
    event.direction = {1.0f, 0.0f, 0.0f};
    event.knockbackStrength = 1.0f;
    CombatResult hit = applyDamage(target, none, event, rng);
    CHECK_NEAR(hit.knockback.x, 0.9f, 0.001f);
}

VF_TEST(thornsRetaliatesOnSomeHitsAndNeverExceedsFourDamage) {
    EffectSet none;
    uint32_t rng = 1234u;
    CombatStats player;
    player.armor = fullSet(Armor::Material::Diamond);
    enchantWholeSet(player.armor, EnchantKind::Thorns, 3);

    int retaliations = 0;
    for (int hit = 0; hit < 300; ++hit) {
        player.health = 20.0f;
        player.alive = true;
        player.invulnerableTicks = 0;
        DamageEvent event;
        event.source = meleeSource(2, {0.0f, 0.0f, 0.0f}, true);
        event.amount = 5.0f;
        event.direction = {0.0f, 0.0f, 1.0f};
        CombatResult struck = applyDamage(player, none, event, rng);
        if (struck.thornsDamage > 0.0f) {
            ++retaliations;
            CHECK(struck.thornsDamage >= 1.0f);
            CHECK(struck.thornsDamage <= 4.0f);
        }
    }
    CHECK(retaliations > 60);
    CHECK(retaliations < 240);
}

VF_TEST(environmentalDamageWithoutAnAttackerNeverTriggersThorns) {
    EffectSet none;
    uint32_t rng = 77u;
    CombatStats player;
    player.armor = fullSet(Armor::Material::Diamond);
    enchantWholeSet(player.armor, EnchantKind::Thorns, 3);

    for (int tick = 0; tick < 100; ++tick) {
        player.health = 20.0f;
        player.alive = true;
        player.invulnerableTicks = 0;
        CombatResult burn = applyDamage(player, none, hitFor(DamageType::Cactus, 2.0f), rng);
        CHECK_NEAR(burn.thornsDamage, 0.0f, 0.001f);
    }
}

VF_TEST(fireResistanceMakesLavaAndFireTicksHarmless) {
    uint32_t rng = 6u;
    EffectSet fireproof;
    applyEffect(fireproof, makeEffect(EffectKind::FireResistance, 1, 400));

    CombatStats player;
    CombatResult lava = applyDamage(player, fireproof, hitFor(DamageType::Lava, 4.0f), rng);
    CHECK(!lava.applied);
    CHECK_NEAR(player.health, 20.0f, 0.001f);

    CombatResult drown = applyDamage(player, fireproof, hitFor(DamageType::Drowning, 2.0f), rng);
    CHECK(drown.applied);
    CHECK_NEAR(player.health, 18.0f, 0.001f);
}

VF_TEST(witherEffectRunsAPlayerDownThroughRepeatedCombatTicks) {
    CombatStats player;
    EffectSet effects;
    applyEffect(effects, makeEffect(EffectKind::Wither, 2, 2000));

    int tick = 0;
    for (; tick < 2000 && player.alive; ++tick) tickCombat(player, effects);
    CHECK(!player.alive);
    CHECK_NEAR(player.health, 0.0f, 0.001f);
    CHECK(tick < 500);
}

VF_TEST(damageTypeClassificationMatchesTheMitigationRules) {
    CHECK(bypassesArmor(DamageType::Void));
    CHECK(bypassesArmor(DamageType::Starve));
    CHECK(bypassesArmor(DamageType::Suffocation));
    CHECK(!bypassesArmor(DamageType::Explosion));
    CHECK(bypassesInvulnerability(DamageType::Void));
    CHECK(!bypassesInvulnerability(DamageType::Fire));
    CHECK(bypassesMitigation(DamageType::Void));
    CHECK(isFireDamage(DamageType::Lava));
    CHECK(isProjectileDamage(DamageType::Projectile));
    CHECK(environmentSource(DamageType::Fire).fiery);
    CHECK(!environmentSource(DamageType::Fall).fiery);
    CHECK(projectileSource(9, 3, {}, true).ownerId == 3);
}
