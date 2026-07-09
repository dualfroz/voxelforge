#pragma once

#include <cstdint>
#include <vector>

#include "Brewing.h"

namespace vf {

// A splash bottle lands at three quarters duration on the impact point, which is
// the factor splashPotion applies. A lingering cloud re-doses anyone standing in
// it at a quarter, over and over for lingeringCloudTicks.
constexpr float splashDurationScale = 0.75f;
constexpr float lingeringDoseScale = 0.25f;
constexpr int lingeringCloudTicks = 600;

// One row of the catalog: the recipe that produces the potion, and every number
// its variants resolve to. The stand keeps the same table privately, so the
// catalog is checked against potionEffect rather than trusted on its own.
struct PotionEntry {
    PotionKind kind = PotionKind::Water;
    const char* name = "water";
    // The canonical route to this potion. A potion reachable more than one way
    // names its shortest route here; BrewGraph knows the rest.
    PotionKind base = PotionKind::Water;
    BrewRecipe::Ingredient ingredient = BrewRecipe::Ingredient::None;
    EffectKind effect = EffectKind::Count;
    int level = 1;
    int upgradedLevel = 1;
    int normalTicks = 0;
    // Zero means the modifier is refused outright, not that it lasts no time.
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

// Every potion the stand can hold, as data. Indexed by PotionKind, one row per
// value below PotionKind::Count.
class PotionCatalog {
public:
    PotionCatalog();

    const std::vector<PotionEntry>& entries() const { return rows; }
    const PotionEntry& entry(PotionKind kind) const;
    std::size_t size() const { return rows.size(); }
    const char* name(PotionKind kind) const;

    // PotionKind::Count when no potion carries that effect.
    PotionKind kindForEffect(EffectKind effect) const;
    // What fermented spider eye turns this potion into, PotionKind::Count when
    // it does nothing. Read off the recipe table, not restated.
    PotionKind invertedKind(PotionKind kind) const;

    // Level and effect duration of a fully specified bottle, form included.
    // Zero for the four potions that carry no effect.
    int variantLevel(const PotionItem& potion) const;
    int variantTicks(const PotionItem& potion) const;

    // How long the cloud a lingering bottle leaves behind survives.
    int cloudTicks(const PotionItem& potion) const;

private:
    std::vector<PotionEntry> rows;
};

const PotionCatalog& potionCatalog();

// Blaze powder needed to cover a run of brews, rounded up to whole items.
int blazePowderForBrews(int brewCount);

// One brewing move, extending brewResult with the dragon breath step the
// salvaged stand does not know: a splash bottle plus dragon breath becomes a
// lingering cloud, and a lingering cloud accepts nothing further.
PotionItem catalogBrewStep(const PotionItem& base, BrewRecipe::Ingredient ingredient,
                           bool& changed);

// A shortest route from a water bottle to some other bottle, with the cost of
// running it.
struct BrewPath {
    PotionItem target;
    std::vector<BrewRecipe::Ingredient> steps;
    bool reachable = false;
    // One brew burns one unit of fuel, one blaze powder is worth
    // brewsPerBlazePowder units.
    int blazePowderFuel = 0;
    // Blaze powder is also an ingredient on the strength branch, so a full
    // shopping list has to count both uses.
    int blazePowderIngredient = 0;
    int totalTicks = 0;

    int stepCount() const { return static_cast<int>(steps.size()); }
    int blazePowderTotal() const { return blazePowderFuel + blazePowderIngredient; }
};

// Breadth-first search over every bottle the stand can reach from water. Built
// once, queried many times.
class BrewGraph {
public:
    explicit BrewGraph(const PotionCatalog& catalog);

    BrewPath shortestPath(const PotionItem& target) const;
    // The plain drinkable, unmodified bottle of that kind.
    BrewPath shortestPath(PotionKind kind) const;
    bool reachable(const PotionItem& potion) const;

    // Number of brews between water and the bottle, -1 when unreachable.
    int depthOf(const PotionItem& potion) const;
    int reachableStateCount() const;
    std::vector<PotionItem> statesAtDepth(int steps) const;
    // One path per catalog row, in catalog order.
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

} // namespace vf
