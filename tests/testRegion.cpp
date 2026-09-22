#include "testHarness.h"
#include "vf/Region.h"

using namespace vf;

namespace {

Chunk patternChunk(int seed) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) {
                int v = (x + y * 2 + z * 3 + seed) % static_cast<int>(Block::Count);
                chunk.set(x, y, z, static_cast<Block>(v));
            }
    return chunk;
}

}

VF_TEST(regionCoordFloorsNegatives) {
    CHECK(regionForChunk({0, 0, 0}) == (RegionCoord{0, 0}));
    CHECK(regionForChunk({15, 4, 15}) == (RegionCoord{0, 0}));
    CHECK(regionForChunk({16, 0, 0}) == (RegionCoord{1, 0}));
    CHECK(regionForChunk({-1, 0, -1}) == (RegionCoord{-1, -1}));
    CHECK(regionForChunk({-16, 0, -17}) == (RegionCoord{-1, -2}));
}

VF_TEST(regionPutGetRoundTrips) {
    Region region({0, 0});
    Chunk chunk = patternChunk(7);
    region.putChunk({1, 2, 3}, chunk);
    CHECK(region.hasChunk({1, 2, 3}));
    CHECK(!region.hasChunk({4, 4, 4}));

    Chunk back;
    CHECK(region.getChunk({1, 2, 3}, back));
    CHECK(back.raw() == chunk.raw());
    CHECK(!region.getChunk({9, 9, 9}, back));
}

VF_TEST(regionSerializeRoundTrips) {
    Region region({2, -1});
    Chunk a = patternChunk(1), b = patternChunk(2), c = patternChunk(3);
    region.putChunk({0, 0, 0}, a);
    region.putChunk({0, 5, 1}, b);
    region.putChunk({3, 1, 2}, c);

    std::vector<uint8_t> bytes = region.serialize();
    Region restored;
    CHECK(Region::deserialize(bytes.data(), bytes.size(), restored));
    CHECK(restored.location() == (RegionCoord{2, -1}));
    CHECK_EQ(restored.chunkCount(), static_cast<std::size_t>(3));

    Chunk out;
    CHECK(restored.getChunk({0, 5, 1}, out));
    CHECK(out.raw() == b.raw());
    CHECK(restored.getChunk({3, 1, 2}, out));
    CHECK(out.raw() == c.raw());
}

VF_TEST(regionRejectsBadInput) {
    Region region({0, 0});
    region.putChunk({0, 0, 0}, patternChunk(4));
    std::vector<uint8_t> bytes = region.serialize();

    Region out;
    bytes[0] ^= 0xff;
    CHECK(!Region::deserialize(bytes.data(), bytes.size(), out));
    bytes[0] ^= 0xff;
    CHECK(!Region::deserialize(bytes.data(), bytes.size() / 2, out));
}

VF_TEST(regionWorldSplitCoversMultipleRegions) {
    World world;
    world.setBlock(1, 1, 1, Block::Stone);
    world.setBlock(20 * chunkSize, 0, 0, Block::Grass);
    world.setBlock(-1, 40, -1, Block::Sand);

    auto regions = splitWorldIntoRegions(world);
    CHECK(regions.size() >= 3);

    std::vector<uint8_t> blob = serializeRegions(world);
    World restored;
    CHECK(deserializeRegions(blob.data(), blob.size(), restored));
    CHECK(restored.blockAt(1, 1, 1) == Block::Stone);
    CHECK(restored.blockAt(20 * chunkSize, 0, 0) == Block::Grass);
    CHECK(restored.blockAt(-1, 40, -1) == Block::Sand);
}
