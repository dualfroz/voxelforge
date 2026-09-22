#include <cstdio>
#include <set>
#include <vector>

#include "testHarness.h"
#include "vf/Block.h"
#include "vf/TileTexture.h"

using namespace vf;

namespace {

int luminance(TilePixel texel) {
    return (texel.r * 299 + texel.g * 587 + texel.b * 114) / 1000;
}

int colourDistance(TilePixel texel, TileRgb reference) {
    int dr = static_cast<int>(texel.r) - reference.r;
    int dg = static_cast<int>(texel.g) - reference.g;
    int db = static_cast<int>(texel.b) - reference.b;
    return dr * dr + dg * dg + db * db;
}

bool leansToAccent(TilePixel texel, const TileSpec& spec) {
    return colourDistance(texel, spec.accent) < colourDistance(texel, spec.base);
}

int renderedTiles(const TileSampler& sampler) {
    int held = sampler.palette().size();
    return held < tileCount ? held : tileCount;
}

std::vector<uint8_t> renderTile(const TileSampler& sampler, int tile) {
    int size = sampler.params().tileSize;
    std::vector<uint8_t> bytes(static_cast<std::size_t>(size) * size * 4);
    sampler.writeTile(tile, bytes.data());
    return bytes;
}

std::vector<uint8_t> renderAtlas(const TileSampler& sampler, bool backwards) {
    int size = sampler.params().tileSize;
    std::size_t tileBytes = static_cast<std::size_t>(size) * size * 4;
    int held = renderedTiles(sampler);
    std::vector<uint8_t> bytes(tileBytes * static_cast<std::size_t>(held));
    if (backwards) {
        for (int tile = held - 1; tile >= 0; --tile) {
            sampler.writeTile(tile, bytes.data() + tileBytes * static_cast<std::size_t>(tile));
        }
    } else {
        for (int tile = 0; tile < held; ++tile) {
            sampler.writeTile(tile, bytes.data() + tileBytes * static_cast<std::size_t>(tile));
        }
    }
    return bytes;
}

struct LuminanceRange {
    int lowest = 255;
    int highest = 0;
    int spread() const { return highest - lowest; }
};

LuminanceRange interiorRange(const TileSampler& sampler, int tile) {
    LuminanceRange range;
    int size = sampler.params().tileSize;
    for (int y = 1; y < size; ++y) {
        for (int x = 1; x < size; ++x) {
            int value = luminance(sampler.pixelAt(tile, x, y));
            if (value < range.lowest) range.lowest = value;
            if (value > range.highest) range.highest = value;
        }
    }
    return range;
}

TileSpec withoutGrain(TileSpec spec) {
    spec.noise = 0.0f;
    return spec;
}

std::vector<int> accentColumns(const TileSampler& sampler, const TileSpec& spec, int tileSalt,
                               int y) {
    std::vector<int> columns;
    for (int x = 0; x < sampler.params().tileSize; ++x) {
        if (leansToAccent(sampler.pixelOf(spec, tileSalt, x, y), spec)) columns.push_back(x);
    }
    return columns;
}

int distinctJointLayouts(const TileSampler& sampler, const TileSpec& spec, int tileSalt) {
    int size = sampler.params().tileSize;
    std::set<std::vector<int>> layouts;
    for (int y = 0; y < size; ++y) {
        std::vector<int> columns = accentColumns(sampler, spec, tileSalt, y);
        if (columns.empty()) continue;
        if (static_cast<int>(columns.size()) > size / 2) continue;
        layouts.insert(columns);
    }
    return static_cast<int>(layouts.size());
}

TileSpec syntheticBrick() {
    TileSpec spec;
    spec.name = "syntheticBrick";
    spec.pattern = TilePattern::Brick;
    spec.material = TileMaterial::Stone;
    spec.base = TileRgb{30, 30, 30};
    spec.accent = TileRgb{240, 240, 240};
    spec.noise = 0.0f;
    spec.scale = 0.40f;
    spec.density = 0.50f;
    return spec;
}

}

VF_TEST(renderingATileTwiceProducesIdenticalBytes) {
    TileSampler first;
    TileSampler second;
    CHECK_EQ(first.palette().size(), tileCount);
    for (int tile = 0; tile < renderedTiles(first); ++tile) {
        std::vector<uint8_t> a = renderTile(first, tile);
        std::vector<uint8_t> b = renderTile(second, tile);
        bool identical = a == b;
        CHECK(identical);
        if (!identical) std::printf("  tile %d is not reproducible\n", tile);
    }
}

VF_TEST(theAtlasDoesNotDependOnTheOrderTilesAreGenerated) {
    TileSampler sampler;
    std::vector<uint8_t> forwards = renderAtlas(sampler, false);
    std::vector<uint8_t> backwards = renderAtlas(sampler, true);
    CHECK_EQ(forwards.size(), backwards.size());
    CHECK(forwards == backwards);
}

