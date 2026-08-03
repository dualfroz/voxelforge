#include "vf/PotionCatalog.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace vf {

namespace {

using Ingredient = BrewRecipe::Ingredient;

constexpr int potionKindCount = static_cast<int>(PotionKind::Count);
constexpr int potionFormCount = 3;
constexpr int potionModifierCount = 3;

PotionEntry basePotion(PotionKind kind, const char* name, PotionKind base, Ingredient ingredient) {
    PotionEntry entry;
    entry.kind = kind;
    entry.name = name;
    entry.base = base;
    entry.ingredient = ingredient;
    return entry;
}

PotionEntry timedPotion(PotionKind kind, const char* name, Ingredient ingredient, EffectKind effect,
                        int normalTicks, int extendedTicks, int upgradedTicks, int upgradedLevel) {
    PotionEntry entry;
    entry.kind = kind;
    entry.name = name;
    entry.base = PotionKind::Awkward;
    entry.ingredient = ingredient;
    entry.effect = effect;
    entry.normalTicks = normalTicks;
    entry.extendedTicks = extendedTicks;
    entry.upgradedTicks = upgradedTicks;
    entry.upgradedLevel = upgradedLevel;
    entry.harmful = isHarmfulEffect(effect);
    return entry;
}

PotionEntry instantPotion(PotionKind kind, const char* name, PotionKind base, Ingredient ingredient,
                          EffectKind effect) {
    PotionEntry entry;
    entry.kind = kind;
    entry.name = name;
    entry.base = base;
    entry.ingredient = ingredient;
    entry.effect = effect;
    entry.normalTicks = 1;
    entry.upgradedTicks = 1;
    entry.upgradedLevel = 2;
    entry.harmful = isHarmfulEffect(effect);
    entry.instant = true;
    return entry;
}

// The durations mirror the profile table the stand keeps privately; the catalog
// tests hold the two against each other rather than assuming they agree.
std::vector<PotionEntry> buildRows() {
    std::vector<PotionEntry> rows;
    rows.push_back(basePotion(PotionKind::Water, "water", PotionKind::Water, Ingredient::None));
    rows.push_back(basePotion(PotionKind::Mundane, "mundane", PotionKind::Water,
                              Ingredient::Redstone));
    rows.push_back(basePotion(PotionKind::Thick, "thick", PotionKind::Water,
                              Ingredient::Glowstone));
    rows.push_back(basePotion(PotionKind::Awkward, "awkward", PotionKind::Water,
                              Ingredient::NetherWart));

    rows.push_back(instantPotion(PotionKind::Healing, "healing", PotionKind::Awkward,
                                 Ingredient::GlisteringMelon, EffectKind::InstantHealth));
    rows.push_back(instantPotion(PotionKind::Harming, "harming", PotionKind::Healing,
                                 Ingredient::FermentedSpiderEye, EffectKind::InstantDamage));

    rows.push_back(timedPotion(PotionKind::Regeneration, "regeneration", Ingredient::GhastTear,
                               EffectKind::Regeneration, 900, 1800, 440, 2));
    rows.push_back(timedPotion(PotionKind::Poison, "poison", Ingredient::SpiderEye,
                               EffectKind::Poison, 900, 1800, 432, 2));
    rows.push_back(timedPotion(PotionKind::Swiftness, "swiftness", Ingredient::Sugar,
                               EffectKind::Speed, 3600, 9600, 1800, 2));

    PotionEntry slowness = timedPotion(PotionKind::Slowness, "slowness",
                                       Ingredient::FermentedSpiderEye, EffectKind::Slowness, 1800,
                                       4800, 400, 4);
    slowness.base = PotionKind::Swiftness;
    rows.push_back(slowness);

    rows.push_back(timedPotion(PotionKind::Leaping, "leaping", Ingredient::RabbitFoot,
                               EffectKind::JumpBoost, 3600, 9600, 1800, 2));
    rows.push_back(timedPotion(PotionKind::Strength, "strength", Ingredient::BlazePowder,
                               EffectKind::Strength, 3600, 9600, 1800, 2));

    PotionEntry weakness = timedPotion(PotionKind::Weakness, "weakness",
                                       Ingredient::FermentedSpiderEye, EffectKind::Weakness, 1800,
                                       4800, 0, 1);
    weakness.base = PotionKind::Water;
    rows.push_back(weakness);

    rows.push_back(timedPotion(PotionKind::FireResistance, "fireResistance", Ingredient::MagmaCream,
                               EffectKind::FireResistance, 3600, 9600, 0, 1));
    rows.push_back(timedPotion(PotionKind::WaterBreathing, "waterBreathing", Ingredient::PufferFish,
                               EffectKind::WaterBreathing, 3600, 9600, 0, 1));
    rows.push_back(timedPotion(PotionKind::NightVision, "nightVision", Ingredient::GoldenCarrot,
                               EffectKind::NightVision, 3600, 9600, 0, 1));

    PotionEntry invisibility = timedPotion(PotionKind::Invisibility, "invisibility",
                                           Ingredient::FermentedSpiderEye, EffectKind::Invisibility,
                                           3600, 9600, 0, 1);
    invisibility.base = PotionKind::NightVision;
    rows.push_back(invisibility);
    return rows;
}

int modifierIndex(const PotionItem& potion) {
    if (potion.upgraded) return 1;
    if (potion.extended) return 2;
    return 0;
}

} // namespace

PotionCatalog::PotionCatalog() : rows(buildRows()) {}

const PotionEntry& PotionCatalog::entry(PotionKind kind) const {
    return rows[static_cast<std::size_t>(kind)];
}

const char* PotionCatalog::name(PotionKind kind) const { return entry(kind).name; }

PotionKind PotionCatalog::kindForEffect(EffectKind effect) const {
    if (effect == EffectKind::Count) return PotionKind::Count;
    for (const PotionEntry& row : rows) {
        if (row.effect == effect) return row.kind;
    }
    return PotionKind::Count;
}

PotionKind PotionCatalog::invertedKind(PotionKind kind) const {
    for (const BrewRecipe& recipe : brewRecipes()) {
        if (recipe.ingredient != Ingredient::FermentedSpiderEye) continue;
        if (recipe.input == kind) return recipe.output;
    }
    return PotionKind::Count;
}

int PotionCatalog::variantLevel(const PotionItem& potion) const {
    const PotionEntry& row = entry(potion.kind);
    if (!row.carriesEffect()) return 0;
    if (potion.upgraded && row.acceptsGlowstone()) return row.upgradedLevel;
    return row.level;
}

int PotionCatalog::variantTicks(const PotionItem& potion) const {
    const PotionEntry& row = entry(potion.kind);
    if (!row.carriesEffect()) return 0;
    if (row.instant) return 1;

