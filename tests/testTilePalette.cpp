#include <cstdio>
#include <cstring>
#include <vector>

#include "testHarness.h"
#include "vf/Block.h"
#include "vf/TilePalette.h"

using namespace vf;

namespace {

const Face allFaces[6] = {Face::PosX, Face::NegX, Face::PosY,
                          Face::NegY, Face::PosZ, Face::NegZ};

bool sameLook(const TileSpec& a, const TileSpec& b) {
    return a.base == b.base && a.accent == b.accent && a.pattern == b.pattern;
}

// A palette short of a row is exactly what these tests are here to catch, so
// they must report it rather than walk off the end of the table proving it.
int rowsToWalk(const TilePalette& palette) {
    return palette.size() < tileCount ? palette.size() : tileCount;
}

} // namespace

// The whole point of the palette: 157 tiles, 157 entries. The old generator had
// a switch covering fifteen of them and one grey default for the other one
// hundred and forty two, and nothing could tell you that from the outside.
VF_TEST(everyAtlasTileHasItsOwnPaletteEntry) {
    TilePalette palette;
    CHECK_EQ(palette.size(), tileCount);

    for (int tile = 0; tile < rowsToWalk(palette); ++tile) {
        const TileSpec& spec = palette.specFor(tile);
        bool named = spec.name != nullptr && std::strlen(spec.name) > 0;
        CHECK(named);
        if (!named) std::printf("  unnamed tile: %d\n", tile);
        CHECK(spec.noise >= 0.0f);
        CHECK(spec.noise <= 0.30f);
        CHECK(spec.scale >= 0.0f);
        CHECK(spec.scale <= 1.0f);
        CHECK(spec.density >= 0.0f);
        CHECK(spec.density <= 1.0f);
        CHECK(spec.pattern != TilePattern::Count);
        CHECK(spec.material != TileMaterial::Count);
    }
}

// Two tiles that agree on base colour, accent colour and pattern render the
// same picture, which means one of them is a copy nobody meant to ship.
VF_TEST(noTwoTilesShareTheSameBaseAccentAndPattern) {
    TilePalette palette;
    int duplicates = 0;
    for (int a = 0; a < palette.size(); ++a) {
        for (int b = a + 1; b < palette.size(); ++b) {
            if (!sameLook(palette.specFor(a), palette.specFor(b))) continue;
            ++duplicates;
            if (duplicates <= 8) {
                std::printf("  duplicate tiles: %d (%s) and %d (%s)\n", a, palette.specFor(a).name,
                            b, palette.specFor(b).name);
            }
        }
    }
    CHECK_EQ(duplicates, 0);
}

// The palette is indexed by whatever faceTile hands out, so the two have to
// agree over the entire block list, not over the blocks that existed when the
// palette was written.
VF_TEST(everyBlockFaceLandsOnATileThatHasASpec) {
    TilePalette palette;
    for (int id = 1; id < static_cast<int>(Block::Count); ++id) {
        Block block = static_cast<Block>(id);
        for (Face face : allFaces) {
            int tile = faceTile(block, face);
            CHECK(tile >= 0);
            CHECK(tile < palette.size());
            if (tile < 0 || tile >= palette.size()) {
                std::printf("  block %d has no spec for tile %d\n", id, tile);
                continue;
            }
            CHECK(std::strlen(palette.specFor(tile).name) > 0);
        }
    }
}

