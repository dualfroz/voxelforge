#include "vf/TilePalette.h"

namespace vf {

namespace {

using M = TileMaterial;
using P = TilePattern;

TileSpec makeTile(const char* name, M material, P pattern, TileRgb base, TileRgb accent,
                  float noise, float scale, float density) {
    TileSpec spec;
    spec.name = name;
    spec.material = material;
    spec.pattern = pattern;
    spec.base = base;
    spec.accent = accent;
    spec.noise = noise;
    spec.scale = scale;
    spec.density = density;
    return spec;
}

TileSpec runsVertical(TileSpec spec) {
    spec.vertical = true;
    return spec;
}

TileSpec flipped(TileSpec spec) {
    spec.flipped = true;
    return spec;
}

TileSpec cutOut(TileSpec spec) {
    spec.cutout = true;
    return spec;
}

void appendOverworldBasics(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("grassTop", M::Foliage, P::Solid, {86, 154, 66}, {112, 182, 88},
                            0.11f, 0.45f, 0.45f));
    rows.push_back(makeTile("grassSide", M::Soil, P::EdgeBand, {112, 92, 58}, {86, 154, 66},
                            0.11f, 0.29f, 0.50f));
    rows.push_back(makeTile("dirt", M::Soil, P::Solid, {119, 88, 58}, {94, 68, 44},
                            0.11f, 0.55f, 0.55f));
    rows.push_back(makeTile("stone", M::Stone, P::Solid, {129, 129, 132}, {154, 154, 158},
                            0.11f, 0.50f, 0.42f));
    rows.push_back(makeTile("sand", M::Soil, P::Solid, {214, 199, 143}, {236, 224, 178},
                            0.11f, 0.35f, 0.45f));
    rows.push_back(runsVertical(makeTile("woodBark", M::Wood, P::Plank, {104, 78, 46},
                                         {70, 50, 30}, 0.11f, 0.28f, 0.60f)));
    rows.push_back(makeTile("woodRings", M::Wood, P::GrowthRings, {160, 128, 78}, {114, 86, 48},
                            0.11f, 0.30f, 0.85f));
    rows.push_back(makeTile("leaves", M::Foliage, P::Splatter, {58, 122, 52}, {34, 84, 32},
                            0.11f, 0.35f, 0.50f));
    rows.push_back(makeTile("water", M::Fluid, P::Liquid, {54, 96, 168}, {104, 154, 220},
                            0.07f, 0.50f, 0.70f));
    rows.push_back(makeTile("snow", M::Crystal, P::Solid, {236, 240, 245}, {206, 218, 236},
                            0.11f, 0.40f, 0.45f));
    rows.push_back(makeTile("coalOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {38, 38, 42},
                            0.11f, 0.45f, 0.34f));
    rows.push_back(makeTile("ironOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {196, 156, 120},
                            0.11f, 0.38f, 0.30f));
    rows.push_back(makeTile("goldOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {238, 214, 96},
                            0.11f, 0.32f, 0.27f));
    rows.push_back(makeTile("gravel", M::Stone, P::Splatter, {122, 120, 120}, {84, 82, 84},
                            0.11f, 0.25f, 0.55f));
    rows.push_back(makeTile("glowstone", M::Light, P::Dots, {242, 220, 132}, {255, 250, 212},
                            0.14f, 0.40f, 0.50f));
}

void appendBuildingAndOres(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("cobblestone", M::Stone, P::Splatter, {124, 124, 126}, {90, 90, 94},
                            0.12f, 0.45f, 0.52f));
    rows.push_back(makeTile("planks", M::Wood, P::Plank, {166, 132, 82}, {124, 94, 54},
                            0.09f, 0.35f, 0.50f));
    rows.push_back(makeTile("sandstoneTop", M::Stone, P::Solid, {224, 212, 160}, {198, 184, 132},
                            0.08f, 0.55f, 0.45f));
    rows.push_back(makeTile("sandstoneSide", M::Stone, P::Strata, {219, 206, 152}, {188, 174, 122},
                            0.08f, 0.40f, 0.35f));
    rows.push_back(makeTile("ice", M::Crystal, P::Liquid, {150, 190, 236}, {206, 230, 250},
                            0.07f, 0.40f, 0.60f));
    rows.push_back(makeTile("bedrock", M::Stone, P::Splatter, {62, 62, 66}, {22, 22, 26},
                            0.14f, 0.35f, 0.55f));
    rows.push_back(makeTile("obsidian", M::Crystal, P::Solid, {26, 20, 38}, {92, 66, 132},
                            0.12f, 0.50f, 0.60f));
    rows.push_back(makeTile("lava", M::Light, P::Liquid, {214, 84, 18}, {255, 216, 88},
                            0.10f, 0.45f, 0.75f));
    rows.push_back(makeTile("diamondOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {104, 224, 222},
                            0.11f, 0.36f, 0.26f));
    rows.push_back(makeTile("redstoneOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {198, 32, 32},
                            0.11f, 0.34f, 0.30f));
    rows.push_back(makeTile("emeraldOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {56, 204, 104},
                            0.11f, 0.30f, 0.29f));
    rows.push_back(makeTile("lapisOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {42, 72, 182},
                            0.11f, 0.40f, 0.28f));
}

