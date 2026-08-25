#include <cstring>
#include <vector>

#include "testHarness.h"
#include "vf/PotionCatalog.h"

using namespace vf;

namespace {

using Ingredient = BrewRecipe::Ingredient;

PotionItem bottleOf(PotionKind kind, bool upgraded = false, bool extended = false,
                    PotionItem::Form form = PotionItem::Form::Drinkable) {
    PotionItem potion;
    potion.kind = kind;
    potion.upgraded = upgraded;
    potion.extended = extended;
    potion.form = form;
    return potion;
}

// Runs a whole ingredient list through an actual brewing stand, one full
// four hundred tick brew per step.
PotionItem replayOnAStand(const std::vector<Ingredient>& steps) {
    BrewingStandState stand;
    setBottle(stand, 0, PotionItem{});
    addFuel(stand, blazePowderForBrews(static_cast<int>(steps.size())) + 1);
    for (Ingredient ingredient : steps) {
        setIngredient(stand, ingredient, 1);
        for (int tick = 0; tick < brewDurationTicks; ++tick) tickBrewing(stand);
    }
    return stand.bottles[0].potion;
}

PotionItem replayThroughSteps(const std::vector<Ingredient>& steps) {
    PotionItem bottle;
    for (Ingredient ingredient : steps) {
        bool changed = false;
        bottle = catalogBrewStep(bottle, ingredient, changed);
        if (!changed) return PotionItem{};
    }
    return bottle;
}

int splashDurationThroughTheStand(const PotionItem& potion) {
    PotionItem splash = potion;
    splash.form = PotionItem::Form::Splash;
    EffectSet target;
    if (!splashPotion(target, splash, 0.0f, 4.0f)) return 0;
    return target.active[0].durationTicks;
}

} // namespace

VF_TEST(theCatalogsPublishedConstantsAreTheOnesTheNumbersWereDerivedFrom) {
    CHECK_NEAR(splashDurationScale, 0.75f, 0.0001f);
    CHECK_NEAR(lingeringDoseScale, 0.25f, 0.0001f);
    CHECK_EQ(lingeringCloudTicks, 600);
    CHECK_EQ(lingeringCloudTicks, 30 * ticksPerSecond);

    // Pinned against literals rather than the constants, so shifting a constant
    // cannot quietly shift the expectations with it.
    const PotionCatalog& catalog = potionCatalog();
    PotionItem swiftness = bottleOf(PotionKind::Swiftness);
    CHECK_EQ(catalog.variantTicks(swiftness), 3600);
    swiftness.form = PotionItem::Form::Splash;
    CHECK_EQ(catalog.variantTicks(swiftness), 2700);
    swiftness.form = PotionItem::Form::Lingering;
    CHECK_EQ(catalog.variantTicks(swiftness), 900);
    CHECK_EQ(catalog.cloudTicks(swiftness), 600);
}

VF_TEST(theCatalogHasExactlyOneRowPerPotionKindInEnumOrder) {
    const PotionCatalog& catalog = potionCatalog();
    CHECK_EQ(catalog.size(), static_cast<std::size_t>(PotionKind::Count));

    for (int index = 0; index < static_cast<int>(PotionKind::Count); ++index) {
        PotionKind kind = static_cast<PotionKind>(index);
        const PotionEntry& row = catalog.entry(kind);
        CHECK_EQ(row.kind, kind);
        CHECK(row.name != nullptr);
        CHECK(std::strlen(row.name) > 0);
    }

    int duplicateNames = 0;
    const std::vector<PotionEntry>& rows = catalog.entries();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (std::strcmp(rows[i].name, rows[j].name) == 0) ++duplicateNames;
        }
    }
    CHECK_EQ(duplicateNames, 0);
}

VF_TEST(everyCatalogDurationAndLevelMatchesWhatTheStandActuallyApplies) {
    const PotionCatalog& catalog = potionCatalog();
    for (int index = 0; index < static_cast<int>(PotionKind::Count); ++index) {
        PotionKind kind = static_cast<PotionKind>(index);
        const PotionEntry& row = catalog.entry(kind);

        const struct { bool upgraded; bool extended; } modifiers[] = {
            {false, false}, {true, false}, {false, true}, {true, true}};
        for (const auto& modifier : modifiers) {
            PotionItem potion = bottleOf(kind, modifier.upgraded, modifier.extended);
            EffectInstance applied = potionEffect(potion);

            CHECK_EQ(catalog.variantTicks(potion), applied.durationTicks);
            if (row.carriesEffect()) {
                CHECK_EQ(catalog.variantLevel(potion), applied.level());
                CHECK_EQ(row.effect, applied.kind);
            } else {
                CHECK_EQ(catalog.variantLevel(potion), 0);
                CHECK_EQ(applied.kind, EffectKind::Count);
            }
        }
    }
}

