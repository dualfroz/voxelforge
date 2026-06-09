#include "testHarness.h"
#include "vf/World.h"

using namespace vf;

VF_TEST(worldFloorDivHandlesNegatives) {
    CHECK_EQ(floorDiv(0, 16), 0);
    CHECK_EQ(floorDiv(15, 16), 0);
    CHECK_EQ(floorDiv(16, 16), 1);
    CHECK_EQ(floorDiv(-1, 16), -1);
    CHECK_EQ(floorDiv(-16, 16), -1);
    CHECK_EQ(floorDiv(-17, 16), -2);
}

VF_TEST(worldFloorModHandlesNegatives) {
    CHECK_EQ(floorMod(0, 16), 0);
    CHECK_EQ(floorMod(-1, 16), 15);
    CHECK_EQ(floorMod(16, 16), 0);
    CHECK_EQ(floorMod(-17, 16), 15);
}

VF_TEST(worldSetGetCrossesChunkBorders) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    world.setBlock(-1, 5, 3, Block::Grass);
    world.setBlock(31, 40, 17, Block::Sand);

    CHECK(world.blockAt(0, 0, 0) == Block::Stone);
    CHECK(world.blockAt(-1, 5, 3) == Block::Grass);
    CHECK(world.blockAt(31, 40, 17) == Block::Sand);
    CHECK(world.blockAt(100, 100, 100) == Block::Air);
}

VF_TEST(worldNegativeCoordinateLandsInExpectedChunk) {
    World world;
    world.setBlock(-1, -1, -1, Block::Wood);
    const Chunk* chunk = world.chunkAt({-1, -1, -1});
    CHECK(chunk != nullptr);
    CHECK(chunk->at(15, 15, 15) == Block::Wood);
    CHECK(world.solidAt(-1, -1, -1));
}

VF_TEST(worldEnsureChunkIsIdempotent) {
    World world;
    Chunk& first = world.ensureChunk({2, 0, 2});
    first.set(1, 1, 1, Block::Dirt);
    Chunk& second = world.ensureChunk({2, 0, 2});
    CHECK(&first == &second);
    CHECK_EQ(world.chunkCount(), static_cast<std::size_t>(1));
    CHECK(second.at(1, 1, 1) == Block::Dirt);
}