void appendUtilityAndRedstone(std::vector<TileSpec>& rows) {
    rows.push_back(flipped(makeTile("craftingTableTop", M::Wood, P::Lattice, {162, 128, 80},
                                    {96, 70, 42}, 0.08f, 0.10f, 0.55f)));
    rows.push_back(runsVertical(makeTile("craftingTableSide", M::Wood, P::Plank, {150, 116, 72},
                                         {104, 76, 44}, 0.09f, 0.30f, 0.50f)));
    rows.push_back(flipped(makeTile("furnaceFrontCold", M::Stone, P::EdgeBand, {122, 122, 124},
                                    {44, 42, 44}, 0.09f, 0.68f, 0.50f)));
    rows.push_back(makeTile("furnaceSide", M::Stone, P::Solid, {118, 118, 120}, {92, 92, 96},
                            0.09f, 0.45f, 0.50f));
    rows.push_back(flipped(makeTile("furnaceFrontLit", M::Light, P::EdgeBand, {122, 122, 124},
                                    {248, 168, 52}, 0.14f, 0.68f, 0.50f)));
    rows.push_back(makeTile("chest", M::Wood, P::Plank, {150, 110, 58}, {84, 58, 28},
                            0.09f, 0.42f, 0.45f));
    rows.push_back(cutOut(runsVertical(makeTile("torch", M::Light, P::Cross, {132, 96, 52},
                                                {255, 226, 140}, 0.15f, 0.55f, 0.40f))));
    rows.push_back(cutOut(makeTile("redstoneWire", M::Metal, P::Cross, {124, 20, 20},
                                   {216, 48, 48}, 0.10f, 0.60f, 0.50f)));
    rows.push_back(cutOut(runsVertical(makeTile("redstoneTorch", M::Light, P::Cross, {124, 90, 50},
                                                {240, 72, 60}, 0.15f, 0.55f, 0.40f))));
    rows.push_back(makeTile("redstoneLamp", M::Metal, P::Dots, {126, 88, 50}, {84, 58, 32},
                            0.10f, 0.45f, 0.55f));
    rows.push_back(makeTile("redstoneLampLit", M::Light, P::Dots, {248, 214, 138}, {255, 244, 196},
                            0.16f, 0.45f, 0.55f));
    rows.push_back(cutOut(runsVertical(makeTile("lever", M::Wood, P::Cross, {118, 86, 50},
                                                {80, 56, 32}, 0.10f, 0.45f, 0.35f))));
    rows.push_back(makeTile("button", M::Stone, P::GrowthRings, {126, 126, 130}, {92, 92, 96},
                            0.09f, 0.90f, 0.75f));
    rows.push_back(makeTile("redstoneBlock", M::Metal, P::Splatter, {172, 26, 26}, {216, 60, 50},
                            0.11f, 0.40f, 0.55f));
    rows.push_back(makeTile("repeater", M::Stone, P::Dots, {166, 166, 170}, {206, 54, 44},
                            0.08f, 0.55f, 0.22f));
    rows.push_back(makeTile("comparator", M::Stone, P::Dots, {158, 160, 166}, {228, 86, 72},
                            0.08f, 0.50f, 0.30f));
    rows.push_back(makeTile("pistonFacePlate", M::Wood, P::Plank, {188, 158, 102}, {132, 104, 58},
                            0.09f, 0.50f, 0.45f));
    rows.push_back(makeTile("pistonSide", M::Stone, P::Strata, {150, 144, 132}, {108, 102, 92},
                            0.09f, 0.45f, 0.30f));
    rows.push_back(makeTile("pistonBack", M::Stone, P::Solid, {138, 132, 120}, {100, 96, 86},
                            0.09f, 0.40f, 0.50f));
    rows.push_back(makeTile("pistonStickyFace", M::Organic, P::Splatter, {150, 196, 110},
                            {108, 164, 78}, 0.10f, 0.35f, 0.55f));
    rows.push_back(flipped(makeTile("pistonHead", M::Wood, P::Plank, {196, 166, 110},
                                    {138, 108, 60}, 0.09f, 0.55f, 0.45f)));
    rows.push_back(makeTile("tntCaps", M::Fabric, P::GrowthRings, {186, 52, 40}, {120, 28, 22},
                            0.09f, 0.55f, 0.70f));
    rows.push_back(makeTile("tntFuseBand", M::Fabric, P::Strata, {206, 62, 46}, {236, 236, 236},
                            0.09f, 0.20f, 0.30f));
}

