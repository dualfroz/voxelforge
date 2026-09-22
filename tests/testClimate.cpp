#include "testHarness.h"
#include "vf/Climate.h"
#include "vf/BiomeGen.h"

using namespace vf;

namespace {

ClimateConfig coldConfig() {
    ClimateConfig cfg;
    cfg.freezeTemperature = 100.0f;
    cfg.meltTemperature = 200.0f;
    return cfg;
}

ClimateConfig warmConfig() {
    ClimateConfig cfg;
    cfg.freezeTemperature = -100.0f;
    cfg.meltTemperature = -100.0f;
    return cfg;
}

ClimateConfig neutralConfig() {
    ClimateConfig cfg;
    cfg.freezeTemperature = -100.0f;
    cfg.meltTemperature = 200.0f;
    return cfg;
}

}

VF_TEST(temperatureFallsWithHeightAndLatitude) {
    BiomeGen biomes;
    Climate climate(biomes);
    float atSea = climate.temperatureAt(50, climate.settings().seaLevel, 0);
    float aloft = climate.temperatureAt(50, climate.settings().seaLevel + 80, 0);
    float polar = climate.temperatureAt(50, climate.settings().seaLevel, 4000);
    CHECK(aloft < atSea);
    CHECK(polar < atSea);
}

VF_TEST(exposedWaterFreezesWhenColdButCoveredWaterDoesNot) {
    BiomeGen biomes;
    Climate climate(biomes, coldConfig());
    World world;
    world.setBlock(10, 30, 10, Block::Water);
    world.setBlock(14, 30, 10, Block::Water);
    world.setBlock(14, 31, 10, Block::Stone);

    std::vector<ClimateChange> changes = climate.tick(world, Vec3i{8, 30, 8}, Vec3i{10, 4, 6},
                                                    Precipitation::None);
    Climate::apply(world, changes);

    CHECK(world.blockAt(10, 30, 10) == Block::Ice);
    CHECK(world.blockAt(14, 30, 10) == Block::Water);
}

VF_TEST(iceThawsWhenWarm) {
    BiomeGen biomes;
    Climate climate(biomes, warmConfig());
    World world;
    world.setBlock(5, 40, 5, Block::Ice);

    std::vector<ClimateChange> changes = climate.tick(world, Vec3i{4, 40, 4}, Vec3i{4, 2, 4},
                                                    Precipitation::None);
    Climate::apply(world, changes);
    CHECK(world.blockAt(5, 40, 5) == Block::Water);
}

VF_TEST(iceMeltsBesideHeatSourceOnlyWhenAdjacent) {
    BiomeGen biomes;
    Climate climate(biomes, neutralConfig());
    World world;
    world.setBlock(20, 50, 20, Block::Ice);
    world.setBlock(21, 50, 20, Block::Lava);
    world.setBlock(25, 50, 20, Block::Ice);

    std::vector<ClimateChange> changes = climate.tick(world, Vec3i{18, 49, 18}, Vec3i{12, 4, 6},
                                                    Precipitation::None);
    Climate::apply(world, changes);
    CHECK(world.blockAt(20, 50, 20) == Block::Water);
    CHECK(world.blockAt(25, 50, 20) == Block::Ice);
}

VF_TEST(snowAccumulatesUnderSnowfallAndStopsWithoutIt) {
    BiomeGen biomes;
    Climate climate(biomes, coldConfig());
    World world;
    world.setBlock(0, 30, 0, Block::Stone);
    Vec3i origin{-1, 30, -1};
    Vec3i dims{3, 5, 3};
    Vec3i snowCell{0, 31, 0};

    std::vector<ClimateChange> first = climate.tick(world, origin, dims, Precipitation::Snow);
    Climate::apply(world, first);
    CHECK(world.blockAt(0, 31, 0) == Block::SnowLayer);
    CHECK_EQ(climate.snowDepthAt(snowCell), 1);

    for (int i = 0; i < 3; ++i) {
        std::vector<ClimateChange> more = climate.tick(world, origin, dims, Precipitation::Snow);
        Climate::apply(world, more);
    }
    CHECK_EQ(climate.snowDepthAt(snowCell), 4);

    int depthBefore = climate.snowDepthAt(snowCell);
    std::vector<ClimateChange> dry = climate.tick(world, origin, dims, Precipitation::None);
    Climate::apply(world, dry);
    CHECK_EQ(climate.snowDepthAt(snowCell), depthBefore);
}

