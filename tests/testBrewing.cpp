#include <vector>

#include "testHarness.h"
#include "vf/Brewing.h"

using namespace vf;

namespace {

using Ingredient = BrewRecipe::Ingredient;

BrewingStandState fueledStand(const PotionItem& bottle, int blazePowder = 5) {
    BrewingStandState stand;
    setBottle(stand, 0, bottle);
    addFuel(stand, blazePowder);
    return stand;
}

PotionItem brewOnce(BrewingStandState& stand, Ingredient ingredient) {
    setIngredient(stand, ingredient, 1);
    for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);
    return stand.bottles[0].potion;
}

PotionItem brewChain(const PotionItem& start, const std::vector<Ingredient>& chain) {
    BrewingStandState stand = fueledStand(start, 10);
    PotionItem bottle = start;
    for (Ingredient ingredient : chain) bottle = brewOnce(stand, ingredient);
    return bottle;
}

PotionItem potionOf(PotionKind kind) {
    PotionItem potion;
    potion.kind = kind;
    return potion;
}

int durationOf(PotionKind kind, bool upgraded, bool extended) {
    PotionItem potion = potionOf(kind);
    potion.upgraded = upgraded;
    potion.extended = extended;
    return potionEffect(potion).durationTicks;
}

int levelOf(PotionKind kind, bool upgraded, bool extended) {
    PotionItem potion = potionOf(kind);
    potion.upgraded = upgraded;
    potion.extended = extended;
    return potionEffect(potion).level();
}

}

VF_TEST(theStandsPublishedConstantsAreTheOnesTheRestOfTheEngineWasBuiltAgainst) {
    CHECK_EQ(brewingBottleSlots, 3);
    CHECK_EQ(brewDurationTicks, 400);
    CHECK_EQ(brewDurationTicks, 20 * ticksPerSecond);
    CHECK_EQ(brewsPerBlazePowder, 20);
    CHECK_EQ(static_cast<int>(PotionKind::Count), 17);
    CHECK_EQ(static_cast<int>(BrewRecipe::Ingredient::None), 0);

    BrewingStandState stand;
    setBottle(stand, 0, PotionItem{});
    addFuel(stand, 1);
    setIngredient(stand, Ingredient::NetherWart, 1);
    CHECK_EQ(stand.fuel, 20);

    int completedOn = 0;
    for (int tick = 1; tick <= 500; ++tick) {
        if (tickBrewing(stand) && completedOn == 0) completedOn = tick;
    }
    CHECK_EQ(completedOn, 400);
    CHECK_EQ(stand.fuel, 19);
}

VF_TEST(recipeTableHasNoDuplicateInputAndIngredientPairs) {
    const std::vector<BrewRecipe>& recipes = brewRecipes();
    CHECK(recipes.size() >= 19);

    int collisions = 0;
    int selfLoops = 0;
    int emptyIngredients = 0;
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        if (recipes[i].ingredient == Ingredient::None) ++emptyIngredients;
        if (recipes[i].input == recipes[i].output) ++selfLoops;
        for (std::size_t j = i + 1; j < recipes.size(); ++j) {
            if (recipes[i].input == recipes[j].input &&
                recipes[i].ingredient == recipes[j].ingredient) {
                ++collisions;
            }
        }
    }
    CHECK_EQ(collisions, 0);
    CHECK_EQ(selfLoops, 0);
    CHECK_EQ(emptyIngredients, 0);
}