void appendTransportAndAutomation(std::vector<TileSpec>& rows) {
    rows.push_back(cutOut(runsVertical(makeTile("ladder", M::Wood, P::Lattice, {142, 108, 62},
                                                {96, 70, 38}, 0.10f, 0.45f, 0.55f))));
    rows.push_back(cutOut(makeTile("rail", M::Metal, P::Lattice, {154, 150, 146}, {120, 88, 50},
                                   0.10f, 0.30f, 0.60f)));
    rows.push_back(cutOut(makeTile("poweredRail", M::Metal, P::Lattice, {198, 158, 68},
                                   {124, 84, 46}, 0.10f, 0.30f, 0.60f)));
    rows.push_back(cutOut(makeTile("detectorRail", M::Metal, P::Lattice, {170, 166, 160},
                                   {188, 64, 52}, 0.10f, 0.30f, 0.60f)));
    rows.push_back(cutOut(makeTile("activatorRail", M::Metal, P::Lattice, {138, 134, 132},
                                   {186, 84, 64}, 0.10f, 0.30f, 0.60f)));
    rows.push_back(cutOut(flipped(makeTile("hopper", M::Metal, P::Lattice, {74, 74, 78},
                                           {40, 40, 44}, 0.11f, 0.30f, 0.75f))));
    rows.push_back(makeTile("dropper", M::Stone, P::GrowthRings, {112, 112, 116}, {58, 58, 62},
                            0.09f, 0.80f, 0.70f));
    rows.push_back(makeTile("dispenser", M::Stone, P::GrowthRings, {106, 106, 110}, {44, 44, 48},
                            0.09f, 0.65f, 0.80f));
    rows.push_back(makeTile("observer", M::Stone, P::GrowthRings, {96, 96, 100}, {200, 58, 48},
                            0.09f, 0.70f, 0.60f));
    rows.push_back(makeTile("targetBlock", M::Fabric, P::GrowthRings, {226, 214, 196},
                            {198, 68, 52}, 0.09f, 0.35f, 0.80f));
    rows.push_back(makeTile("slimeBlock", M::Organic, P::Splatter, {124, 196, 96}, {166, 226, 138},
                            0.10f, 0.55f, 0.50f));
    rows.push_back(makeTile("noteBlock", M::Wood, P::GrowthRings, {126, 96, 58}, {70, 50, 30},
                            0.10f, 0.75f, 0.70f));
}

