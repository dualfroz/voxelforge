#include "testHarness.h"
#include "vf/LightEngine.h"

using namespace vf;

namespace {

void fillBox(World& world, Vec3i lo, Vec3i hi, Block block) {
    for (int x = lo.x; x <= hi.x; ++x)
        for (int y = lo.y; y <= hi.y; ++y)
            for (int z = lo.z; z <= hi.z; ++z)
                world.setBlock(x, y, z, block);
}

bool matchesRebuild(const LightEngine& incremental, World& world) {
    Vec3i origin = incremental.regionOrigin();
    Vec3i dims = incremental.regionDims();
    LightEngine fresh(world, origin, dims);
    fresh.rebuild();
    for (int lx = 0; lx < dims.x; ++lx)
        for (int ly = 0; ly < dims.y; ++ly)
            for (int lz = 0; lz < dims.z; ++lz) {
                int gx = origin.x + lx, gy = origin.y + ly, gz = origin.z + lz;
                if (incremental.sunAt(gx, gy, gz) != fresh.sunAt(gx, gy, gz)) return false;
                if (incremental.blockAt(gx, gy, gz) != fresh.blockAt(gx, gy, gz)) return false;
            }
    return true;
}

}

VF_TEST(lightEngineSkyFillsOpenColumnStraightDown) {
    World world;
    fillBox(world, {0, 0, 0}, {7, 0, 7}, Block::Stone);
    LightEngine engine(world, {0, 0, 0}, {8, 8, 8});
    engine.rebuild();
    CHECK_EQ(static_cast<int>(engine.sunAt(3, 7, 3)), maxLightLevel);
    CHECK_EQ(static_cast<int>(engine.sunAt(3, 1, 3)), maxLightLevel);
}

VF_TEST(lightEngineBlockEmitterFallsOffRadially) {
    World world;
    world.setBlock(5, 5, 5, Block::Glowstone);
    LightEngine engine(world, {0, 0, 0}, {11, 11, 11});
    engine.rebuild();
    CHECK_EQ(static_cast<int>(engine.blockAt(5, 5, 5)), maxLightLevel);
    CHECK_EQ(static_cast<int>(engine.blockAt(6, 5, 5)), maxLightLevel - 1);
    CHECK_EQ(static_cast<int>(engine.blockAt(8, 5, 5)), maxLightLevel - 3);
}

VF_TEST(lightEnginePlacingOpaqueDarkensColumnBelow) {
    World world;
    fillBox(world, {0, 0, 0}, {4, 11, 4}, Block::Stone);
    for (int y = 1; y <= 11; ++y) world.setBlock(2, y, 2, Block::Air);
    LightEngine engine(world, {0, 0, 0}, {5, 12, 5});
    engine.rebuild();
    CHECK_EQ(static_cast<int>(engine.sunAt(2, 1, 2)), maxLightLevel);

    engine.setBlock(2, 6, 2, Block::Stone);
    CHECK_EQ(static_cast<int>(engine.sunAt(2, 5, 2)), 0);
    CHECK_EQ(static_cast<int>(engine.sunAt(2, 7, 2)), maxLightLevel);
    CHECK(matchesRebuild(engine, world));
}

VF_TEST(lightEngineBreakingOpaqueRePropagates) {
    World world;
    fillBox(world, {0, 0, 0}, {4, 11, 4}, Block::Stone);
    for (int y = 1; y <= 11; ++y) world.setBlock(2, y, 2, Block::Air);
    world.setBlock(2, 6, 2, Block::Stone);
    LightEngine engine(world, {0, 0, 0}, {5, 12, 5});
    engine.rebuild();
    CHECK_EQ(static_cast<int>(engine.sunAt(2, 5, 2)), 0);

    engine.setBlock(2, 6, 2, Block::Air);
    CHECK_EQ(static_cast<int>(engine.sunAt(2, 5, 2)), maxLightLevel);
    CHECK(matchesRebuild(engine, world));
}

VF_TEST(lightEngineRemovingEmitterDarkensRoom) {
    World world;
    fillBox(world, {0, 0, 0}, {8, 8, 8}, Block::Stone);
    fillBox(world, {1, 1, 1}, {7, 7, 7}, Block::Air);
    world.setBlock(4, 4, 4, Block::Glowstone);
    LightEngine engine(world, {0, 0, 0}, {9, 9, 9});
    engine.rebuild();
    CHECK(engine.blockAt(5, 4, 4) > 0);

    engine.setBlock(4, 4, 4, Block::Air);
    CHECK_EQ(static_cast<int>(engine.blockAt(5, 4, 4)), 0);
    CHECK_EQ(static_cast<int>(engine.blockAt(4, 4, 4)), 0);
    CHECK(matchesRebuild(engine, world));
}

VF_TEST(lightEngineEditAcrossChunkBorderMatchesRebuild) {
    World world;
    fillBox(world, {12, 0, 12}, {35, 0, 35}, Block::Stone);
    LightEngine engine(world, {12, 0, 12}, {24, 20, 24});
    engine.rebuild();

    world.setBlock(0, 0, 0, Block::Air);
    engine.setBlock(15, 5, 15, Block::Glowstone);
    engine.setBlock(16, 5, 16, Block::Stone);
    engine.setBlock(31, 5, 31, Block::Glowstone);
    engine.setBlock(31, 5, 31, Block::Air);
    CHECK(matchesRebuild(engine, world));
}

VF_TEST(lightEngineEditSequenceStaysConsistentWithRebuild) {
    World world;
    fillBox(world, {0, 0, 0}, {15, 0, 15}, Block::Stone);
    LightEngine engine(world, {0, 0, 0}, {16, 16, 16});
    engine.rebuild();

    const Block palette[4] = {Block::Stone, Block::Air, Block::Glowstone, Block::Water};
    uint32_t rng = 0x1234567u;
    auto nextRng = [&rng]() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    for (int i = 0; i < 120; ++i) {
        int x = static_cast<int>(nextRng() % 16);
        int y = 1 + static_cast<int>(nextRng() % 12);
        int z = static_cast<int>(nextRng() % 16);
        Block b = palette[nextRng() % 4];
        engine.setBlock(x, y, z, b);
    }
    CHECK(matchesRebuild(engine, world));
}