VF_TEST(opaqueTilesAreFullyOpaqueInEveryPixel) {
    TileSampler sampler;
    int size = sampler.params().tileSize;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        if (sampler.palette().specFor(tile).cutout) continue;
        int transparent = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (sampler.pixelAt(tile, x, y).a != 255) ++transparent;
            }
        }
        CHECK_EQ(transparent, 0);
        if (transparent != 0) {
            std::printf("  tile %d (%s) has %d see-through pixels\n", tile,
                        sampler.palette().specFor(tile).name, transparent);
        }
    }
}

VF_TEST(cutoutTilesCarryBothHolesAndSolidPixels) {
    TileSampler sampler;
    int size = sampler.params().tileSize;
    int cutouts = 0;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        const TileSpec& spec = sampler.palette().specFor(tile);
        if (!spec.cutout) continue;
        ++cutouts;
        int holes = 0;
        int solid = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                uint8_t alpha = sampler.pixelAt(tile, x, y).a;
                if (alpha == 0) ++holes;
                if (alpha == 255) ++solid;
            }
        }
        CHECK(holes > 0);
        CHECK(solid > 0);
        CHECK_EQ(holes + solid, size * size);
        if (holes == 0 || solid == 0) {
            std::printf("  tile %d (%s): %d holes, %d solid\n", tile, spec.name, holes, solid);
        }
    }
    CHECK(cutouts >= 20);
}

VF_TEST(everyTileVariesAcrossItsInterior) {
    TileSampler sampler;
    const int minimumSpread = 12;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        LuminanceRange range = interiorRange(sampler, tile);
        CHECK(range.spread() >= minimumSpread);
        if (range.spread() < minimumSpread) {
            std::printf("  tile %d (%s) is flat: spread %d\n", tile,
                        sampler.palette().specFor(tile).name, range.spread());
        }
    }
}

VF_TEST(brickCoursesShiftTheirJointsBetweenRows) {
    TileSampler sampler;
    TileSpec synthetic = syntheticBrick();
    int layouts = distinctJointLayouts(sampler, synthetic, 500);
    CHECK(layouts >= 2);
    if (layouts < 2) std::printf("  synthetic brick has %d joint layouts\n", layouts);

    std::vector<int> firstCourse = accentColumns(sampler, synthetic, 500, 2);
    std::vector<int> secondCourse = accentColumns(sampler, synthetic, 500, 6);
    CHECK(!firstCourse.empty());
    CHECK(!secondCourse.empty());
    CHECK(firstCourse != secondCourse);

    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        const TileSpec& spec = sampler.palette().specFor(tile);
        if (spec.pattern != TilePattern::Brick) continue;
        int real = distinctJointLayouts(sampler, withoutGrain(spec), tile);
        CHECK(real >= 2);
        if (real < 2) std::printf("  tile %d (%s) has %d joint layouts\n", tile, spec.name, real);
    }
}

VF_TEST(raisingContrastWidensTheLuminanceRange) {
    TileTextureParams plain;
    TileTextureParams punchy;
    punchy.contrast = 2.0f;
    TileSampler flat(plain);
    TileSampler bold(punchy);

    int widened = 0;
    for (int tile = 0; tile < renderedTiles(flat); ++tile) {
        int quiet = interiorRange(flat, tile).spread();
        int loud = interiorRange(bold, tile).spread();
        CHECK(loud >= quiet);
        if (loud > quiet) ++widened;
    }
    CHECK(widened > tileCount / 2);
}

VF_TEST(changingTheNoiseSeedRedrawsTheGrainButNotThePalette) {
    TileTextureParams first;
    TileTextureParams second;
    second.noiseSeed = first.noiseSeed ^ 0x5bf03635u;
    TileSampler original(first);
    TileSampler reseeded(second);

    int changed = 0;
    for (int tile = 0; tile < renderedTiles(original); ++tile) {
        if (renderTile(original, tile) != renderTile(reseeded, tile)) ++changed;
        CHECK(original.palette().specFor(tile).base == reseeded.palette().specFor(tile).base);
        CHECK(original.palette().specFor(tile).accent == reseeded.palette().specFor(tile).accent);
    }
    CHECK(changed > tileCount * 3 / 4);

    TileSampler sameSeedAgain(first);
    CHECK(renderAtlas(original, false) == renderAtlas(sameSeedAgain, false));
}

VF_TEST(writeTileAgreesWithPixelAtEverywhere) {
    TileSampler sampler;
    int size = sampler.params().tileSize;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        std::vector<uint8_t> bytes = renderTile(sampler, tile);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                TilePixel texel = sampler.pixelAt(tile, x, y);
                std::size_t offset = (static_cast<std::size_t>(y) * size + x) * 4;
                CHECK_EQ(bytes[offset + 0], texel.r);
                CHECK_EQ(bytes[offset + 1], texel.g);
                CHECK_EQ(bytes[offset + 2], texel.b);
                CHECK_EQ(bytes[offset + 3], texel.a);
            }
        }
    }
}