VF_TEST(theSplashColumnOfTheCatalogMatchesWhatASplashBottleLandsAtTheImpactPoint) {
    const PotionCatalog& catalog = potionCatalog();
    int checkedTimedPotions = 0;
    for (const PotionEntry& row : catalog.entries()) {
        if (!row.carriesEffect()) continue;

        const struct { bool upgraded; bool extended; } modifiers[] = {
            {false, false}, {true, false}, {false, true}};
        for (const auto& modifier : modifiers) {
            PotionItem drinkable = bottleOf(row.kind, modifier.upgraded, modifier.extended);
            PotionItem splash = drinkable;
            splash.form = PotionItem::Form::Splash;

            CHECK_EQ(catalog.variantTicks(splash), splashDurationThroughTheStand(drinkable));
            CHECK_EQ(catalog.variantLevel(splash), catalog.variantLevel(drinkable));
            if (!row.instant) ++checkedTimedPotions;
        }
    }
    CHECK(checkedTimedPotions >= 30);
}

VF_TEST(lingeringBottlesDoseAQuarterAndLeaveACloudBehindThem) {
    const PotionCatalog& catalog = potionCatalog();

    PotionItem swiftness = bottleOf(PotionKind::Swiftness);
    CHECK_EQ(catalog.variantTicks(swiftness), 3600);
    CHECK_EQ(catalog.cloudTicks(swiftness), 0);

    PotionItem lingering = bottleOf(PotionKind::Swiftness, false, false,
                                    PotionItem::Form::Lingering);
    CHECK_EQ(catalog.variantTicks(lingering), 900);
    CHECK_EQ(catalog.cloudTicks(lingering), lingeringCloudTicks);

    PotionItem lingeringStrong = bottleOf(PotionKind::Swiftness, true, false,
                                          PotionItem::Form::Lingering);
    CHECK_EQ(catalog.variantTicks(lingeringStrong), 450);
    CHECK_EQ(catalog.variantLevel(lingeringStrong), 2);

    PotionItem lingeringWater = bottleOf(PotionKind::Water, false, false,
                                         PotionItem::Form::Lingering);
    CHECK_EQ(catalog.variantTicks(lingeringWater), 0);
    CHECK_EQ(catalog.cloudTicks(lingeringWater), 0);
}

VF_TEST(instantPotionsStayAtOneTickInEveryFormAndModifier) {
    const PotionCatalog& catalog = potionCatalog();
    const PotionKind instants[] = {PotionKind::Healing, PotionKind::Harming};
    const PotionItem::Form forms[] = {PotionItem::Form::Drinkable, PotionItem::Form::Splash,
                                      PotionItem::Form::Lingering};
    for (PotionKind kind : instants) {
        CHECK(catalog.entry(kind).instant);
        CHECK(!catalog.entry(kind).acceptsRedstone());
        CHECK(catalog.entry(kind).acceptsGlowstone());
        for (PotionItem::Form form : forms) {
            CHECK_EQ(catalog.variantTicks(bottleOf(kind, false, false, form)), 1);
            CHECK_EQ(catalog.variantTicks(bottleOf(kind, true, false, form)), 1);
            CHECK_EQ(catalog.variantLevel(bottleOf(kind, true, false, form)), 2);
        }
    }
}

VF_TEST(everyCatalogRouteIsARealRecipeAndAlsoTheShortestOne) {
    const PotionCatalog& catalog = potionCatalog();
    const BrewGraph& graph = brewGraph();

    int routesChecked = 0;
    for (const PotionEntry& row : catalog.entries()) {
        if (row.ingredient == Ingredient::None) {
            CHECK_EQ(row.kind, PotionKind::Water);
            continue;
        }
        bool changed = false;
        PotionItem brewed = brewResult(bottleOf(row.base), row.ingredient, changed);
        CHECK(changed);
        CHECK_EQ(brewed.kind, row.kind);
        CHECK_EQ(graph.depthOf(bottleOf(row.kind)), graph.depthOf(bottleOf(row.base)) + 1);
        ++routesChecked;
    }
    CHECK_EQ(routesChecked, static_cast<int>(PotionKind::Count) - 1);
}

VF_TEST(harmfulFlagsAndEffectLookupsAgreeWithTheEffectModule) {
    const PotionCatalog& catalog = potionCatalog();
    for (const PotionEntry& row : catalog.entries()) {
        if (!row.carriesEffect()) {
            CHECK(!row.harmful);
            CHECK(!row.instant);
            continue;
        }
        CHECK_EQ(row.harmful, isHarmfulEffect(row.effect));
        CHECK_EQ(row.instant, isInstantEffect(row.effect));
        CHECK_EQ(catalog.kindForEffect(row.effect), row.kind);
    }

    const EffectKind uncovered[] = {EffectKind::Haste,       EffectKind::MiningFatigue,
                                    EffectKind::Nausea,      EffectKind::Resistance,
                                    EffectKind::Wither,      EffectKind::HealthBoost,
                                    EffectKind::Absorption,  EffectKind::Saturation,
                                    EffectKind::Levitation,  EffectKind::Count};
    for (EffectKind effect : uncovered) CHECK_EQ(catalog.kindForEffect(effect), PotionKind::Count);
}