// Regression on the look of the world that already exists. These fifteen
// numbers are copied out of the generator that render/Atlas.cpp used to carry;
// if the palette drifts from them, old terrain changes colour.
VF_TEST(theFifteenOriginalTilesKeepTheirExactColours) {
    TilePalette palette;
    const TileRgb historical[15] = {
        {86, 154, 66},    // 0  grass top
        {112, 92, 58},    // 1  grass side, dirt under the green lip
        {119, 88, 58},    // 2  dirt
        {129, 129, 132},  // 3  stone
        {214, 199, 143},  // 4  sand
        {104, 78, 46},    // 5  wood bark
        {160, 128, 78},   // 6  wood rings
        {58, 122, 52},    // 7  leaves
        {54, 96, 168},    // 8  water
        {236, 240, 245},  // 9  snow
        {129, 129, 132},  // 10 coal ore, stone base
        {129, 129, 132},  // 11 iron ore, stone base
        {129, 129, 132},  // 12 gold ore, stone base
        {122, 120, 120},  // 13 gravel
        {242, 220, 132},  // 14 glowstone
    };
    CHECK(palette.size() >= 15);
    for (int tile = 0; tile < rowsToWalk(palette) && tile < 15; ++tile) {
        bool kept = palette.specFor(tile).base == historical[tile];
        CHECK(kept);
        if (!kept) {
            const TileRgb& now = palette.specFor(tile).base;
            std::printf("  tile %d drifted to %u,%u,%u\n", tile, now.r, now.g, now.b);
        }
    }

    // The accents the old code drew on top: the green lip on the grass side and
    // the three ore vein colours.
    TileRgb grassGreen{86, 154, 66};
    TileRgb coal{38, 38, 42};
    TileRgb iron{196, 156, 120};
    TileRgb gold{238, 214, 96};
    CHECK(palette.specFor(1).accent == grassGreen);
    CHECK(palette.specFor(10).accent == coal);
    CHECK(palette.specFor(11).accent == iron);
    CHECK(palette.specFor(12).accent == gold);
}

// The grey the old generator returned for every unhandled tile. Nothing may
// still be sitting on it, and the TileSpec default is that same grey so an
// entry that was declared and never filled in is caught here too.
VF_TEST(noTileIsLeftOnTheOldDefaultGrey) {
    TilePalette palette;
    TileSpec untouched;
    for (int tile = 0; tile < palette.size(); ++tile) {
        const TileSpec& spec = palette.specFor(tile);
        bool blank = spec.base == untouched.base && spec.accent == untouched.accent;
        CHECK(!blank);
        if (blank) std::printf("  tile %d is still default grey\n", tile);
    }
}

// The table is filled by pushing rows in order, so a row inserted or dropped in
// the middle shifts everything after it. Names pinned to the indices Block.h
// documents catch that shift immediately.
VF_TEST(paletteEntriesSitAtTheTileIndexTheyName) {
    TilePalette palette;
    struct Anchor {
        int tile;
        const char* name;
    };
    const Anchor anchors[] = {
        {0, "grassTop"},      {3, "stone"},         {14, "glowstone"},
        {15, "cobblestone"},  {26, "lapisOre"},     {27, "craftingTableTop"},
        {49, "tntFuseBand"},  {50, "ladder"},       {61, "noteBlock"},
        {62, "farmlandTop"},  {79, "packedIce"},    {80, "brewingStand"},
        {88, "cobweb"},       {89, "netherrack"},   {98, "blackstone"},
        {99, "endStone"},     {105, "chorusFlower"},{106, "glass"},
        {134, "hayBaleSide"}, {135, "clay"},        {156, "moss"},
    };
    for (const Anchor& anchor : anchors) {
        CHECK(anchor.tile < palette.size());
        if (anchor.tile >= palette.size()) {
            std::printf("  tile %d is missing entirely\n", anchor.tile);
            continue;
        }
        bool matched = std::strcmp(palette.specFor(anchor.tile).name, anchor.name) == 0;
        CHECK(matched);
        if (!matched) {
            std::printf("  tile %d is %s, expected %s\n", anchor.tile,
                        palette.specFor(anchor.tile).name, anchor.name);
        }
    }
}

