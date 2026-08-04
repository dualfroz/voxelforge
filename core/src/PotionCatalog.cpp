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

    int ticks = row.normalTicks;
    if (potion.upgraded && row.acceptsGlowstone()) {
        ticks = row.upgradedTicks;
    } else if (potion.extended && row.acceptsRedstone()) {
        ticks = row.extendedTicks;
    }

    if (potion.form == PotionItem::Form::Splash) {
        return static_cast<int>(std::floor(static_cast<float>(ticks) * splashDurationScale));
    }
    if (potion.form == PotionItem::Form::Lingering) {
        return static_cast<int>(std::floor(static_cast<float>(ticks) * lingeringDoseScale));
    }
    return ticks;
}

int PotionCatalog::cloudTicks(const PotionItem& potion) const {
    if (potion.form != PotionItem::Form::Lingering) return 0;
    if (!entry(potion.kind).carriesEffect()) return 0;
    return lingeringCloudTicks;
}

const PotionCatalog& potionCatalog() {
    static const PotionCatalog catalog;
    return catalog;
}

int blazePowderForBrews(int brewCount) {
    if (brewCount <= 0) return 0;
    return (brewCount + brewsPerBlazePowder - 1) / brewsPerBlazePowder;
}

PotionItem catalogBrewStep(const PotionItem& base, Ingredient ingredient, bool& changed) {
    changed = false;
    if (base.form == PotionItem::Form::Lingering) return base;
    if (ingredient == Ingredient::DragonBreath) {
        if (base.form != PotionItem::Form::Splash) return base;
        PotionItem lingering = base;
        lingering.form = PotionItem::Form::Lingering;
        changed = true;
        return lingering;
    }
    return brewResult(base, ingredient, changed);
}

int BrewGraph::stateCount() { return potionKindCount * potionFormCount * potionModifierCount; }

int BrewGraph::stateIndex(const PotionItem& potion) {
    int kind = static_cast<int>(potion.kind);
    int form = static_cast<int>(potion.form);
    return (kind * potionFormCount + form) * potionModifierCount + modifierIndex(potion);
}

PotionItem BrewGraph::stateAt(int index) {
    PotionItem potion;
    int modifier = index % potionModifierCount;
    int rest = index / potionModifierCount;
    potion.form = static_cast<PotionItem::Form>(rest % potionFormCount);
    potion.kind = static_cast<PotionKind>(rest / potionFormCount);
    potion.upgraded = modifier == 1;
    potion.extended = modifier == 2;
    return potion;
}

BrewGraph::BrewGraph(const PotionCatalog& source) : catalog(&source), nodes(stateCount()) {
    int start = stateIndex(PotionItem{});
    nodes[static_cast<std::size_t>(start)].depth = 0;

    std::deque<int> frontier;
    frontier.push_back(start);
    while (!frontier.empty()) {
        int current = frontier.front();
        frontier.pop_front();
        PotionItem bottle = stateAt(current);

        for (int step = 1; step < static_cast<int>(Ingredient::Count); ++step) {
            Ingredient ingredient = static_cast<Ingredient>(step);
            bool changed = false;
            PotionItem brewed = catalogBrewStep(bottle, ingredient, changed);
            if (!changed) continue;

            int next = stateIndex(brewed);
            Node& node = nodes[static_cast<std::size_t>(next)];
            if (node.depth >= 0) continue;
            node.depth = nodes[static_cast<std::size_t>(current)].depth + 1;
            node.previous = current;
            node.via = ingredient;
            frontier.push_back(next);
        }
    }
}

int BrewGraph::depthOf(const PotionItem& potion) const {
    return nodes[static_cast<std::size_t>(stateIndex(potion))].depth;
}

bool BrewGraph::reachable(const PotionItem& potion) const { return depthOf(potion) >= 0; }

BrewPath BrewGraph::shortestPath(const PotionItem& target) const {
    BrewPath path;
    path.target = target;

    int index = stateIndex(target);
    if (nodes[static_cast<std::size_t>(index)].depth < 0) return path;

    path.reachable = true;
    while (nodes[static_cast<std::size_t>(index)].previous >= 0) {
        const Node& node = nodes[static_cast<std::size_t>(index)];
        path.steps.push_back(node.via);
        if (node.via == Ingredient::BlazePowder) ++path.blazePowderIngredient;
        index = node.previous;
    }
    std::reverse(path.steps.begin(), path.steps.end());
    path.blazePowderFuel = blazePowderForBrews(path.stepCount());
    path.totalTicks = path.stepCount() * brewDurationTicks;
    return path;
}

BrewPath BrewGraph::shortestPath(PotionKind kind) const {
    PotionItem bottle;
    bottle.kind = kind;
    return shortestPath(bottle);
}

int BrewGraph::reachableStateCount() const {
    int total = 0;
    for (const Node& node : nodes) {
        if (node.depth >= 0) ++total;
    }
    return total;
}

std::vector<PotionItem> BrewGraph::statesAtDepth(int steps) const {
    std::vector<PotionItem> found;
    for (int index = 0; index < stateCount(); ++index) {
        if (nodes[static_cast<std::size_t>(index)].depth != steps) continue;
        found.push_back(stateAt(index));
    }
    return found;
}

std::vector<BrewPath> BrewGraph::drinkablePaths() const {
    std::vector<BrewPath> paths;
    for (const PotionEntry& row : catalog->entries()) paths.push_back(shortestPath(row.kind));
    return paths;
}

const BrewGraph& brewGraph() {
    static const BrewGraph graph(potionCatalog());
    return graph;
}

} // namespace vf