void appendFarmingAndClimate(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("farmlandTop", M::Soil, P::Plank, {104, 72, 42}, {66, 44, 24},
                            0.11f, 0.30f, 0.45f));
    rows.push_back(cutOut(runsVertical(makeTile("wheat", M::Foliage, P::Cross, {198, 178, 86},
                                                {150, 132, 54}, 0.10f, 0.35f, 0.55f))));
    rows.push_back(cutOut(runsVertical(makeTile("carrot", M::Foliage, P::Cross, {62, 132, 52},
                                                {226, 124, 32}, 0.10f, 0.40f, 0.50f))));
    rows.push_back(cutOut(runsVertical(makeTile("potato", M::Foliage, P::Cross, {70, 136, 58},
                                                {214, 196, 96}, 0.10f, 0.40f, 0.50f))));
    rows.push_back(makeTile("pumpkinTop", M::Organic, P::GrowthRings, {206, 126, 32},
                            {148, 88, 20}, 0.09f, 0.45f, 0.70f));
    rows.push_back(runsVertical(makeTile("pumpkinSide", M::Organic, P::Plank, {212, 132, 34},
                                         {152, 90, 22}, 0.09f, 0.30f, 0.40f)));
    rows.push_back(makeTile("melonTop", M::Organic, P::GrowthRings, {110, 150, 42}, {66, 106, 28},
                            0.09f, 0.40f, 0.70f));
    rows.push_back(runsVertical(makeTile("melonSide", M::Organic, P::Plank, {116, 158, 46},
                                         {58, 100, 26}, 0.09f, 0.26f, 0.45f)));
    rows.push_back(cutOut(runsVertical(makeTile("pumpkinStem", M::Foliage, P::Cross, {126, 166, 58},
                                                {92, 130, 40}, 0.10f, 0.30f, 0.45f))));
    rows.push_back(cutOut(runsVertical(makeTile("melonStem", M::Foliage, P::Cross, {110, 152, 50},
                                                {80, 118, 34}, 0.10f, 0.30f, 0.45f))));
    rows.push_back(cutOut(runsVertical(makeTile("sugarCane", M::Foliage, P::Cross, {150, 196, 120},
                                                {106, 162, 88}, 0.10f, 0.45f, 0.35f))));
    rows.push_back(makeTile("cactusTop", M::Foliage, P::GrowthRings, {80, 138, 58}, {52, 98, 38},
                            0.10f, 0.35f, 0.65f));
    rows.push_back(makeTile("cactusBottom", M::Foliage, P::Solid, {70, 120, 50}, {44, 84, 32},
                            0.10f, 0.45f, 0.55f));
    rows.push_back(runsVertical(makeTile("cactusSide", M::Foliage, P::Plank, {74, 128, 54},
                                         {38, 78, 30}, 0.10f, 0.25f, 0.55f)));
    rows.push_back(cutOut(runsVertical(makeTile("sapling", M::Foliage, P::Cross, {112, 84, 44},
                                                {74, 142, 58}, 0.10f, 0.30f, 0.60f))));
    rows.push_back(cutOut(runsVertical(makeTile("vine", M::Foliage, P::Cross, {56, 116, 44},
                                                {88, 154, 62}, 0.10f, 0.30f, 0.70f))));
    rows.push_back(makeTile("snowLayer", M::Crystal, P::EdgeBand, {238, 242, 248}, {202, 214, 232},
                            0.10f, 0.30f, 0.50f));
    rows.push_back(makeTile("packedIce", M::Crystal, P::Strata, {138, 180, 232}, {176, 210, 246},
                            0.08f, 0.45f, 0.40f));
}

