#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Effect.h"

namespace vf {

// A stand holds three bottles, one ingredient stack and a blaze powder fuel bar.
// Every completed brew converts all three bottles at once and spends a single
// unit of ingredient and a single unit of fuel.
constexpr int brewingBottleSlots = 3;
constexpr int brewDurationTicks = 400;
constexpr int brewsPerBlazePowder = 20;

// Ordering is load bearing: the profile table in Brewing.cpp is indexed by this
// enum and static_asserts against Count.
enum class PotionKind : uint8_t {
    Water,
    Mundane,
    Thick,
    Awkward,
    Healing,
    Harming,
    Regeneration,
    Poison,
    Swiftness,
    Slowness,
    Leaping,
    Strength,
    Weakness,
    FireResistance,
    WaterBreathing,
    NightVision,
    Invisibility,
    Count
};

struct PotionItem {
    // Gunpowder turns a drinkable into a splash. Dragon breath turns a splash
    // into a lingering cloud, which the stand itself cannot do: see
    // catalogBrewStep in PotionCatalog.h.
    enum class Form : uint8_t { Drinkable, Splash, Lingering };

    PotionKind kind = PotionKind::Water;
    Form form = Form::Drinkable;
    // Glowstone raises the level and shortens the potion, redstone lengthens it
    // at level one. The two are mutually exclusive, the later one wins.
    bool upgraded = false;
    bool extended = false;

    bool operator==(const PotionItem& other) const {
        return kind == other.kind && form == other.form && upgraded == other.upgraded &&
               extended == other.extended;
    }
    bool operator!=(const PotionItem& other) const { return !(*this == other); }
};

struct BrewRecipe {
    enum class Ingredient : uint8_t {
        None,
        NetherWart,
        Redstone,
        Glowstone,
        Gunpowder,
        DragonBreath,
        FermentedSpiderEye,
        SpiderEye,
        GhastTear,
        Sugar,
        RabbitFoot,
        BlazePowder,
        GlisteringMelon,
        MagmaCream,
        PufferFish,
        GoldenCarrot,
        Count
    };

    PotionKind input = PotionKind::Water;
    Ingredient ingredient = Ingredient::None;
    PotionKind output = PotionKind::Water;
};

struct BrewingSlot {
    PotionItem potion;
    bool occupied = false;
};

struct BrewingStandState {
    std::array<BrewingSlot, brewingBottleSlots> bottles{};
    BrewRecipe::Ingredient ingredient = BrewRecipe::Ingredient::None;
    int ingredientCount = 0;
    // Measured in brews, not in items: addFuel multiplies by brewsPerBlazePowder.
    int fuel = 0;
    int progressTicks = 0;
    bool brewing = false;
};

// Every kind-changing recipe the stand knows. Modifier ingredients (glowstone,
// redstone, gunpowder) are not recipes and do not appear here.
const std::vector<BrewRecipe>& brewRecipes();

// Resolves one ingredient against one bottle. A kind change wipes the glowstone
// and redstone modifiers but keeps the form. Sets `changed` to false when the
// ingredient does nothing to that bottle, in which case the input is returned
// unaltered.
PotionItem brewResult(const PotionItem& base, BrewRecipe::Ingredient ingredient, bool& changed);

// Fails when the slot index is out of range or already holds a bottle.
bool setBottle(BrewingStandState& stand, int slot, const PotionItem& potion);
PotionItem takeBottle(BrewingStandState& stand, int slot, bool& taken);

// Stacks onto the existing ingredient when it matches, refuses a different one
// while the current stack is not empty.
bool setIngredient(BrewingStandState& stand, BrewRecipe::Ingredient ingredient, int count);
bool addFuel(BrewingStandState& stand, int blazePowderCount);

// True when fuel, ingredient and at least one bottle that the ingredient would
// actually change are all present.
bool canBrew(const BrewingStandState& stand);

// One tick of the stand. Returns true on the tick the brew completes, which is
// the brewDurationTicks-th consecutive tick with canBrew holding. Any tick where
// canBrew fails resets the progress bar to zero.
bool tickBrewing(BrewingStandState& stand);

// Water, mundane, thick and awkward carry no effect.
bool potionHasEffect(const PotionItem& potion);

// The effect a bottle would apply, with the level and duration its glowstone or
// redstone modifier calls for. Returns an EffectKind::Count instance for the
// four effectless kinds.
EffectInstance potionEffect(const PotionItem& potion);

// Drinkable form only. Returns whatever applyEffect reports.
bool drinkPotion(EffectSet& target, const PotionItem& potion);

// Linear falloff from the impact point, clamped to 0..1.
float splashPotency(float distance, float radius);

// Splash form only. Instant effects scale their magnitude by the potency, timed
// effects keep their level and lose duration: three quarters at the impact
// point, less further out.
bool splashPotion(EffectSet& target, const PotionItem& potion, float distance, float radius);

} // namespace vf
