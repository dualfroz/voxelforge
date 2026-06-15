#include "testHarness.h"
#include "vf/Lighting.h"

using namespace vf;

namespace {

void fillBox(World& world, Vec3i lo, Vec3i hi, Block block) {
    for (int x = lo.x; x <= hi.x; ++x)
        for (int y = lo.y; y <= hi.y; ++y)
            for (int z = lo.z; z <= hi.z; ++z)
                world.setBlock(x, y, z, block);
}

} // namespace

VF_TEST(lightSunFloodsOpenColumnToFloor) {
    World world;
    fillBox(world, {0, 0, 0}, {7, 0, 7}, Block::Stone);   // single floor layer
    LightField field = computeLight(world, {0, 0, 0}, {8, 8, 8});
    CHECK_EQ(static_cast<int>(field.sunAtWorld(3, 1, 3)), maxLight);   // air above floor
    CHECK_EQ(static_cast<int>(field.sunAtWorld(3, 7, 3)), maxLight);   // top of column
}

VF_TEST(lightSealedPocketStaysDark) {
    World world;
    fillBox(world, {10, 10, 10}, {12, 12, 12}, Block::Stone);
    world.setBlock(11, 11, 11, Block::Air);   // sealed air cell in the core
    LightField field = computeLight(world, {9, 9, 9}, {5, 5, 5});
    CHECK_EQ(static_cast<int>(field.sunAtWorld(11, 11, 11)), 0);
    CHECK_EQ(static_cast<int>(field.blockAtWorld(11, 11, 11)), 0);
}

VF_TEST(lightSunAttenuatesAlongTunnel) {
    World world;
    fillBox(world, {0, 0, 0}, {11, 4, 2}, Block::Stone);
    for (int y = 1; y <= 4; ++y) world.setBlock(0, y, 1, Block::Air);   // vertical shaft
    for (int x = 1; x <= 9; ++x) world.setBlock(x, 1, 1, Block::Air);   // horizontal tunnel

    LightField field = computeLight(world, {0, 0, 0}, {12, 5, 3});
    CHECK_EQ(static_cast<int>(field.sunAtWorld(0, 1, 1)), maxLight);       // shaft foot
    CHECK_EQ(static_cast<int>(field.sunAtWorld(1, 1, 1)), maxLight - 1);
    CHECK_EQ(static_cast<int>(field.sunAtWorld(3, 1, 1)), maxLight - 3);
    CHECK_EQ(static_cast<int>(field.sunAtWorld(6, 1, 1)), maxLight - 6);
}

VF_TEST(lightBlockEmitterFallsOffRadially) {
    World world;
    world.setBlock(5, 5, 5, Block::Glowstone);
    LightField field = computeLight(world, {0, 0, 0}, {11, 11, 11});
    CHECK_EQ(static_cast<int>(field.blockAtWorld(5, 5, 5)), maxLight);
    CHECK_EQ(static_cast<int>(field.blockAtWorld(6, 5, 5)), maxLight - 1);
    CHECK_EQ(static_cast<int>(field.blockAtWorld(8, 5, 5)), maxLight - 3);
    CHECK_EQ(static_cast<int>(field.blockAtWorld(5, 5, 9)), maxLight - 4);
}

VF_TEST(lightCombinedTakesBrighterChannel) {
    World world;
    fillBox(world, {0, 0, 0}, {11, 11, 11}, Block::Stone);
    world.setBlock(5, 5, 5, Block::Glowstone);
    world.setBlock(6, 5, 5, Block::Air);   // lit only by the torch, no sky
    LightField field = computeLight(world, {0, 0, 0}, {12, 12, 12});
    CHECK_EQ(static_cast<int>(field.sunAtWorld(6, 5, 5)), 0);
    CHECK_EQ(static_cast<int>(field.blockAtWorld(6, 5, 5)), maxLight - 1);
    CHECK_NEAR(field.combinedAtWorld(6, 5, 5), static_cast<float>(maxLight - 1) / maxLight, 1e-4f);
}