VF_TEST(netherWartTurnsWaterIntoAwkwardAndAwkwardOpensEveryBasePotion) {
    bool changed = false;
    CHECK_EQ(brewResult(PotionItem{}, Ingredient::NetherWart, changed).kind, PotionKind::Awkward);
    CHECK(changed);

    const struct { Ingredient ingredient; PotionKind expected; } branches[] = {
        {Ingredient::Sugar, PotionKind::Swiftness},
        {Ingredient::GlisteringMelon, PotionKind::Healing},
        {Ingredient::SpiderEye, PotionKind::Poison},
        {Ingredient::BlazePowder, PotionKind::Strength},
        {Ingredient::MagmaCream, PotionKind::FireResistance},
        {Ingredient::PufferFish, PotionKind::WaterBreathing},
        {Ingredient::GoldenCarrot, PotionKind::NightVision},
        {Ingredient::GhastTear, PotionKind::Regeneration},
        {Ingredient::RabbitFoot, PotionKind::Leaping},
    };
    for (const auto& branch : branches) {
        bool branched = false;
        PotionItem brewed = brewResult(potionOf(PotionKind::Awkward), branch.ingredient, branched);
        CHECK(branched);
        CHECK_EQ(brewed.kind, branch.expected);
    }
}

VF_TEST(aStandTurnsWaterIntoAwkwardOnTheFourHundredthTickAndNotOneTickEarlier) {
    BrewingStandState stand = fueledStand(PotionItem{}, 1);
    CHECK_EQ(stand.fuel, brewsPerBlazePowder);
    CHECK(setIngredient(stand, Ingredient::NetherWart, 1));

    bool completedEarly = false;
    for (int tick = 1; tick < brewDurationTicks; ++tick) {
        if (tickBrewing(stand)) completedEarly = true;
    }
    CHECK(!completedEarly);
    CHECK(stand.brewing);
    CHECK_EQ(stand.progressTicks, brewDurationTicks - 1);
    CHECK_EQ(stand.bottles[0].potion.kind, PotionKind::Water);

    CHECK(tickBrewing(stand));
    CHECK_EQ(stand.bottles[0].potion.kind, PotionKind::Awkward);
    CHECK_EQ(stand.progressTicks, 0);
    CHECK(!stand.brewing);
    CHECK_EQ(stand.fuel, brewsPerBlazePowder - 1);
    CHECK_EQ(stand.ingredientCount, 0);
    CHECK_EQ(stand.ingredient, Ingredient::None);
}

VF_TEST(theFullWaterToUpgradedSplashStrengthCycleRunsOnOneStand) {
    BrewingStandState stand = fueledStand(PotionItem{}, 1);

    CHECK_EQ(brewOnce(stand, Ingredient::NetherWart).kind, PotionKind::Awkward);
    CHECK_EQ(brewOnce(stand, Ingredient::BlazePowder).kind, PotionKind::Strength);

    PotionItem upgraded = brewOnce(stand, Ingredient::Glowstone);
    CHECK(upgraded.upgraded);
    CHECK(!upgraded.extended);

    PotionItem splash = brewOnce(stand, Ingredient::Gunpowder);
    CHECK_EQ(splash.form, PotionItem::Form::Splash);
    CHECK_EQ(splash.kind, PotionKind::Strength);
    CHECK(splash.upgraded);

    CHECK_EQ(stand.fuel, brewsPerBlazePowder - 4);

    EffectInstance effect = potionEffect(splash);
    CHECK_EQ(effect.kind, EffectKind::Strength);
    CHECK_EQ(effect.level(), 2);
    CHECK_EQ(effect.durationTicks, 1800);
}

VF_TEST(glowstoneRaisesTheLevelAndShortensWhileRedstoneOnlyLengthens) {
    PotionItem plain = potionOf(PotionKind::Swiftness);
    PotionItem upgraded = brewChain(plain, {Ingredient::Glowstone});
    PotionItem extended = brewChain(plain, {Ingredient::Redstone});

    CHECK_EQ(potionEffect(plain).level(), 1);
    CHECK_EQ(potionEffect(plain).durationTicks, 3600);

    CHECK(upgraded.upgraded);
    CHECK_EQ(potionEffect(upgraded).level(), 2);
    CHECK_EQ(potionEffect(upgraded).durationTicks, 1800);

    CHECK(extended.extended);
    CHECK_EQ(potionEffect(extended).level(), 1);
    CHECK_EQ(potionEffect(extended).durationTicks, 9600);
}

