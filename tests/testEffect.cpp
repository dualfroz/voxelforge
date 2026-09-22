#include "testHarness.h"
#include "vf/Effect.h"

using namespace vf;

namespace {

int ticksUntilDead(EffectSet& set, float& health, float maxHealth, int limit) {
    for (int tick = 0; tick < limit; ++tick) {
        EffectSet::Pending pending = tickEffects(set);
        applyEffectTick(pending, health, maxHealth);
        if (health <= 0.0f) return tick + 1;
    }
    return -1;
}

}

VF_TEST(strongerEffectReplacesWeakerAndLongerOneOfEqualLevelWins) {
    EffectSet set;
    CHECK(applyEffect(set, makeEffect(EffectKind::Speed, 2, 100)));
    CHECK_EQ(effectLevel(set, EffectKind::Speed), 2);

    CHECK(!applyEffect(set, makeEffect(EffectKind::Speed, 1, 5000)));
    CHECK_EQ(effectLevel(set, EffectKind::Speed), 2);
    CHECK_EQ(effectDuration(set, EffectKind::Speed), 100);

    CHECK(applyEffect(set, makeEffect(EffectKind::Speed, 2, 240)));
    CHECK_EQ(effectDuration(set, EffectKind::Speed), 240);

    CHECK(applyEffect(set, makeEffect(EffectKind::Speed, 3, 10)));
    CHECK_EQ(effectLevel(set, EffectKind::Speed), 3);
    CHECK_EQ(effectDuration(set, EffectKind::Speed), 10);
    CHECK_EQ(static_cast<int>(set.active.size()), 1);
}

VF_TEST(regenerationHealsOnItsIntervalAndFasterAtHigherLevel) {
    EffectSet slow;
    applyEffect(slow, makeEffect(EffectKind::Regeneration, 1, 500));
    float slowHealth = 10.0f;
    for (int tick = 0; tick < 500; ++tick) {
        EffectSet::Pending pending = tickEffects(slow);
        applyEffectTick(pending, slowHealth, 20.0f);
    }
    CHECK_NEAR(slowHealth, 20.0f, 0.001f);

    EffectSet fast;
    applyEffect(fast, makeEffect(EffectKind::Regeneration, 2, 100));
    float fastHealth = 10.0f;
    int fastHeals = 0;
    for (int tick = 0; tick < 100; ++tick) {
        EffectSet::Pending pending = tickEffects(fast);
        if (pending.regenerationHeal > 0.0f) ++fastHeals;
        applyEffectTick(pending, fastHealth, 20.0f);
    }
    CHECK_EQ(periodicIntervalTicks(EffectKind::Regeneration, 0), 50);
    CHECK_EQ(periodicIntervalTicks(EffectKind::Regeneration, 1), 25);
    CHECK_EQ(fastHeals, 4);
    CHECK_NEAR(fastHealth, 14.0f, 0.001f);
}

VF_TEST(poisonNeverTakesTheLastHalfHeartButWitherKills) {
    EffectSet poisoned;
    applyEffect(poisoned, makeEffect(EffectKind::Poison, 1, 2000));
    float poisonedHealth = 20.0f;
    CHECK_EQ(ticksUntilDead(poisoned, poisonedHealth, 20.0f, 2000), -1);
    CHECK_NEAR(poisonedHealth, 1.0f, 0.001f);

    EffectSet withered;
    applyEffect(withered, makeEffect(EffectKind::Wither, 1, 2000));
    float witheredHealth = 20.0f;
    int deathTick = ticksUntilDead(withered, witheredHealth, 20.0f, 2000);
    CHECK(deathTick > 0);
    CHECK_NEAR(witheredHealth, 0.0f, 0.001f);
}

VF_TEST(witherAtHigherLevelKillsFasterThanAtLevelOne) {
    EffectSet weak;
    applyEffect(weak, makeEffect(EffectKind::Wither, 1, 4000));
    float weakHealth = 20.0f;
    int slowDeath = ticksUntilDead(weak, weakHealth, 20.0f, 4000);

    EffectSet strong;
    applyEffect(strong, makeEffect(EffectKind::Wither, 3, 4000));
    float strongHealth = 20.0f;
    int fastDeath = ticksUntilDead(strong, strongHealth, 20.0f, 4000);

    CHECK(slowDeath > 0);
    CHECK(fastDeath > 0);
    CHECK(fastDeath * 3 < slowDeath);
}

VF_TEST(effectsExpireAfterTheirDurationAndStopModifyingAttributes) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::Speed, 3, 40));
    CHECK_NEAR(effectModifiers(set).moveSpeedMultiplier, 1.6f, 0.001f);

    int expired = 0;
    for (int tick = 0; tick < 39; ++tick) expired += tickEffects(set).expiredCount;
    CHECK_EQ(expired, 0);
    CHECK(hasEffect(set, EffectKind::Speed));

    expired += tickEffects(set).expiredCount;
    CHECK_EQ(expired, 1);
    CHECK(!hasEffect(set, EffectKind::Speed));
    CHECK_NEAR(effectModifiers(set).moveSpeedMultiplier, 1.0f, 0.001f);
}

