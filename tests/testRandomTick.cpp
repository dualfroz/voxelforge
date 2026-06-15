#include "testHarness.h"
#include "vf/RandomTick.h"
#include "vf/Chunk.h"

using namespace vf;

VF_TEST(sampleReturnsConfiguredCountInBounds) {
    RandomTick selector(123u, 5);
    std::vector<Vec3i> draw = selector.sampleLocal({0, 0, 0}, 0);
    CHECK_EQ(draw.size(), static_cast<std::size_t>(5));
    for (const Vec3i& cell : draw) {
        CHECK(cell.x >= 0 && cell.x < chunkSize);
        CHECK(cell.y >= 0 && cell.y < chunkSize);
        CHECK(cell.z >= 0 && cell.z < chunkSize);
    }
}

VF_TEST(zeroTicksPerChunkYieldsEmptyDraw) {
    RandomTick selector(1u, 0);
    CHECK_EQ(selector.ticksPerChunk(), 0);
    CHECK(selector.sampleLocal({2, 3, 4}, 9).empty());
}

VF_TEST(globalDrawIsLocalDrawOffsetByChunkOrigin) {
    RandomTick selector(77u, 6);
    Vec3i coord{1, -2, 3};
    long long tick = 41;
    std::vector<Vec3i> local = selector.sampleLocal(coord, tick);
    std::vector<Vec3i> global = selector.sampleGlobal(coord, tick);
    CHECK_EQ(local.size(), global.size());
    for (std::size_t i = 0; i < local.size(); ++i) {
        CHECK_EQ(global[i].x, local[i].x + coord.x * chunkSize);
        CHECK_EQ(global[i].y, local[i].y + coord.y * chunkSize);
        CHECK_EQ(global[i].z, local[i].z + coord.z * chunkSize);
    }
}

VF_TEST(sameSeedReplaysIdenticalStream) {
    RandomTick a(2024u, 4);
    RandomTick b(2024u, 4);
    for (int cx = -2; cx <= 2; ++cx)
        for (long long t = 0; t < 8; ++t)
            CHECK(a.sampleLocal({cx, 0, 0}, t) == b.sampleLocal({cx, 0, 0}, t));
}

VF_TEST(theTickNumberChangesTheDraw) {
    RandomTick selector(5u, 4);
    Vec3i coord{0, 0, 0};
    bool sawDifference = false;
    std::vector<Vec3i> first = selector.sampleLocal(coord, 0);
    for (long long t = 1; t < 16 && !sawDifference; ++t)
        if (selector.sampleLocal(coord, t) != first) sawDifference = true;
    CHECK(sawDifference);
}

VF_TEST(theSeedChangesTheDraw) {
    RandomTick a(1u, 6);
    RandomTick b(2u, 6);
    CHECK(a.sampleLocal({0, 0, 0}, 0) != b.sampleLocal({0, 0, 0}, 0));
}

VF_TEST(theChunkCoordinateChangesTheDraw) {
    RandomTick selector(9u, 6);
    CHECK(selector.sampleLocal({0, 0, 0}, 3) != selector.sampleLocal({1, 0, 0}, 3));
    CHECK(selector.sampleLocal({0, 0, 0}, 3) != selector.sampleLocal({0, 0, 1}, 3));
}

VF_TEST(hashDistinguishesIndicesAndIsStable) {
    RandomTick selector(31u, 4);
    Vec3i coord{4, 1, -3};
    uint64_t first = selector.hash(coord, 10, 0);
    uint64_t second = selector.hash(coord, 10, 1);
    CHECK(first != second);
    CHECK_EQ(first, selector.hash(coord, 10, 0));
}