VF_TEST(glowstoneAndRedstoneReplaceEachOtherAndNeverStack) {
    BrewingStandState stand = fueledStand(potionOf(PotionKind::Swiftness), 5);

    PotionItem extended = brewOnce(stand, Ingredient::Redstone);
    CHECK(extended.extended);

    PotionItem upgraded = brewOnce(stand, Ingredient::Glowstone);
    CHECK(upgraded.upgraded);
    CHECK(!upgraded.extended);

    PotionItem backToExtended = brewOnce(stand, Ingredient::Redstone);
    CHECK(backToExtended.extended);
    CHECK(!backToExtended.upgraded);

    bool changed = false;
    brewResult(backToExtended, Ingredient::Redstone, changed);
    CHECK(!changed);
}

VF_TEST(potionsThatIgnoreAModifierRejectItInsteadOfSilentlyConsumingTheBrew) {
    const PotionKind noGlowstone[] = {PotionKind::Weakness, PotionKind::FireResistance,
                                      PotionKind::WaterBreathing, PotionKind::NightVision,
                                      PotionKind::Invisibility};
    for (PotionKind kind : noGlowstone) {
        bool changed = false;
        brewResult(potionOf(kind), Ingredient::Glowstone, changed);
        CHECK(!changed);
    }

    const PotionKind noRedstone[] = {PotionKind::Healing, PotionKind::Harming};
    for (PotionKind kind : noRedstone) {
        bool changed = false;
        brewResult(potionOf(kind), Ingredient::Redstone, changed);
        CHECK(!changed);
    }

    bool healingUpgrades = false;
    PotionItem strongHealing = brewResult(potionOf(PotionKind::Healing), Ingredient::Glowstone,
                                          healingUpgrades);
    CHECK(healingUpgrades);
    CHECK_EQ(potionEffect(strongHealing).level(), 2);
}

VF_TEST(fermentedSpiderEyeCorruptsAlongTheWholeInversionChain) {
    const struct { PotionKind from; PotionKind to; } inversions[] = {
        {PotionKind::Water, PotionKind::Weakness},
        {PotionKind::NightVision, PotionKind::Invisibility},
        {PotionKind::Healing, PotionKind::Harming},
        {PotionKind::Poison, PotionKind::Harming},
        {PotionKind::Swiftness, PotionKind::Slowness},
        {PotionKind::Leaping, PotionKind::Slowness},
        {PotionKind::Strength, PotionKind::Weakness},
    };
    for (const auto& inversion : inversions) {
        bool changed = false;
        PotionItem corrupted = brewResult(potionOf(inversion.from), Ingredient::FermentedSpiderEye,
                                          changed);
        CHECK(changed);
        CHECK_EQ(corrupted.kind, inversion.to);
    }

    const PotionKind uncorruptible[] = {PotionKind::Regeneration, PotionKind::FireResistance,
                                        PotionKind::WaterBreathing, PotionKind::Awkward};
    for (PotionKind kind : uncorruptible) {
        bool changed = false;
        brewResult(potionOf(kind), Ingredient::FermentedSpiderEye, changed);
        CHECK(!changed);
    }
}

VF_TEST(aKindChangeWipesTheModifiersButKeepsTheSplashForm) {
    PotionItem source = potionOf(PotionKind::Swiftness);
    source.upgraded = true;
    source.form = PotionItem::Form::Splash;

    bool changed = false;
    PotionItem corrupted = brewResult(source, Ingredient::FermentedSpiderEye, changed);
    CHECK(changed);
    CHECK_EQ(corrupted.kind, PotionKind::Slowness);
    CHECK(!corrupted.upgraded);
    CHECK(!corrupted.extended);
    CHECK_EQ(corrupted.form, PotionItem::Form::Splash);
}