VF_TEST(snowDepthClampsAtMax) {
    BiomeGen biomes;
    ClimateConfig cfg = coldConfig();
    cfg.maxSnowDepth = 3;
    Climate climate(biomes, cfg);
    World world;
    world.setBlock(0, 30, 0, Block::Stone);
    Vec3i origin{-1, 30, -1};
    Vec3i dims{3, 5, 3};

    for (int i = 0; i < 10; ++i) {
        std::vector<ClimateChange> changes = climate.tick(world, origin, dims, Precipitation::Snow);
        Climate::apply(world, changes);
    }
    CHECK_EQ(climate.snowDepthAt(Vec3i{0, 31, 0}), 3);
}

VF_TEST(iceCompactsToPackedIceInProlongedCold) {
    BiomeGen biomes;
    ClimateConfig cfg = coldConfig();
    cfg.packThreshold = 4;
    Climate climate(biomes, cfg);
    World world;
    world.setBlock(5, 40, 5, Block::Ice);
    Vec3i origin{4, 40, 4};
    Vec3i dims{4, 3, 4};

    for (int i = 0; i < 3; ++i) {
        std::vector<ClimateChange> changes = climate.tick(world, origin, dims, Precipitation::None);
        Climate::apply(world, changes);
    }
    CHECK(world.blockAt(5, 40, 5) == Block::Ice);
    CHECK_EQ(climate.iceAgeAt(Vec3i{5, 40, 5}), 3);

    std::vector<ClimateChange> last = climate.tick(world, origin, dims, Precipitation::None);
    Climate::apply(world, last);
    CHECK(world.blockAt(5, 40, 5) == Block::PackedIce);
}

VF_TEST(frostSpreadsFromExistingIceButNotAcrossOpenWater) {
    BiomeGen biomes;
    ClimateConfig cfg;
    cfg.freezeTemperature = -100.0f;
    cfg.frostSpreadTemperature = 100.0f;
    cfg.meltTemperature = 200.0f;
    Climate climate(biomes, cfg);
    World world;
    world.setBlock(10, 40, 10, Block::Ice);
    world.setBlock(11, 40, 10, Block::Water);
    world.setBlock(20, 40, 10, Block::Water);

    std::vector<ClimateChange> changes = climate.tick(world, Vec3i{8, 40, 8}, Vec3i{16, 3, 6},
                                                    Precipitation::None);
    Climate::apply(world, changes);
    CHECK(world.blockAt(11, 40, 10) == Block::Ice);
    CHECK(world.blockAt(20, 40, 10) == Block::Water);
}

VF_TEST(packedIceMeltsBesideHeat) {
    BiomeGen biomes;
    ClimateConfig cfg = coldConfig();
    cfg.packThreshold = 2;
    Climate climate(biomes, cfg);
    World world;
    world.setBlock(5, 40, 5, Block::Ice);
    Vec3i origin{4, 40, 4};
    Vec3i dims{4, 3, 4};

    for (int i = 0; i < 2; ++i) {
        std::vector<ClimateChange> changes = climate.tick(world, origin, dims, Precipitation::None);
        Climate::apply(world, changes);
    }
    CHECK(world.blockAt(5, 40, 5) == Block::PackedIce);

    world.setBlock(6, 40, 5, Block::Lava);
    std::vector<ClimateChange> melt = climate.tick(world, origin, dims, Precipitation::None);
    Climate::apply(world, melt);
    CHECK(world.blockAt(5, 40, 5) == Block::Water);
}

VF_TEST(accumulatedSnowMeltsBesideHeat) {
    BiomeGen biomes;
    Climate climate(biomes, coldConfig());
    World world;
    world.setBlock(0, 30, 0, Block::Stone);
    Vec3i origin{-1, 30, -1};
    Vec3i dims{3, 5, 3};
    Vec3i snowCell{0, 31, 0};

    for (int i = 0; i < 5; ++i) {
        std::vector<ClimateChange> changes = climate.tick(world, origin, dims, Precipitation::Snow);
        Climate::apply(world, changes);
    }
    int peak = climate.snowDepthAt(snowCell);
    CHECK(peak > 1);

    world.setBlock(1, 31, 0, Block::Torch);
    for (int i = 0; i < peak; ++i) {
        std::vector<ClimateChange> changes = climate.tick(world, origin, dims, Precipitation::None);
        Climate::apply(world, changes);
    }
    CHECK_EQ(climate.snowDepthAt(snowCell), 0);
    CHECK(world.blockAt(0, 31, 0) == Block::Air);
}
