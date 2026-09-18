#include "vf/Brewing.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

using Ingredient = BrewRecipe::Ingredient;

struct PotionProfile {
    EffectKind kind = EffectKind::Count;
    int baseTicks = 0;
    int extendedTicks = 0;
    int upgradedTicks = 0;
    int upgradedLevel = 1;
};

const PotionProfile& profileFor(PotionKind kind) {
    static const PotionProfile profiles[] = {
        {EffectKind::Count, 0, 0, 0, 1},
        {EffectKind::Count, 0, 0, 0, 1},
        {EffectKind::Count, 0, 0, 0, 1},
        {EffectKind::Count, 0, 0, 0, 1},
        {EffectKind::InstantHealth, 1, 0, 1, 2},
        {EffectKind::InstantDamage, 1, 0, 1, 2},
        {EffectKind::Regeneration, 900, 1800, 440, 2},
        {EffectKind::Poison, 900, 1800, 432, 2},
        {EffectKind::Speed, 3600, 9600, 1800, 2},
        {EffectKind::Slowness, 1800, 4800, 400, 4},
        {EffectKind::JumpBoost, 3600, 9600, 1800, 2},
        {EffectKind::Strength, 3600, 9600, 1800, 2},
        {EffectKind::Weakness, 1800, 4800, 0, 1},
        {EffectKind::FireResistance, 3600, 9600, 0, 1},
        {EffectKind::WaterBreathing, 3600, 9600, 0, 1},
        {EffectKind::NightVision, 3600, 9600, 0, 1},
        {EffectKind::Invisibility, 3600, 9600, 0, 1},
    };
    static_assert(sizeof(profiles) / sizeof(profiles[0]) ==
                      static_cast<std::size_t>(PotionKind::Count),
                  "potion profile table must cover every PotionKind");
    return profiles[static_cast<std::size_t>(kind)];
}

bool applyModifier(PotionItem& potion, Ingredient ingredient) {
    const PotionProfile& profile = profileFor(potion.kind);
    switch (ingredient) {
        case Ingredient::Glowstone:
            if (profile.upgradedTicks == 0 || potion.upgraded) return false;
            potion.upgraded = true;
            potion.extended = false;
            return true;
        case Ingredient::Redstone:
            if (profile.extendedTicks == 0 || potion.extended) return false;
            potion.extended = true;
            potion.upgraded = false;
            return true;
        case Ingredient::Gunpowder:
            if (potion.form == PotionItem::Form::Splash) return false;
            potion.form = PotionItem::Form::Splash;
            return true;
        default:
            return false;
    }
}

}

const std::vector<BrewRecipe>& brewRecipes() {
    static const std::vector<BrewRecipe> recipes = {
        {PotionKind::Water, Ingredient::NetherWart, PotionKind::Awkward},
        {PotionKind::Water, Ingredient::Glowstone, PotionKind::Thick},
        {PotionKind::Water, Ingredient::Redstone, PotionKind::Mundane},
        {PotionKind::Water, Ingredient::FermentedSpiderEye, PotionKind::Weakness},

        {PotionKind::Awkward, Ingredient::Sugar, PotionKind::Swiftness},
        {PotionKind::Awkward, Ingredient::GlisteringMelon, PotionKind::Healing},
        {PotionKind::Awkward, Ingredient::SpiderEye, PotionKind::Poison},
        {PotionKind::Awkward, Ingredient::BlazePowder, PotionKind::Strength},
        {PotionKind::Awkward, Ingredient::MagmaCream, PotionKind::FireResistance},
        {PotionKind::Awkward, Ingredient::PufferFish, PotionKind::WaterBreathing},
        {PotionKind::Awkward, Ingredient::GoldenCarrot, PotionKind::NightVision},
        {PotionKind::Awkward, Ingredient::GhastTear, PotionKind::Regeneration},
        {PotionKind::Awkward, Ingredient::RabbitFoot, PotionKind::Leaping},

        {PotionKind::NightVision, Ingredient::FermentedSpiderEye, PotionKind::Invisibility},
        {PotionKind::Healing, Ingredient::FermentedSpiderEye, PotionKind::Harming},
        {PotionKind::Poison, Ingredient::FermentedSpiderEye, PotionKind::Harming},
        {PotionKind::Swiftness, Ingredient::FermentedSpiderEye, PotionKind::Slowness},
        {PotionKind::Leaping, Ingredient::FermentedSpiderEye, PotionKind::Slowness},
        {PotionKind::Strength, Ingredient::FermentedSpiderEye, PotionKind::Weakness},
    };
    return recipes;
}

PotionItem brewResult(const PotionItem& base, Ingredient ingredient, bool& changed) {
    PotionItem brewed = base;
    changed = false;
    if (ingredient == Ingredient::None) return brewed;

    for (const BrewRecipe& recipe : brewRecipes()) {
        if (recipe.input != base.kind || recipe.ingredient != ingredient) continue;
        brewed.kind = recipe.output;
        brewed.upgraded = false;
        brewed.extended = false;
        changed = true;
        return brewed;
    }
    changed = applyModifier(brewed, ingredient);
    return brewed;
}