VF_TEST(brewResultLeavesTheInputAloneWhenTheIngredientDoesNothing) {
    PotionItem source = potionOf(PotionKind::Regeneration);
    source.extended = true;

    bool changed = true;
    PotionItem untouched = brewResult(source, Ingredient::SpiderEye, changed);
    CHECK(!changed);
    CHECK(untouched == source);

    changed = true;
    PotionItem noIngredient = brewResult(source, Ingredient::None, changed);
    CHECK(!changed);
    CHECK(noIngredient == source);
}

VF_TEST(theStandConvertsEveryOccupiedBottleInASinglePass) {
    BrewingStandState stand;
    CHECK(setBottle(stand, 0, PotionItem{}));
    CHECK(setBottle(stand, 1, PotionItem{}));
    CHECK(setBottle(stand, 2, PotionItem{}));
    addFuel(stand, 1);
    setIngredient(stand, Ingredient::NetherWart, 1);

    for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);

    for (const BrewingSlot& bottle : stand.bottles) {
        CHECK(bottle.occupied);
        CHECK_EQ(bottle.potion.kind, PotionKind::Awkward);
    }
    CHECK_EQ(stand.fuel, brewsPerBlazePowder - 1);
}

VF_TEST(bottlesTheIngredientCannotChangeAreLeftAsTheyWere) {
    BrewingStandState stand;
    setBottle(stand, 0, potionOf(PotionKind::Awkward));
    setBottle(stand, 1, PotionItem{});
    addFuel(stand, 1);
    setIngredient(stand, Ingredient::Sugar, 1);

    CHECK(canBrew(stand));
    for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);

    CHECK_EQ(stand.bottles[0].potion.kind, PotionKind::Swiftness);
    CHECK_EQ(stand.bottles[1].potion.kind, PotionKind::Water);
    CHECK(!stand.bottles[2].occupied);
}

VF_TEST(theStandRefusesAMixedIngredientStackButTakesMoreOfTheSame) {
    BrewingStandState stand = fueledStand(PotionItem{}, 1);

    CHECK(setIngredient(stand, Ingredient::Sugar, 2));
    CHECK_EQ(stand.ingredientCount, 2);
    CHECK(!setIngredient(stand, Ingredient::Redstone, 1));
    CHECK_EQ(stand.ingredient, Ingredient::Sugar);
    CHECK_EQ(stand.ingredientCount, 2);

    CHECK(setIngredient(stand, Ingredient::Sugar, 3));
    CHECK_EQ(stand.ingredientCount, 5);

    CHECK(!setIngredient(stand, Ingredient::None, 1));
    CHECK(!setIngredient(stand, Ingredient::Sugar, 0));
    CHECK(!setIngredient(stand, Ingredient::Sugar, -4));
}

VF_TEST(everyCompletedBrewSpendsExactlyOneIngredientAndOneFuelCharge) {
    BrewingStandState stand;
    setBottle(stand, 0, PotionItem{});
    addFuel(stand, 1);
    setIngredient(stand, Ingredient::NetherWart, 3);

    for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);
    CHECK_EQ(stand.ingredientCount, 2);
    CHECK_EQ(stand.ingredient, Ingredient::NetherWart);
    CHECK_EQ(stand.fuel, brewsPerBlazePowder - 1);

    CHECK(!canBrew(stand));
    for (int tick = 0; tick < brewDurationTicks; ++tick) CHECK(!tickBrewing(stand));
    CHECK_EQ(stand.ingredientCount, 2);
    CHECK_EQ(stand.fuel, brewsPerBlazePowder - 1);
}

VF_TEST(fuelIsCountedInBrewsAndOneBlazePowderBuysTwenty) {
    BrewingStandState stand;
    CHECK(!addFuel(stand, 0));
    CHECK(!addFuel(stand, -3));
    CHECK_EQ(stand.fuel, 0);

    CHECK(addFuel(stand, 1));
    CHECK_EQ(stand.fuel, brewsPerBlazePowder);
    CHECK(addFuel(stand, 2));
    CHECK_EQ(stand.fuel, 3 * brewsPerBlazePowder);
}

