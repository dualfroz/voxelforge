#include "testHarness.h"
#include "vf/Serialize.h"
#include "vf/Terrain.h"

using namespace vf;

namespace {

bool sameOverRegion(const World& a, const World& b, int xMax, int yMax, int zMax) {
    for (int x = 0; x < xMax; ++x)
        for (int y = 0; y < yMax; ++y)
            for (int z = 0; z < zMax; ++z)
                if (a.blockAt(x, y, z) != b.blockAt(x, y, z)) return false;
    return true;
}

}

VF_TEST(serializeRoundTripsManualBlocks) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    world.setBlock(1, 2, 3, Block::Grass);
    world.setBlock(-5, 40, 17, Block::Water);
    world.setBlock(-5, 41, 17, Block::Leaves);

    std::vector<uint8_t> bytes = serializeWorld(world);
    World restored;
    CHECK(deserializeWorld(bytes.data(), bytes.size(), restored));

    CHECK(restored.blockAt(0, 0, 0) == Block::Stone);
    CHECK(restored.blockAt(1, 2, 3) == Block::Grass);
    CHECK(restored.blockAt(-5, 40, 17) == Block::Water);
    CHECK(restored.blockAt(-5, 41, 17) == Block::Leaves);
    CHECK_EQ(restored.chunkCount(), world.chunkCount());
}

VF_TEST(serializeRoundTripsTerrain) {
    TerrainParams params;
    params.chunksX = 2;
    params.chunksZ = 2;
    params.worldHeight = 48;
    World world;
    generateTerrain(world, params);

    std::vector<uint8_t> bytes = serializeWorld(world);
    World restored;
    CHECK(deserializeWorld(bytes.data(), bytes.size(), restored));
    CHECK_EQ(restored.chunkCount(), world.chunkCount());
    CHECK(sameOverRegion(world, restored, 2 * chunkSize, params.worldHeight, 2 * chunkSize));
}

VF_TEST(serializeRejectsBadMagic) {
    std::vector<uint8_t> junk = {0x00, 0x11, 0x22, 0x33, 0x44};
    World out;
    CHECK(!deserializeWorld(junk.data(), junk.size(), out));
}

VF_TEST(serializeRejectsTruncatedStream) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    std::vector<uint8_t> bytes = serializeWorld(world);
    bytes.resize(bytes.size() - 4);
    World out;
    CHECK(!deserializeWorld(bytes.data(), bytes.size(), out));
}

VF_TEST(serializeEmptyWorldRoundTrips) {
    World world;
    std::vector<uint8_t> bytes = serializeWorld(world);
    World restored;
    CHECK(deserializeWorld(bytes.data(), bytes.size(), restored));
    CHECK_EQ(restored.chunkCount(), static_cast<std::size_t>(0));
}