VF_TEST(tilesKeepTheirAlphaAndContrastAtOtherTileSizes) {
    const int sizes[] = {8, 16, 32};
    for (int size : sizes) {
        TileTextureParams params;
        params.tileSize = size;
        TileSampler sampler(params);
        for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
            const TileSpec& spec = sampler.palette().specFor(tile);
            int holes = 0;
            int solid = 0;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    uint8_t alpha = sampler.pixelAt(tile, x, y).a;
                    if (alpha == 0) ++holes;
                    if (alpha == 255) ++solid;
                }
            }
            CHECK_EQ(holes + solid, size * size);
            if (spec.cutout) {
                CHECK(holes > 0);
                CHECK(solid > 0);
                if (holes == 0 || solid == 0) {
                    std::printf("  tile %d (%s) at size %d: %d holes, %d solid\n", tile, spec.name,
                                size, holes, solid);
                }
            } else {
                CHECK_EQ(holes, 0);
            }
            int spread = interiorRange(sampler, tile).spread();
            CHECK(spread >= 8);
            if (spread < 8) {
                std::printf("  tile %d (%s) at size %d is flat: spread %d\n", tile, spec.name, size,
                            spread);
            }
        }
    }
}

VF_TEST(plankGrainFollowsTheBoardDirection) {
    TileSampler sampler;
    int size = sampler.params().tileSize;

    TileSpec board;
    board.name = "testBoard";
    board.pattern = TilePattern::Plank;
    board.material = TileMaterial::Wood;
    board.base = TileRgb{160, 120, 70};
    board.accent = TileRgb{90, 64, 34};
    board.noise = 0.0f;
    board.scale = 0.35f;
    board.density = 0.50f;

    TileSpec upright = board;
    upright.vertical = true;

    auto axisSpread = [&](const TileSpec& spec, bool alongColumns) {
        int lowest = 255;
        int highest = 0;
        for (int outer = 1; outer < size; ++outer) {
            int total = 0;
            for (int inner = 1; inner < size; ++inner) {
                int x = alongColumns ? outer : inner;
                int y = alongColumns ? inner : outer;
                total += luminance(sampler.pixelOf(spec, 900, x, y));
            }
            int mean = total / (size - 1);
            if (mean < lowest) lowest = mean;
            if (mean > highest) highest = mean;
        }
        return highest - lowest;
    };

    int uprightAcrossColumns = axisSpread(upright, true);
    int uprightAcrossRows = axisSpread(upright, false);
    CHECK(uprightAcrossColumns > uprightAcrossRows);

    int flatAcrossRows = axisSpread(board, false);
    int flatAcrossColumns = axisSpread(board, true);
    CHECK(flatAcrossRows > flatAcrossColumns);
}

VF_TEST(latticeTilesLeaveOpeningsBetweenTheirBars) {
    TileSampler sampler;
    int size = sampler.params().tileSize;
    int checked = 0;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        const TileSpec& spec = sampler.palette().specFor(tile);
        if (spec.pattern != TilePattern::Lattice || !spec.cutout) continue;
        ++checked;
        int holes = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (sampler.pixelAt(tile, x, y).a == 0) ++holes;
            }
        }
        float openFraction = static_cast<float>(holes) / static_cast<float>(size * size);
        CHECK(openFraction > 0.15f);
        CHECK(openFraction < 0.92f);
        if (openFraction <= 0.15f || openFraction >= 0.92f) {
            std::printf("  tile %d (%s) is %.2f open\n", tile, spec.name,
                        static_cast<double>(openFraction));
        }
    }
    CHECK(checked >= 8);
}

VF_TEST(oreVeinsCoverPartOfTheHostRockWithoutSwallowingIt) {
    TileSampler sampler;
    int size = sampler.params().tileSize;
    int checked = 0;
    for (int tile = 0; tile < renderedTiles(sampler); ++tile) {
        const TileSpec& spec = sampler.palette().specFor(tile);
        if (spec.pattern != TilePattern::OreSpeckle) continue;
        ++checked;
        int vein = 0;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (leansToAccent(sampler.pixelAt(tile, x, y), spec)) ++vein;
            }
        }
        float coverage = static_cast<float>(vein) / static_cast<float>(size * size);
        CHECK(coverage > 0.02f);
        CHECK(coverage < 0.55f);
        if (coverage <= 0.02f || coverage >= 0.55f) {
            std::printf("  tile %d (%s) vein coverage %.3f\n", tile, spec.name,
                        static_cast<double>(coverage));
        }
    }
    CHECK(checked >= 9);
}
