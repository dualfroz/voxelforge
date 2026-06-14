#include "testHarness.h"
#include "vf/WorldGen.h"

#include <set>

using namespace vf;

VF_TEST(worldGenChunkIsDeterministic) {
    WorldGen gen;
    Chunk first, second;
    gen.generateChunk(first, {2, 1, -3});
    gen.generateChunk(second, {2, 1, -3});
    CHECK(first.raw() == second.raw());

    Chunk elsewhere;
    gen.generateChunk(elsewhere, {2, 1, 4});
    CHECK(first.raw() != elsewhere.raw());   // different columns differ
}

VF_TEST(worldGenSurfaceHeightStaysInBounds) {
    WorldGen gen;
    int lo = 1 << 30, hi = -(1 << 30);
    for (int gx = -200; gx <= 200; gx += 7) {
        for (int gz = -200; gz <= 200; gz += 7) {
            int h = gen.surfaceHeight(gx, gz);
            CHECK(h >= 1 && h < gen.settings().maxY);
            lo = h < lo ? h : lo;
            hi = h > hi ? h : hi;
        }
    }
    CHECK(hi > lo);   // terrain actually varies
}

VF_TEST(worldGenProducesSeveralBiomes) {
    WorldGen gen;
    std::set<int> seen;
    for (int gx = -600; gx <= 600; gx += 25)
        for (int gz = -600; gz <= 600; gz += 25)
            seen.insert(static_cast<int>(gen.biomeAt(gx, gz)));
    CHECK(seen.size() >= 3);
}

VF_TEST(worldGenOceanMatchesLowSurface) {
    WorldGen gen;
    int sea = gen.settings().seaLevel;
    for (int gx = -400; gx <= 400; gx += 13) {
        for (int gz = -400; gz <= 400; gz += 13) {
            bool low = gen.surfaceHeight(gx, gz) < sea - 1;
            bool ocean = gen.biomeAt(gx, gz) == Biome::Ocean;
            CHECK_EQ(low, ocean);
        }
    }
}

VF_TEST(worldGenCarvesCavesButNotEverything) {
    WorldGen gen;
    int caves = 0, solid = 0;
    for (int gx = 0; gx < 64; gx += 2)
        for (int gy = 4; gy < 40; gy += 2)
            for (int gz = 0; gz < 64; gz += 2)
                (gen.isCave(gx, gy, gz) ? caves : solid) += 1;
    CHECK(caves > 0);
    CHECK(solid > caves);   // caves are the minority
}

VF_TEST(worldGenPlacesOresUnderground) {
    WorldGen gen;
    World world;
    for (int cx = 0; cx < 2; ++cx)
        for (int cz = 0; cz < 2; ++cz)
            gen.generateColumn(world, cx, cz);

    int ores = 0;
    for (int gx = 0; gx < 32; ++gx) {
        for (int gz = 0; gz < 32; ++gz) {
            int surface = gen.surfaceHeight(gx, gz);
            for (int gy = 1; gy < surface; ++gy) {
                Block b = world.blockAt(gx, gy, gz);
                if (b == Block::CoalOre || b == Block::IronOre || b == Block::GoldOre) {
                    ++ores;
                    CHECK(gy < surface);   // ores are always below the surface
                }
            }
        }
    }
    CHECK(ores > 0);
}

VF_TEST(worldGenPlantsTrees) {
    WorldGen gen;
    World world;
    int wood = 0;
    for (int cx = 0; cx < 8; ++cx) {
        for (int cz = 0; cz < 8; ++cz) {
            gen.generateColumn(world, cx, cz);
        }
    }
    for (const auto& entry : world.allChunks()) {
        for (Block b : entry.second->raw())
            if (b == Block::Wood) ++wood;
    }
    CHECK(wood > 0);
}