// Cutout is what earns a tile a shape in the alpha channel. Glass, both
// portals, the web, the plants, the ladder and the grates need one; a solid
// rock must never get one or the mesher would show daylight through a wall.
VF_TEST(cutoutIsSetExactlyForTheTilesThatCarryAShape) {
    TilePalette palette;
    const int shaped[] = {33, 34, 35, 38, 50, 51, 52, 53, 54, 55, 63, 64,  65,  70, 71,
                          72, 76, 77, 80, 88, 94, 100, 104, 105, 106, 113, 117, 153};
    for (int tile : shaped) {
        CHECK(tile < palette.size());
        if (tile >= palette.size()) continue;
        bool cut = palette.specFor(tile).cutout;
        CHECK(cut);
        if (!cut) std::printf("  tile %d (%s) lost its cutout\n", tile, palette.specFor(tile).name);
    }

    const int solid[] = {0, 2, 3, 4, 7, 8, 13, 15, 16, 20, 89, 107, 108, 137, 156};
    for (int tile : solid) {
        CHECK(tile < palette.size());
        if (tile >= palette.size()) continue;
        bool cut = palette.specFor(tile).cutout;
        CHECK(!cut);
        if (cut) std::printf("  tile %d (%s) became a cutout\n", tile, palette.specFor(tile).name);
    }
}

// A taxonomy nothing uses is decoration. Every material and every pattern the
// enums declare has to be carried by at least one real tile.
VF_TEST(everyMaterialAndEveryPatternIsCarriedByATile) {
    TilePalette palette;
    int materialUses[static_cast<int>(TileMaterial::Count)] = {};
    int patternUses[static_cast<int>(TilePattern::Count)] = {};
    for (const TileSpec& spec : palette.specs()) {
        ++materialUses[static_cast<int>(spec.material)];
        ++patternUses[static_cast<int>(spec.pattern)];
    }
    for (int i = 0; i < static_cast<int>(TileMaterial::Count); ++i) {
        CHECK(materialUses[i] > 0);
        if (materialUses[i] == 0) std::printf("  material %d is unused\n", i);
    }
    for (int i = 0; i < static_cast<int>(TilePattern::Count); ++i) {
        CHECK(patternUses[i] > 0);
        if (patternUses[i] == 0) std::printf("  pattern %d is unused\n", i);
    }
}

// Ore in stone is stone with a vein in it. The host colour has to stay the
// stone colour, and the veins have to be told apart from each other, because
// the base colour alone cannot distinguish coal from gold.
VF_TEST(stoneBorneOresKeepTheHostRockAndDifferByVein) {
    TilePalette palette;
    TileRgb stone = palette.specFor(3).base;
    const int stoneOres[] = {10, 11, 12, 23, 24, 25, 26, 145};
    std::vector<TileRgb> veins;
    for (int tile : stoneOres) {
        CHECK(tile < palette.size());
        if (tile >= palette.size()) continue;
        const TileSpec& spec = palette.specFor(tile);
        CHECK(spec.pattern == TilePattern::OreSpeckle);
        CHECK(spec.base == stone);
        CHECK(spec.accent != stone);
        for (const TileRgb& seen : veins) CHECK(seen != spec.accent);
        veins.push_back(spec.accent);
    }
    CHECK_EQ(veins.size(), sizeof(stoneOres) / sizeof(stoneOres[0]));

    CHECK(palette.size() > 92);
    if (palette.size() <= 92) return;
    // Nether quartz sits in netherrack, not stone, so it must not share the
    // overworld host colour.
    CHECK(palette.specFor(92).pattern == TilePattern::OreSpeckle);
    CHECK(palette.specFor(92).base != stone);
}

// Two palettes built independently must agree row for row: the table is data,
// not something assembled from whatever ran before it.
VF_TEST(twoPalettesBuiltSeparatelyAgreeRowForRow) {
    TilePalette first;
    TilePalette second;
    CHECK_EQ(first.size(), second.size());
    for (int tile = 0; tile < first.size(); ++tile) {
        const TileSpec& a = first.specFor(tile);
        const TileSpec& b = second.specFor(tile);
        CHECK(a.base == b.base);
        CHECK(a.accent == b.accent);
        CHECK(a.pattern == b.pattern);
        CHECK(a.material == b.material);
        CHECK(a.cutout == b.cutout);
        CHECK(a.vertical == b.vertical);
        CHECK(a.flipped == b.flipped);
        CHECK_NEAR(a.noise, b.noise, 0.0f);
        CHECK_NEAR(a.scale, b.scale, 0.0f);
        CHECK_NEAR(a.density, b.density, 0.0f);
    }
}