void appendBrewingAndCombat(std::vector<TileSpec>& rows) {
    rows.push_back(cutOut(runsVertical(makeTile("brewingStand", M::Metal, P::Lattice,
                                                {124, 116, 110}, {188, 168, 96},
                                                0.11f, 0.40f, 0.35f))));
    rows.push_back(makeTile("enchantingTableTop", M::Crystal, P::Dots, {58, 42, 74}, {212, 58, 58},
                            0.11f, 0.60f, 0.30f));
    rows.push_back(makeTile("enchantingTableSide", M::Crystal, P::Strata, {48, 36, 64},
                            {128, 56, 152}, 0.11f, 0.35f, 0.30f));
    rows.push_back(makeTile("cauldronTop", M::Fluid, P::Liquid, {62, 62, 66}, {58, 102, 172},
                            0.09f, 0.55f, 0.80f));
    rows.push_back(makeTile("cauldronSide", M::Metal, P::EdgeBand, {72, 72, 76}, {112, 112, 118},
                            0.10f, 0.22f, 0.50f));
    rows.push_back(makeTile("anvilTop", M::Metal, P::Splatter, {84, 84, 88}, {50, 50, 54},
                            0.11f, 0.40f, 0.45f));
    rows.push_back(makeTile("anvilSide", M::Metal, P::EdgeBand, {68, 68, 72}, {104, 104, 110},
                            0.11f, 0.35f, 0.50f));
    rows.push_back(runsVertical(makeTile("bookshelf", M::Fabric, P::Plank, {158, 124, 74},
                                         {176, 72, 64}, 0.10f, 0.16f, 0.55f)));
    rows.push_back(cutOut(makeTile("cobweb", M::Fabric, P::Web, {236, 236, 240}, {180, 180, 190},
                                   0.09f, 0.45f, 0.50f)));
}

void appendNether(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("netherrack", M::Stone, P::Splatter, {124, 44, 44}, {84, 24, 24},
                            0.12f, 0.30f, 0.55f));
    rows.push_back(makeTile("soulSand", M::Soil, P::Splatter, {88, 68, 54}, {52, 38, 30},
                            0.12f, 0.40f, 0.50f));
    rows.push_back(makeTile("soulSoil", M::Soil, P::Splatter, {74, 58, 46}, {44, 32, 26},
                            0.12f, 0.50f, 0.45f));
    rows.push_back(makeTile("netherQuartzOre", M::Stone, P::OreSpeckle, {124, 46, 46},
                            {236, 232, 226}, 0.11f, 0.40f, 0.30f));
    rows.push_back(makeTile("netherBrick", M::Stone, P::Brick, {70, 36, 40}, {38, 18, 22},
                            0.09f, 0.40f, 0.50f));
    rows.push_back(cutOut(makeTile("netherPortal", M::Crystal, P::Liquid, {110, 42, 182},
                                   {196, 128, 240}, 0.12f, 0.55f, 0.75f)));
    rows.push_back(makeTile("magma", M::Light, P::Splatter, {148, 52, 24}, {248, 170, 52},
                            0.13f, 0.35f, 0.45f));
    rows.push_back(makeTile("basaltTop", M::Stone, P::Splatter, {82, 80, 84}, {50, 48, 52},
                            0.11f, 0.30f, 0.50f));
    rows.push_back(runsVertical(makeTile("basaltSide", M::Stone, P::Strata, {76, 74, 80},
                                         {44, 42, 48}, 0.11f, 0.55f, 0.40f)));
    rows.push_back(makeTile("blackstone", M::Stone, P::Splatter, {52, 48, 56}, {24, 22, 28},
                            0.14f, 0.45f, 0.55f));
}