VF_TEST(speedAndSlownessMultiplyMovementInOppositeDirections) {
    EffectSet hasted;
    applyEffect(hasted, makeEffect(EffectKind::Speed, 2, 100));
    CHECK_NEAR(effectModifiers(hasted).moveSpeedMultiplier, 1.4f, 0.001f);

    EffectSet slowed;
    applyEffect(slowed, makeEffect(EffectKind::Slowness, 4, 100));
    CHECK_NEAR(effectModifiers(slowed).moveSpeedMultiplier, 0.4f, 0.001f);

    EffectSet both;
    applyEffect(both, makeEffect(EffectKind::Speed, 2, 100));
    applyEffect(both, makeEffect(EffectKind::Slowness, 2, 100));
    CHECK_NEAR(effectModifiers(both).moveSpeedMultiplier, 1.4f * 0.7f, 0.001f);
}

VF_TEST(miningFatigueCompoundsWithHasteOnTheMiningMultiplier) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::Haste, 2, 200));
    CHECK_NEAR(effectModifiers(set).miningSpeedMultiplier, 1.4f, 0.001f);

    applyEffect(set, makeEffect(EffectKind::MiningFatigue, 1, 200));
    CHECK_NEAR(effectModifiers(set).miningSpeedMultiplier, 1.4f * 0.3f, 0.001f);
}

VF_TEST(instantHealthResolvesOnTheNextTickAndLeavesNoResidue) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::InstantHealth, 2, 0));
    CHECK_EQ(static_cast<int>(set.active.size()), 1);

    float health = 6.0f;
    EffectSet::Pending pending = tickEffects(set);
    CHECK_NEAR(pending.instantHeal, 8.0f, 0.001f);
    applyEffectTick(pending, health, 20.0f);
    CHECK_NEAR(health, 14.0f, 0.001f);
    CHECK_EQ(static_cast<int>(set.active.size()), 0);

    pending = tickEffects(set);
    CHECK(pending.empty());
}

VF_TEST(instantDamageAtReducedPotencyHitsSofterThanADirectDose) {
    EffectSet full;
    EffectInstance strong = makeEffect(EffectKind::InstantDamage, 1, 0);
    applyEffect(full, strong);
    float fullHealth = 20.0f;
    applyEffectTick(tickEffects(full), fullHealth, 20.0f);
    CHECK_NEAR(fullHealth, 14.0f, 0.001f);

    EffectSet weak;
    EffectInstance grazed = strong;
    grazed.potency = 0.5f;
    applyEffect(weak, grazed);
    float weakHealth = 20.0f;
    applyEffectTick(tickEffects(weak), weakHealth, 20.0f);
    CHECK_NEAR(weakHealth, 17.0f, 0.001f);
}

VF_TEST(fermentedSpiderEyeInversionMapsEffectsToTheirOpposites) {
    CHECK(invertedEffect(EffectKind::Speed) == EffectKind::Slowness);
    CHECK(invertedEffect(EffectKind::InstantHealth) == EffectKind::InstantDamage);
    CHECK(invertedEffect(EffectKind::Poison) == EffectKind::InstantDamage);
    CHECK(invertedEffect(EffectKind::NightVision) == EffectKind::Invisibility);
    CHECK(invertedEffect(EffectKind::Regeneration) == EffectKind::Count);
    CHECK(isHarmfulEffect(EffectKind::Wither));
    CHECK(!isHarmfulEffect(EffectKind::Regeneration));
    CHECK(isInstantEffect(EffectKind::InstantDamage));
}

VF_TEST(absorptionAndHealthBoostRaiseTheCeilingThroughModifiers) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::HealthBoost, 2, 400));
    applyEffect(set, makeEffect(EffectKind::Absorption, 3, 400));
    EffectModifier modifier = effectModifiers(set);
    CHECK_NEAR(modifier.maxHealthBonus, 8.0f, 0.001f);
    CHECK_NEAR(modifier.absorptionHearts, 12.0f, 0.001f);

    applyEffect(set, makeEffect(EffectKind::FireResistance, 1, 400));
    applyEffect(set, makeEffect(EffectKind::NightVision, 1, 400));
    modifier = effectModifiers(set);
    CHECK(modifier.fireImmune);
    CHECK(modifier.nightVision);
    CHECK(!modifier.waterBreathing);
}

VF_TEST(regenerationAndPoisonTogetherLeaveHealthDrifting) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::Regeneration, 1, 600));
    applyEffect(set, makeEffect(EffectKind::Poison, 1, 600));

    float health = 10.0f;
    for (int tick = 0; tick < 600; ++tick) {
        EffectSet::Pending pending = tickEffects(set);
        applyEffectTick(pending, health, 20.0f);
    }
    CHECK_NEAR(health, 1.0f, 0.001f);
    CHECK_EQ(static_cast<int>(set.active.size()), 0);
}

VF_TEST(removingAndClearingEffectsDropsThemImmediately) {
    EffectSet set;
    applyEffect(set, makeEffect(EffectKind::Strength, 2, 300));
    applyEffect(set, makeEffect(EffectKind::Weakness, 1, 300));
    CHECK_NEAR(effectModifiers(set).attackDamageBonus, 2.0f, 0.001f);

    CHECK(removeEffect(set, EffectKind::Weakness));
    CHECK(!removeEffect(set, EffectKind::Weakness));
    CHECK_NEAR(effectModifiers(set).attackDamageBonus, 6.0f, 0.001f);

    clearEffects(set);
    CHECK_EQ(static_cast<int>(set.active.size()), 0);
    CHECK_NEAR(effectModifiers(set).attackDamageBonus, 0.0f, 0.001f);
}
