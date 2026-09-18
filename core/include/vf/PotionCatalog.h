#pragma once

#include <cstdint>
#include <vector>

#include "Brewing.h"

namespace vf {

constexpr float splashDurationScale = 0.75f;
constexpr float lingeringDoseScale = 0.25f;
constexpr int lingeringCloudTicks = 600;

struct PotionEntry {
    PotionKind kind = PotionKind::Water;
    const char* name = "water";
    PotionKind base = PotionKind::Water;
    BrewRecipe::Ingredient ingredient = BrewRecipe::Ingredient::None;
    EffectKind effect = EffectKind::Count;
    int level = 1;
    int upgradedLevel = 1;
    int normalTicks = 0;
    int upgradedTicks = 0;
    int extendedTicks = 0;
    bool harmful = false;
    bool instant = false;

    bool carriesEffect() const { return effect != EffectKind::Count; }
    bool acceptsGlowstone() const { return upgradedTicks > 0; }
    bool acceptsRedstone() const { return extendedTicks > 0; }
    int variantCount() const {
        return 1 + (acceptsGlowstone() ? 1 : 0) + (acceptsRedstone() ? 1 : 0);
    }
};

class PotionCatalog {
public:
    PotionCatalog();

    const std::vector<PotionEntry>& entries() const { return rows; }
    const PotionEntry& entry(PotionKind kind) const;
    std::size_t size() const { return rows.size(); }
    const char* name(PotionKind kind) const;

    PotionKind kindForEffect(EffectKind effect) const;
    PotionKind invertedKind(PotionKind kind) const;

    int variantLevel(const PotionItem& potion) const;
    int variantTicks(const PotionItem& potion) const;

    int cloudTicks(const PotionItem& potion) const;

private:
    std::vector<PotionEntry> rows;
};

const PotionCatalog& potionCatalog();

int blazePowderForBrews(int brewCount);

PotionItem catalogBrewStep(const PotionItem& base, BrewRecipe::Ingredient ingredient,
                           bool& changed);

struct BrewPath {
    PotionItem target;
    std::vector<BrewRecipe::Ingredient> steps;
    bool reachable = false;
    int blazePowderFuel = 0;
    int blazePowderIngredient = 0;
    int totalTicks = 0;

    int stepCount() const { return static_cast<int>(steps.size()); }
    int blazePowderTotal() const { return blazePowderFuel + blazePowderIngredient; }
};

class BrewGraph {
public:
    explicit BrewGraph(const PotionCatalog& catalog);

    BrewPath shortestPath(const PotionItem& target) const;
    BrewPath shortestPath(PotionKind kind) const;
    bool reachable(const PotionItem& potion) const;

    int depthOf(const PotionItem& potion) const;
    int reachableStateCount() const;
    std::vector<PotionItem> statesAtDepth(int steps) const;
    std::vector<BrewPath> drinkablePaths() const;

    static int stateIndex(const PotionItem& potion);
    static PotionItem stateAt(int index);
    static int stateCount();

private:
    struct Node {
        int depth = -1;
        int previous = -1;
        BrewRecipe::Ingredient via = BrewRecipe::Ingredient::None;
    };

    const PotionCatalog* catalog;
    std::vector<Node> nodes;
};

const BrewGraph& brewGraph();

}