void appendEnd(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("endStone", M::Stone, P::Solid, {222, 222, 164}, {192, 190, 126},
                            0.09f, 0.55f, 0.50f));
    rows.push_back(cutOut(makeTile("endPortal", M::Light, P::Liquid, {16, 16, 32}, {132, 220, 204},
                                   0.14f, 0.60f, 0.70f)));
    rows.push_back(makeTile("endPortalFrameTop", M::Crystal, P::GrowthRings, {198, 198, 148},
                            {56, 92, 84}, 0.09f, 0.40f, 0.75f));
    rows.push_back(makeTile("endPortalFrameSide", M::Stone, P::Strata, {186, 188, 140},
                            {146, 148, 102}, 0.09f, 0.30f, 0.35f));
    rows.push_back(makeTile("purpur", M::Stone, P::Dots, {172, 124, 172}, {206, 162, 206},
                            0.10f, 0.30f, 0.60f));
    rows.push_back(cutOut(runsVertical(makeTile("chorusPlant", M::Organic, P::Cross, {124, 88, 124},
                                                {84, 54, 84}, 0.11f, 0.50f, 0.55f))));
    rows.push_back(cutOut(runsVertical(makeTile("chorusFlower", M::Organic, P::Cross,
                                                {198, 180, 198}, {142, 120, 142},
                                                0.11f, 0.45f, 0.65f))));
}

