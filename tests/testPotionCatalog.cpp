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

    const PotionKind noRedstone[] = {PotionKind::Healing, PotionKind::Harming};
    for (PotionKind kind : noRedstone) {
        CHECK(!graph.reachable(bottleOf(kind, false, true)));
        CHECK(graph.reachable(bottleOf(kind, true, false)));
    }

    // The four effectless potions take no modifier at all: glowstone and
    // redstone on water are recipes, not upgrades.
    const PotionKind bases[] = {PotionKind::Water, PotionKind::Mundane, PotionKind::Thick,
                                PotionKind::Awkward};
    for (PotionKind kind : bases) {
        CHECK(!graph.reachable(bottleOf(kind, true, false)));
        CHECK(!graph.reachable(bottleOf(kind, false, true)));
        CHECK(graph.reachable(bottleOf(kind)));
    }

    BrewPath missing = graph.shortestPath(bottleOf(PotionKind::Harming, false, true));
    CHECK(!missing.reachable);
    CHECK_EQ(missing.stepCount(), 0);
    CHECK_EQ(missing.blazePowderTotal(), 0);
    CHECK_EQ(missing.totalTicks, 0);
}

VF_TEST(gunpowderAndDragonBreathAddExactlyOneStepEachOnTopOfTheDrinkable) {
    const BrewGraph& graph = brewGraph();

    PotionItem drinkable = bottleOf(PotionKind::Strength, true);
    PotionItem splash = drinkable;
    splash.form = PotionItem::Form::Splash;
    PotionItem lingering = drinkable;
    lingering.form = PotionItem::Form::Lingering;

    int drinkableDepth = graph.depthOf(drinkable);
    CHECK_EQ(drinkableDepth, 3);
    CHECK_EQ(graph.depthOf(splash), drinkableDepth + 1);
    CHECK_EQ(graph.depthOf(lingering), drinkableDepth + 2);

    BrewPath path = graph.shortestPath(lingering);
    CHECK(path.reachable);
    // Dragon breath has to close the chain, but gunpowder is free to land
    // anywhere before it: splashing the awkward base is just as short.
    CHECK_EQ(path.steps.back(), Ingredient::DragonBreath);
    int gunpowderSteps = 0;
    int dragonBreathSteps = 0;
    for (Ingredient ingredient : path.steps) {
        if (ingredient == Ingredient::Gunpowder) ++gunpowderSteps;
        if (ingredient == Ingredient::DragonBreath) ++dragonBreathSteps;
    }
    CHECK_EQ(gunpowderSteps, 1);
    CHECK_EQ(dragonBreathSteps, 1);
    CHECK(replayThroughSteps(path.steps) == lingering);
}

VF_TEST(aLingeringBottleTakesNoFurtherIngredientEvenThoughTheBareStandWouldTakeOne) {
    PotionItem lingering = bottleOf(PotionKind::Swiftness, false, false,
                                    PotionItem::Form::Lingering);
    for (int step = 1; step < static_cast<int>(Ingredient::Count); ++step) {
        bool changed = false;
        PotionItem after = catalogBrewStep(lingering, static_cast<Ingredient>(step), changed);
        CHECK(!changed);
        CHECK(after == lingering);
    }

    // Without the catalog guard the salvaged stand would happily push a
    // lingering cloud back down to a splash bottle.
    bool standChanged = false;
    PotionItem downgraded = brewResult(lingering, Ingredient::Gunpowder, standChanged);
    CHECK(standChanged);
    CHECK_EQ(downgraded.form, PotionItem::Form::Splash);
}

VF_TEST(catalogBrewStepMatchesTheStandForEveryIngredientItShares) {
    int compared = 0;
    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        PotionItem state = BrewGraph::stateAt(index);
        if (state.form == PotionItem::Form::Lingering) continue;

        for (int step = 1; step < static_cast<int>(Ingredient::Count); ++step) {
            Ingredient ingredient = static_cast<Ingredient>(step);
            if (ingredient == Ingredient::DragonBreath) continue;

            bool catalogChanged = false;
            bool standChanged = false;
            PotionItem viaCatalog = catalogBrewStep(state, ingredient, catalogChanged);
            PotionItem viaStand = brewResult(state, ingredient, standChanged);
            CHECK_EQ(catalogChanged, standChanged);
            CHECK(viaCatalog == viaStand);
            ++compared;
        }
    }
    CHECK(compared > 1000);
}

