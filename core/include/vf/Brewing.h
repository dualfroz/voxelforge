#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Effect.h"

namespace vf {

constexpr int brewingBottleSlots = 3;
constexpr int brewDurationTicks = 400;
constexpr int brewsPerBlazePowder = 20;

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
    enum class Form : uint8_t { Drinkable, Splash, Lingering };

    PotionKind kind = PotionKind::Water;
    Form form = Form::Drinkable;
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
    int fuel = 0;
    int progressTicks = 0;
    bool brewing = false;
};

const std::vector<BrewRecipe>& brewRecipes();

PotionItem brewResult(const PotionItem& base, BrewRecipe::Ingredient ingredient, bool& changed);

bool setBottle(BrewingStandState& stand, int slot, const PotionItem& potion);
PotionItem takeBottle(BrewingStandState& stand, int slot, bool& taken);

bool setIngredient(BrewingStandState& stand, BrewRecipe::Ingredient ingredient, int count);
bool addFuel(BrewingStandState& stand, int blazePowderCount);

bool canBrew(const BrewingStandState& stand);

bool tickBrewing(BrewingStandState& stand);

bool potionHasEffect(const PotionItem& potion);

EffectInstance potionEffect(const PotionItem& potion);

bool drinkPotion(EffectSet& target, const PotionItem& potion);

float splashPotency(float distance, float radius);

bool splashPotion(EffectSet& target, const PotionItem& potion, float distance, float radius);

}