void appendVillage(std::vector<TileSpec>& rows) {
    rows.push_back(cutOut(makeTile("glass", M::Crystal, P::Pane, {214, 236, 244}, {160, 200, 218},
                                   0.06f, 0.30f, 0.40f)));
    rows.push_back(makeTile("bricks", M::Stone, P::Brick, {150, 86, 66}, {192, 172, 158},
                            0.08f, 0.40f, 0.50f));
    rows.push_back(makeTile("stoneBricks", M::Stone, P::Brick, {128, 128, 132}, {84, 84, 88},
                            0.08f, 0.45f, 0.50f));
    rows.push_back(makeTile("mossyCobblestone", M::Stone, P::Splatter, {112, 124, 104},
                            {64, 92, 56}, 0.11f, 0.40f, 0.50f));
    rows.push_back(runsVertical(makeTile("door", M::Wood, P::Plank, {146, 112, 66}, {94, 68, 38},
                                         0.09f, 0.45f, 0.45f)));
    rows.push_back(makeTile("slabCutFace", M::Stone, P::Solid, {158, 158, 162}, {128, 128, 132},
                            0.08f, 0.60f, 0.50f));
    rows.push_back(makeTile("slabProfile", M::Stone, P::Strata, {148, 148, 152}, {116, 116, 120},
                            0.08f, 0.25f, 0.45f));
    rows.push_back(cutOut(runsVertical(makeTile("fence", M::Wood, P::Lattice, {154, 118, 70},
                                                {106, 78, 44}, 0.10f, 0.50f, 0.30f))));
    rows.push_back(makeTile("bedQuilt", M::Fabric, P::Solid, {182, 42, 48}, {224, 96, 100},
                            0.09f, 0.40f, 0.55f));
    rows.push_back(makeTile("bedFrame", M::Wood, P::EdgeBand, {132, 100, 58}, {182, 42, 48},
                            0.09f, 0.25f, 0.50f));
    rows.push_back(makeTile("bell", M::Metal, P::GrowthRings, {214, 176, 66}, {150, 114, 32},
                            0.10f, 0.55f, 0.70f));
    rows.push_back(cutOut(flipped(makeTile("lantern", M::Light, P::Lattice, {248, 222, 140},
                                           {96, 88, 76}, 0.14f, 0.40f, 0.55f))));
    rows.push_back(makeTile("composterOpen", M::Organic, P::Splatter, {104, 84, 44},
                            {144, 130, 62}, 0.12f, 0.35f, 0.50f));
    rows.push_back(runsVertical(makeTile("composterSlats", M::Wood, P::Plank, {124, 96, 54},
                                         {84, 62, 34}, 0.10f, 0.22f, 0.50f)));
    rows.push_back(makeTile("barrelLid", M::Wood, P::GrowthRings, {148, 116, 70}, {92, 70, 40},
                            0.10f, 0.28f, 0.75f));
    rows.push_back(runsVertical(makeTile("barrelStaves", M::Wood, P::Plank, {138, 106, 62},
                                         {88, 66, 38}, 0.10f, 0.26f, 0.55f)));
    rows.push_back(makeTile("smithingTableTop", M::Metal, P::Splatter, {58, 58, 66}, {102, 102, 112},
                            0.11f, 0.35f, 0.45f));
    rows.push_back(runsVertical(makeTile("smithingTableSide", M::Wood, P::Plank, {74, 64, 60},
                                         {40, 34, 32}, 0.11f, 0.30f, 0.50f)));
    rows.push_back(flipped(makeTile("loomTop", M::Fabric, P::Lattice, {206, 192, 158},
                                    {146, 128, 94}, 0.09f, 0.10f, 0.50f)));
    rows.push_back(runsVertical(makeTile("loomSide", M::Fabric, P::Plank, {186, 170, 134},
                                         {128, 110, 80}, 0.09f, 0.30f, 0.50f)));
    rows.push_back(makeTile("cartographyTableTop", M::Fabric, P::Solid, {220, 208, 180},
                            {148, 116, 80}, 0.09f, 0.45f, 0.55f));
    rows.push_back(runsVertical(makeTile("cartographyTableSide", M::Wood, P::Plank, {118, 92, 58},
                                         {198, 186, 158}, 0.09f, 0.35f, 0.40f)));
    rows.push_back(makeTile("grindstone", M::Stone, P::GrowthRings, {150, 146, 140}, {100, 76, 44},
                            0.10f, 0.32f, 0.65f));
    rows.push_back(makeTile("stonecutterTop", M::Stone, P::Solid, {142, 142, 146}, {108, 108, 112},
                            0.09f, 0.40f, 0.50f));
    rows.push_back(makeTile("stonecutterSide", M::Stone, P::Strata, {132, 132, 136},
                            {184, 184, 188}, 0.09f, 0.55f, 0.25f));
    rows.push_back(makeTile("lecternTop", M::Fabric, P::Solid, {206, 196, 170}, {128, 98, 58},
                            0.09f, 0.35f, 0.60f));
    rows.push_back(runsVertical(makeTile("lecternSide", M::Wood, P::Plank, {160, 126, 78},
                                         {106, 80, 46}, 0.09f, 0.32f, 0.50f)));
    rows.push_back(makeTile("hayBaleCut", M::Organic, P::GrowthRings, {206, 178, 54},
                            {156, 132, 34}, 0.10f, 0.22f, 0.80f));
    rows.push_back(makeTile("hayBaleSide", M::Organic, P::Plank, {198, 170, 52}, {136, 114, 28},
                            0.10f, 0.20f, 0.60f));
}

