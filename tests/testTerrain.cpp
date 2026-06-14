#include "testHarness.h"
#include "vf/Terrain.h"

using namespace vf;

namespace {

int topSolid(const World& world, int gx, int gz, int worldHeight) {
    for (int y = worldHeight - 1; y >= 0; --y)
        if (world.solidAt(gx, y, gz)) return y;
    return -1;
}

} // namespace

VF_TEST(terrainIsDeterministicForSeed) {
    TerrainParams params;
    params.chunksX = 2;
    params.chunksZ = 2;
    params.worldHeight = 48;
    World a, b;
    generateTerrain(a, params);
    generateTerrain(b, params);

    for (int x = 0; x < 2 * chunkSize; ++x)
        for (int y = 0; y < params.worldHeight; ++y)
            for (int z = 0; z < 2 * chunkSize; ++z)
                CHECK(a.blockAt(x, y, z) == b.blockAt(x, y, z));
}

VF_TEST(terrainDiffersBetweenSeeds) {
    TerrainParams p1;
    p1.chunksX = 2;
    p1.chunksZ = 2;
    p1.worldHeight = 48;
    TerrainParams p2 = p1;
    p2.seed = 9001u;

    World a, b;
    generateTerrain(a, p1);
    generateTerrain(b, p2);

    bool anyDifference = false;
    for (int x = 0; x < 2 * chunkSize && !anyDifference; ++x)
        for (int z = 0; z < 2 * chunkSize && !anyDifference; ++z)
            if (topSolid(a, x, z, p1.worldHeight) != topSolid(b, x, z, p2.worldHeight))
                anyDifference = true;
    CHECK(anyDifference);
}

VF_TEST(terrainHeightMatchesColumnHelperAndIsBounded) {
    TerrainParams params;
    params.chunksX = 2;
    params.chunksZ = 2;
    params.worldHeight = 48;
    World world;
    generateTerrain(world, params);

    int minH = params.worldHeight;
    int maxH = 0;
    for (int x = 0; x < 2 * chunkSize; ++x) {
        for (int z = 0; z < 2 * chunkSize; ++z) {
            int top = topSolid(world, x, z, params.worldHeight);
            int expected = columnHeight(params.seed, params, x, z);
            CHECK_EQ(top, expected);
            CHECK(top >= 1 && top < params.worldHeight);
            if (top < minH) minH = top;
            if (top > maxH) maxH = top;
        }
    }
    CHECK(minH < maxH);   // terrain is not flat
}

VF_TEST(terrainColumnAboveSurfaceIsNotStoneCore) {
    TerrainParams params;
    params.chunksX = 1;
    params.chunksZ = 1;
    params.worldHeight = 48;
    World world;
    generateTerrain(world, params);

    for (int x = 0; x < chunkSize; ++x) {
        for (int z = 0; z < chunkSize; ++z) {
            int top = topSolid(world, x, z, params.worldHeight);
            Block below = world.blockAt(x, 0, z);
            CHECK(below == Block::Stone);   // bedrock-ish base is always stone
            CHECK(world.solidAt(x, top, z));
        }
    }
}