VF_TEST(anEmptyFuelBarStopsTheStandEvenWithAValidRecipeLoaded) {
    BrewingStandState stand;
    setBottle(stand, 0, PotionItem{});
    setIngredient(stand, Ingredient::NetherWart, 1);
    CHECK(!canBrew(stand));
    CHECK(!tickBrewing(stand));
    CHECK_EQ(stand.progressTicks, 0);

    stand.fuel = 1;
    CHECK(canBrew(stand));
    for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);
    CHECK_EQ(stand.fuel, 0);
    CHECK_EQ(stand.bottles[0].potion.kind, PotionKind::Awkward);

    setIngredient(stand, Ingredient::Sugar, 1);
    CHECK(!canBrew(stand));
    CHECK(!tickBrewing(stand));
}

VF_TEST(pullingABottleMidBrewThrowsAwayTheProgressBar) {
    BrewingStandState stand = fueledStand(PotionItem{}, 1);
    setIngredient(stand, Ingredient::NetherWart, 1);
    for (int tick = 0; tick < 200; ++tick) tickBrewing(stand);
    CHECK_EQ(stand.progressTicks, 200);

    bool taken = false;
    PotionItem pulled = takeBottle(stand, 0, taken);
    CHECK(taken);
    CHECK_EQ(pulled.kind, PotionKind::Water);
    CHECK_EQ(stand.progressTicks, 0);
    CHECK(!stand.bottles[0].occupied);
    CHECK(!tickBrewing(stand));

    CHECK(setBottle(stand, 0, PotionItem{}));
    for (int tick = 1; tick < brewDurationTicks; ++tick) {
        CHECK_EQ(tickBrewing(stand), false);
    }
    CHECK(tickBrewing(stand));
}

VF_TEST(bottleSlotsRejectOutOfRangeIndicesAndDoubleFilling) {
    BrewingStandState stand;
    CHECK(!setBottle(stand, -1, PotionItem{}));
    CHECK(!setBottle(stand, brewingBottleSlots, PotionItem{}));
    CHECK(setBottle(stand, 0, PotionItem{}));
    CHECK(!setBottle(stand, 0, potionOf(PotionKind::Awkward)));
    CHECK_EQ(stand.bottles[0].potion.kind, PotionKind::Water);

    bool taken = true;
    takeBottle(stand, 1, taken);
    CHECK(!taken);
    taken = true;
    takeBottle(stand, brewingBottleSlots, taken);
    CHECK(!taken);
}

VF_TEST(theFourBasePotionsCarryNoEffectAndCannotBeDrunkForOne) {
    const PotionKind effectless[] = {PotionKind::Water, PotionKind::Mundane, PotionKind::Thick,
                                     PotionKind::Awkward};
    for (PotionKind kind : effectless) {
        PotionItem potion = potionOf(kind);
        CHECK(!potionHasEffect(potion));
        CHECK_EQ(potionEffect(potion).kind, EffectKind::Count);

        EffectSet target;
        CHECK(!drinkPotion(target, potion));
        CHECK(target.active.empty());
    }
}

VF_TEST(drinkingWorksOnlyInDrinkableFormAndLandsTheCatalogDuration) {
    EffectSet target;
    PotionItem regeneration = potionOf(PotionKind::Regeneration);
    CHECK(drinkPotion(target, regeneration));
    CHECK_EQ(effectLevel(target, EffectKind::Regeneration), 1);
    CHECK_EQ(effectDuration(target, EffectKind::Regeneration), 900);

    PotionItem splash = regeneration;
    splash.form = PotionItem::Form::Splash;
    EffectSet other;
    CHECK(!drinkPotion(other, splash));
    CHECK(other.active.empty());

    PotionItem strong = regeneration;
    strong.upgraded = true;
    CHECK(drinkPotion(target, strong));
    CHECK_EQ(effectLevel(target, EffectKind::Regeneration), 2);
    CHECK_EQ(effectDuration(target, EffectKind::Regeneration), 440);
}