void appendGeologyAndDecoration(std::vector<TileSpec>& rows) {
    rows.push_back(makeTile("clay", M::Soil, P::Solid, {162, 166, 178}, {132, 136, 150},
                            0.09f, 0.60f, 0.50f));
    rows.push_back(makeTile("terracotta", M::Soil, P::Strata, {150, 92, 66}, {118, 68, 46},
                            0.10f, 0.60f, 0.40f));
    rows.push_back(makeTile("wool", M::Fabric, P::Solid, {228, 228, 232}, {198, 198, 206},
                            0.10f, 0.30f, 0.50f));
    rows.push_back(makeTile("myceliumTop", M::Organic, P::Splatter, {110, 94, 110}, {148, 128, 152},
                            0.11f, 0.30f, 0.50f));
    rows.push_back(makeTile("myceliumSide", M::Soil, P::EdgeBand, {114, 88, 60}, {124, 102, 128},
                            0.11f, 0.29f, 0.50f));
    rows.push_back(makeTile("andesite", M::Stone, P::Splatter, {136, 136, 138}, {104, 104, 108},
                            0.10f, 0.55f, 0.50f));
    rows.push_back(makeTile("diorite", M::Stone, P::Splatter, {196, 196, 198}, {154, 154, 160},
                            0.10f, 0.35f, 0.50f));
    rows.push_back(makeTile("granite", M::Stone, P::Splatter, {154, 106, 88}, {190, 138, 116},
                            0.10f, 0.30f, 0.50f));
    rows.push_back(makeTile("deepslateCut", M::Stone, P::Solid, {80, 80, 86}, {52, 52, 58},
                            0.12f, 0.45f, 0.60f));
    rows.push_back(runsVertical(makeTile("deepslateSide", M::Stone, P::Strata, {76, 76, 82},
                                         {46, 46, 52}, 0.12f, 0.65f, 0.45f)));
    rows.push_back(makeTile("copperOre", M::Stone, P::OreSpeckle, {129, 129, 132}, {216, 132, 88},
                            0.11f, 0.42f, 0.32f));
    rows.push_back(makeTile("tuff", M::Stone, P::Splatter, {108, 110, 102}, {76, 78, 70},
                            0.11f, 0.25f, 0.55f));
    rows.push_back(runsVertical(makeTile("dripstone", M::Stone, P::Strata, {142, 116, 100},
                                         {108, 84, 72}, 0.11f, 0.40f, 0.50f)));
    rows.push_back(makeTile("amethystBlock", M::Crystal, P::Dots, {154, 106, 204}, {214, 176, 246},
                            0.11f, 0.55f, 0.65f));
    rows.push_back(makeTile("sculk", M::Organic, P::Dots, {30, 36, 44}, {58, 180, 168},
                            0.13f, 0.35f, 0.45f));
    rows.push_back(makeTile("cryingObsidian", M::Crystal, P::Dots, {36, 22, 58}, {104, 56, 210},
                            0.12f, 0.40f, 0.35f));
    rows.push_back(makeTile("prismarine", M::Crystal, P::Brick, {96, 158, 148}, {140, 198, 184},
                            0.09f, 0.30f, 0.50f));
    rows.push_back(makeTile("seaLantern", M::Light, P::Dots, {188, 222, 214}, {240, 252, 248},
                            0.14f, 0.50f, 0.60f));
    rows.push_back(cutOut(runsVertical(makeTile("kelp", M::Foliage, P::Cross, {56, 110, 52},
                                                {98, 152, 72}, 0.10f, 0.35f, 0.65f))));
    rows.push_back(makeTile("coral", M::Organic, P::Splatter, {214, 72, 132}, {250, 134, 182},
                            0.11f, 0.30f, 0.50f));
    rows.push_back(makeTile("mud", M::Soil, P::Solid, {74, 62, 56}, {48, 40, 36},
                            0.13f, 0.40f, 0.65f));
    rows.push_back(makeTile("moss", M::Foliage, P::Splatter, {90, 124, 54}, {60, 92, 34},
                            0.11f, 0.35f, 0.55f));
}

}

TilePalette::TilePalette() {
    rows.reserve(static_cast<std::size_t>(tileCount));
    appendOverworldBasics(rows);
    appendBuildingAndOres(rows);
    appendUtilityAndRedstone(rows);
    appendTransportAndAutomation(rows);
    appendFarmingAndClimate(rows);
    appendBrewingAndCombat(rows);
    appendNether(rows);
    appendEnd(rows);
    appendVillage(rows);
    appendGeologyAndDecoration(rows);
}

const TileSpec& TilePalette::specFor(int tile) const {
    return rows[static_cast<std::size_t>(tile)];
}

}