VF_TEST(invertedKindReadsTheFermentedSpiderEyeBranchesStraightOffTheRecipeTable) {
    const PotionCatalog& catalog = potionCatalog();
    CHECK_EQ(catalog.invertedKind(PotionKind::Water), PotionKind::Weakness);
    CHECK_EQ(catalog.invertedKind(PotionKind::NightVision), PotionKind::Invisibility);
    CHECK_EQ(catalog.invertedKind(PotionKind::Healing), PotionKind::Harming);
    CHECK_EQ(catalog.invertedKind(PotionKind::Poison), PotionKind::Harming);
    CHECK_EQ(catalog.invertedKind(PotionKind::Swiftness), PotionKind::Slowness);
    CHECK_EQ(catalog.invertedKind(PotionKind::Leaping), PotionKind::Slowness);
    CHECK_EQ(catalog.invertedKind(PotionKind::Strength), PotionKind::Weakness);

    const PotionKind untouched[] = {PotionKind::Awkward, PotionKind::Regeneration,
                                    PotionKind::FireResistance, PotionKind::WaterBreathing,
                                    PotionKind::Harming, PotionKind::Slowness};
    for (PotionKind kind : untouched) CHECK_EQ(catalog.invertedKind(kind), PotionKind::Count);

    // Every inversion the catalog reports has to survive a real brew.
    for (int index = 0; index < static_cast<int>(PotionKind::Count); ++index) {
        PotionKind kind = static_cast<PotionKind>(index);
        PotionKind inverted = catalog.invertedKind(kind);
        bool changed = false;
        PotionItem brewed = brewResult(bottleOf(kind), Ingredient::FermentedSpiderEye, changed);
        CHECK_EQ(changed, inverted != PotionKind::Count);
        if (changed) CHECK_EQ(brewed.kind, inverted);
    }
}

VF_TEST(everyPotionInTheCatalogHasABrewingPathThatARealStandCanReplay) {
    const PotionCatalog& catalog = potionCatalog();
    const BrewGraph& graph = brewGraph();

    std::vector<BrewPath> paths = graph.drinkablePaths();
    CHECK_EQ(paths.size(), catalog.size());

    for (std::size_t i = 0; i < paths.size(); ++i) {
        const BrewPath& path = paths[i];
        const PotionEntry& row = catalog.entries()[i];
        CHECK(path.reachable);
        CHECK_EQ(path.target.kind, row.kind);
        CHECK_EQ(path.totalTicks, path.stepCount() * brewDurationTicks);
        CHECK_EQ(path.stepCount(), graph.depthOf(path.target));

        PotionItem brewed = replayOnAStand(path.steps);
        CHECK_EQ(brewed.kind, row.kind);
        CHECK_EQ(brewed.form, PotionItem::Form::Drinkable);
        CHECK(!brewed.upgraded);
        CHECK(!brewed.extended);
    }
}

VF_TEST(everyReachableVariantReplaysExactlyThroughItsOwnShortestPath) {
    const BrewGraph& graph = brewGraph();
    int replayed = 0;
    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        PotionItem state = BrewGraph::stateAt(index);
        BrewPath path = graph.shortestPath(state);
        if (!path.reachable) continue;

        CHECK(replayThroughSteps(path.steps) == state);
        CHECK_EQ(path.stepCount(), graph.depthOf(state));
        ++replayed;
    }
    CHECK_EQ(replayed, graph.reachableStateCount());
}

VF_TEST(theGraphReachesExactlyTheVariantsTheCatalogDeclaresAndNothingElse) {
    const PotionCatalog& catalog = potionCatalog();
    const BrewGraph& graph = brewGraph();

    int expected = 0;
    for (const PotionEntry& row : catalog.entries()) expected += row.variantCount() * 3;
    CHECK_EQ(graph.reachableStateCount(), expected);
    CHECK(graph.reachableStateCount() < BrewGraph::stateCount());

    int fromDepths = 0;
    for (int depth = 0; depth <= BrewGraph::stateCount(); ++depth) {
        fromDepths += static_cast<int>(graph.statesAtDepth(depth).size());
    }
    CHECK_EQ(fromDepths, graph.reachableStateCount());
    CHECK_EQ(graph.statesAtDepth(0).size(), std::size_t{1});
    CHECK(graph.statesAtDepth(0)[0] == PotionItem{});
}

VF_TEST(modifiersAPotionRefusesLeaveThatVariantWithNoPathAtAll) {
    const BrewGraph& graph = brewGraph();
    const PotionCatalog& catalog = potionCatalog();

    const PotionKind noGlowstone[] = {PotionKind::Weakness, PotionKind::FireResistance,
                                      PotionKind::WaterBreathing, PotionKind::NightVision,
                                      PotionKind::Invisibility};
    for (PotionKind kind : noGlowstone) {
        CHECK(!catalog.entry(kind).acceptsGlowstone());
        CHECK(!graph.reachable(bottleOf(kind, true, false)));
        CHECK_EQ(graph.depthOf(bottleOf(kind, true, false)), -1);
        CHECK(graph.reachable(bottleOf(kind, false, true)));
    }