bool setBottle(BrewingStandState& stand, int slot, const PotionItem& potion) {
    if (slot < 0 || slot >= brewingBottleSlots) return false;
    BrewingSlot& bottle = stand.bottles[static_cast<std::size_t>(slot)];
    if (bottle.occupied) return false;
    bottle.potion = potion;
    bottle.occupied = true;
    stand.progressTicks = 0;
    return true;
}

PotionItem takeBottle(BrewingStandState& stand, int slot, bool& taken) {
    taken = false;
    if (slot < 0 || slot >= brewingBottleSlots) return {};
    BrewingSlot& bottle = stand.bottles[static_cast<std::size_t>(slot)];
    if (!bottle.occupied) return {};
    PotionItem removed = bottle.potion;
    bottle = BrewingSlot{};
    taken = true;
    stand.progressTicks = 0;
    return removed;
}

bool setIngredient(BrewingStandState& stand, Ingredient ingredient, int count) {
    if (ingredient == Ingredient::None || count <= 0) return false;
    if (stand.ingredientCount > 0 && stand.ingredient != ingredient) return false;
    if (stand.ingredient == ingredient) {
        stand.ingredientCount += count;
        return true;
    }
    stand.ingredient = ingredient;
    stand.ingredientCount = count;
    stand.progressTicks = 0;
    return true;
}

bool addFuel(BrewingStandState& stand, int blazePowderCount) {
    if (blazePowderCount <= 0) return false;
    stand.fuel += blazePowderCount * brewsPerBlazePowder;
    return true;
}

bool canBrew(const BrewingStandState& stand) {
    if (stand.fuel <= 0) return false;
    if (stand.ingredient == Ingredient::None || stand.ingredientCount <= 0) return false;
    for (const BrewingSlot& bottle : stand.bottles) {
        if (!bottle.occupied) continue;
        bool changed = false;
        brewResult(bottle.potion, stand.ingredient, changed);
        if (changed) return true;
    }
    return false;
}

bool tickBrewing(BrewingStandState& stand) {
    if (!canBrew(stand)) {
        stand.brewing = false;
        stand.progressTicks = 0;
        return false;
    }
    stand.brewing = true;
    ++stand.progressTicks;
    if (stand.progressTicks < brewDurationTicks) return false;

    for (BrewingSlot& bottle : stand.bottles) {
        if (!bottle.occupied) continue;
        bool changed = false;
        PotionItem brewed = brewResult(bottle.potion, stand.ingredient, changed);
        if (changed) bottle.potion = brewed;
    }
    if (--stand.ingredientCount <= 0) {
        stand.ingredientCount = 0;
        stand.ingredient = Ingredient::None;
    }
    --stand.fuel;
    stand.progressTicks = 0;
    stand.brewing = false;
    return true;
}

bool potionHasEffect(const PotionItem& potion) {
    return profileFor(potion.kind).kind != EffectKind::Count;
}

EffectInstance potionEffect(const PotionItem& potion) {
    const PotionProfile& profile = profileFor(potion.kind);
    if (profile.kind == EffectKind::Count) {
        return EffectInstance{EffectKind::Count, 0, 0, 1.0f, false};
    }

    int level = 1;
    int duration = profile.baseTicks;
    if (potion.upgraded && profile.upgradedTicks > 0) {
        level = profile.upgradedLevel;
        duration = profile.upgradedTicks;
    } else if (potion.extended && profile.extendedTicks > 0) {
        duration = profile.extendedTicks;
    }
    return makeEffect(profile.kind, level, duration);
}

bool drinkPotion(EffectSet& target, const PotionItem& potion) {
    if (potion.form != PotionItem::Form::Drinkable) return false;
    if (!potionHasEffect(potion)) return false;
    return applyEffect(target, potionEffect(potion));
}

float splashPotency(float distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    float potency = 1.0f - distance / radius;
    return std::max(0.0f, std::min(1.0f, potency));
}

bool splashPotion(EffectSet& target, const PotionItem& potion, float distance, float radius) {
    if (potion.form != PotionItem::Form::Splash) return false;
    if (!potionHasEffect(potion)) return false;
    float potency = splashPotency(distance, radius);
    if (potency <= 0.0f) return false;

    EffectInstance effect = potionEffect(potion);
    if (isInstantEffect(effect.kind)) {
        effect.potency = potency;
        return applyEffect(target, effect);
    }
    int ticks =
        static_cast<int>(std::floor(static_cast<float>(effect.durationTicks) * 0.75f * potency));
    if (ticks < 1) return false;
    effect.durationTicks = ticks;
    return applyEffect(target, effect);
}

}