VF_TEST(splashPotencyFallsOffLinearlyAndClampsAtBothEnds) {
    CHECK_NEAR(splashPotency(0.0f, 4.0f), 1.0f, 0.0001f);
    CHECK_NEAR(splashPotency(1.0f, 4.0f), 0.75f, 0.0001f);
    CHECK_NEAR(splashPotency(4.0f, 4.0f), 0.0f, 0.0001f);
    CHECK_NEAR(splashPotency(9.0f, 4.0f), 0.0f, 0.0001f);
    CHECK_NEAR(splashPotency(-2.0f, 4.0f), 1.0f, 0.0001f);
    CHECK_NEAR(splashPotency(1.0f, 0.0f), 0.0f, 0.0001f);
}

VF_TEST(aSplashTimedPotionKeepsItsLevelAndLosesDurationWithDistance) {
    PotionItem splash = potionOf(PotionKind::Swiftness);
    splash.form = PotionItem::Form::Splash;

    EffectSet atImpact;
    CHECK(splashPotion(atImpact, splash, 0.0f, 4.0f));
    CHECK_EQ(effectLevel(atImpact, EffectKind::Speed), 1);
    CHECK_EQ(effectDuration(atImpact, EffectKind::Speed), 2700);

    EffectSet halfway;
    CHECK(splashPotion(halfway, splash, 2.0f, 4.0f));
    CHECK_EQ(effectDuration(halfway, EffectKind::Speed), 1350);

    EffectSet outOfRange;
    CHECK(!splashPotion(outOfRange, splash, 4.0f, 4.0f));
    CHECK(outOfRange.active.empty());

    EffectSet wrongForm;
    CHECK(!splashPotion(wrongForm, potionOf(PotionKind::Swiftness), 0.0f, 4.0f));
}

VF_TEST(aSplashInstantPotionScalesItsPotencyInsteadOfItsDuration) {
    PotionItem harming = potionOf(PotionKind::Harming);
    harming.form = PotionItem::Form::Splash;

    EffectSet target;
    CHECK(splashPotion(target, harming, 3.0f, 4.0f));
    CHECK_EQ(target.active.size(), std::size_t{1});
    CHECK_EQ(target.active[0].kind, EffectKind::InstantDamage);
    CHECK_EQ(target.active[0].durationTicks, 1);
    CHECK_NEAR(target.active[0].potency, 0.25f, 0.0001f);

    PotionItem strong = harming;
    strong.upgraded = true;
    EffectSet direct;
    CHECK(splashPotion(direct, strong, 0.0f, 4.0f));
    CHECK_EQ(direct.active[0].level(), 2);
    CHECK_NEAR(direct.active[0].potency, 1.0f, 0.0001f);
}

VF_TEST(everyPotionKindKeepsTheLevelAndDurationInvariantsOfItsModifiers) {
    for (int index = 0; index < static_cast<int>(PotionKind::Count); ++index) {
        PotionKind kind = static_cast<PotionKind>(index);
        PotionItem plain = potionOf(kind);
        EffectInstance base = potionEffect(plain);

        CHECK_EQ(potionHasEffect(plain), base.kind != EffectKind::Count);
        if (!potionHasEffect(plain)) {
            CHECK_EQ(base.durationTicks, 0);
            continue;
        }

        CHECK(base.durationTicks > 0);
        CHECK_EQ(base.level(), 1);

        int upgradedDuration = durationOf(kind, true, false);
        int extendedDuration = durationOf(kind, false, true);
        CHECK(extendedDuration >= base.durationTicks);
        CHECK(levelOf(kind, false, true) == 1);

        int upgradedLevel = levelOf(kind, true, false);
        CHECK(upgradedLevel >= 1);
        if (upgradedLevel > 1 && !isInstantEffect(base.kind)) {
            CHECK(upgradedDuration < base.durationTicks);
        }
        if (isInstantEffect(base.kind)) {
            CHECK_EQ(base.durationTicks, 1);
            CHECK_EQ(upgradedDuration, 1);
        }
    }
}