VF_TEST(dragonBreathOnlyBitesOnASplashBottle) {
    const PotionItem::Form forms[] = {PotionItem::Form::Drinkable, PotionItem::Form::Splash};
    for (PotionItem::Form form : forms) {
        PotionItem bottle = bottleOf(PotionKind::Poison, false, false, form);
        bool changed = false;
        PotionItem after = catalogBrewStep(bottle, Ingredient::DragonBreath, changed);
        CHECK_EQ(changed, form == PotionItem::Form::Splash);
        if (changed) {
            CHECK_EQ(after.form, PotionItem::Form::Lingering);
            CHECK_EQ(after.kind, PotionKind::Poison);
        } else {
            CHECK(after == bottle);
        }
    }
}

VF_TEST(theFuelBalanceRoundsUpToWholeBlazePowderAndCountsIngredientUseSeparately) {
    CHECK_EQ(blazePowderForBrews(0), 0);
    CHECK_EQ(blazePowderForBrews(-5), 0);
    CHECK_EQ(blazePowderForBrews(1), 1);
    CHECK_EQ(blazePowderForBrews(brewsPerBlazePowder), 1);
    CHECK_EQ(blazePowderForBrews(brewsPerBlazePowder + 1), 2);
    CHECK_EQ(blazePowderForBrews(3 * brewsPerBlazePowder), 3);

    const BrewGraph& graph = brewGraph();
    BrewPath strength = graph.shortestPath(PotionKind::Strength);
    CHECK_EQ(strength.stepCount(), 2);
    CHECK_EQ(strength.blazePowderIngredient, 1);
    CHECK_EQ(strength.blazePowderFuel, 1);
    CHECK_EQ(strength.blazePowderTotal(), 2);
    CHECK_EQ(strength.totalTicks, 2 * brewDurationTicks);

    BrewPath swiftness = graph.shortestPath(PotionKind::Swiftness);
    CHECK_EQ(swiftness.blazePowderIngredient, 0);
    CHECK_EQ(swiftness.blazePowderTotal(), 1);

    BrewPath water = graph.shortestPath(PotionKind::Water);
    CHECK(water.reachable);
    CHECK_EQ(water.stepCount(), 0);
    CHECK_EQ(water.blazePowderFuel, 0);
    CHECK_EQ(water.blazePowderTotal(), 0);
}

VF_TEST(everyPathTheGraphReportsIsGenuinelyShortestForItsTarget) {
    const BrewGraph& graph = brewGraph();
    int relaxationFailures = 0;
    std::vector<bool> produced(static_cast<std::size_t>(BrewGraph::stateCount()), false);

    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        PotionItem state = BrewGraph::stateAt(index);
        int depth = graph.depthOf(state);
        if (depth < 0) continue;

        for (int step = 1; step < static_cast<int>(Ingredient::Count); ++step) {
            bool changed = false;
            PotionItem next = catalogBrewStep(state, static_cast<Ingredient>(step), changed);
            if (!changed) continue;
            produced[static_cast<std::size_t>(BrewGraph::stateIndex(next))] = true;
            int nextDepth = graph.depthOf(next);
            if (nextDepth < 0 || nextDepth > depth + 1) ++relaxationFailures;
        }
    }
    CHECK_EQ(relaxationFailures, 0);

    // Water is the only bottle no brew produces; everything else the graph
    // calls reachable has to come out of some edge, and nothing outside the
    // reachable set may be produced at all.
    int mismatches = 0;
    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        bool isReachable = graph.depthOf(BrewGraph::stateAt(index)) >= 0;
        bool expectProduced = isReachable && index != BrewGraph::stateIndex(PotionItem{});
        if (produced[static_cast<std::size_t>(index)] != expectProduced) ++mismatches;
    }
    CHECK_EQ(mismatches, 0);
}

VF_TEST(stateIndexAndStateAtAreExactInversesOverTheWholeSpace) {
    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        PotionItem state = BrewGraph::stateAt(index);
        CHECK_EQ(BrewGraph::stateIndex(state), index);
        CHECK(!(state.upgraded && state.extended));
    }
    CHECK_EQ(BrewGraph::stateCount(), static_cast<int>(PotionKind::Count) * 9);
    CHECK_EQ(BrewGraph::stateIndex(PotionItem{}), 0);
}

VF_TEST(theSharedCatalogAndGraphInstancesAreTheSameObjectEveryTime) {
    CHECK(&potionCatalog() == &potionCatalog());
    CHECK(&brewGraph() == &brewGraph());

    BrewGraph rebuilt(potionCatalog());
    CHECK_EQ(rebuilt.reachableStateCount(), brewGraph().reachableStateCount());
    for (int index = 0; index < BrewGraph::stateCount(); ++index) {
        CHECK_EQ(rebuilt.depthOf(BrewGraph::stateAt(index)),
                 brewGraph().depthOf(BrewGraph::stateAt(index)));
    }
}
