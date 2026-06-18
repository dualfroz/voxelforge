#include "testHarness.h"
#include "vf/Farming.h"

using namespace vf;

namespace {

int countLeaves(World& world, const Vec3i& centre, int radius, int yLow, int yHigh) {
    int count = 0;
    for (int x = centre.x - radius; x <= centre.x + radius; ++x)
        for (int y = yLow; y <= yHigh; ++y)
            for (int z = centre.z - radius; z <= centre.z + radius; ++z)
                if (world.blockAt(x, y, z) == Block::Leaves) ++count;
    return count;
}

} // namespace

VF_TEST(groundCropRipensThroughStagesInLight) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(1u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);
    CHECK_EQ(farm.stateAt(crop)->stage, 0);

    for (int i = 0; i < 4000 && !farm.stateAt(crop)->mature(); ++i) farm.growStep(world, 15);
    CHECK(farm.stateAt(crop)->mature());
    CHECK_EQ(farm.stateAt(crop)->stage, 7);
    CHECK(world.blockAt(0, 64, 0) == Block::Wheat);
}

VF_TEST(cropDoesNotGrowInDarkness) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(1u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);

    for (int i = 0; i < 1000; ++i) farm.growStep(world, 0);
    CHECK_EQ(farm.stateAt(crop)->stage, 0);
}

VF_TEST(hydrationAcceleratesGrowth) {
    Vec3i crop{0, 64, 0};

    World dryWorld;
    dryWorld.setBlock(0, 63, 0, Block::Farmland);
    Farming dryFarm(50u);
    dryFarm.plant(dryWorld, crop, CropType::Wheat);
    CHECK(!dryFarm.isHydrated(dryWorld, Vec3i{0, 63, 0}));
    int drySteps = 0;
    while (!dryFarm.stateAt(crop)->mature() && drySteps < 100000) {
        dryFarm.growStep(dryWorld, 15);
        ++drySteps;
    }

    World wetWorld;
    wetWorld.setBlock(0, 63, 0, Block::Farmland);
    wetWorld.setBlock(2, 63, 0, Block::Water);
    Farming wetFarm(50u);
    wetFarm.plant(wetWorld, crop, CropType::Wheat);
    CHECK(wetFarm.isHydrated(wetWorld, Vec3i{0, 63, 0}));
    int wetSteps = 0;
    while (!wetFarm.stateAt(crop)->mature() && wetSteps < 100000) {
        wetFarm.growStep(wetWorld, 15);
        ++wetSteps;
    }

    CHECK(wetFarm.stateAt(crop)->mature());
    CHECK(dryFarm.stateAt(crop)->mature());
    CHECK(wetSteps < drySteps);
}

VF_TEST(sugarCaneGrowsBesideWaterUpToThree) {
    World world;
    world.setBlock(0, 63, 0, Block::Dirt);
    world.setBlock(1, 63, 0, Block::Water);
    Farming farm(2u);
    Vec3i base{0, 64, 0};
    farm.plant(world, base, CropType::SugarCane);

    for (int i = 0; i < 4000 && farm.stateAt(base)->stage < 2; ++i) farm.growStep(world, 15);
    CHECK(world.blockAt(0, 65, 0) == Block::SugarCane);
    CHECK(world.blockAt(0, 66, 0) == Block::SugarCane);
    CHECK(world.blockAt(0, 67, 0) == Block::Air);
    CHECK_EQ(farm.stateAt(base)->stage, 2);
}

VF_TEST(sugarCaneWillNotGrowWithoutWater) {
    World world;
    world.setBlock(0, 63, 0, Block::Dirt);
    Farming farm(2u);
    Vec3i base{0, 64, 0};
    farm.plant(world, base, CropType::SugarCane);

    for (int i = 0; i < 800; ++i) farm.growStep(world, 15);
    CHECK_EQ(farm.stateAt(base)->stage, 0);
    CHECK(world.blockAt(0, 65, 0) == Block::Air);
}

VF_TEST(cactusGrowsAloneButNotBesideSolids) {
    World open;
    open.setBlock(0, 63, 0, Block::Sand);
    Farming lonely(3u);
    Vec3i base{0, 64, 0};
    lonely.plant(open, base, CropType::Cactus);
    for (int i = 0; i < 4000 && lonely.stateAt(base)->stage < 2; ++i) lonely.growStep(open, 15);
    CHECK(open.blockAt(0, 65, 0) == Block::Cactus);
    CHECK(open.blockAt(0, 66, 0) == Block::Cactus);
    CHECK_EQ(lonely.stateAt(base)->stage, 2);

    World crowded;
    crowded.setBlock(0, 63, 0, Block::Sand);
    crowded.setBlock(1, 65, 0, Block::Stone);
    Farming blocked(3u);
    blocked.plant(crowded, base, CropType::Cactus);
    for (int i = 0; i < 800; ++i) blocked.growStep(crowded, 15);
    CHECK_EQ(blocked.stateAt(base)->stage, 0);
    CHECK(crowded.blockAt(0, 65, 0) == Block::Air);
}

VF_TEST(stemMaturesAndSetsFruitOnFreeTile) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    world.setBlock(1, 63, 0, Block::Dirt);
    Farming farm(4u);
    Vec3i stem{0, 64, 0};
    farm.plant(world, stem, CropType::PumpkinStem);

    bool fruited = false;
    for (int i = 0; i < 8000 && !fruited; ++i) {
        farm.growStep(world, 15);
        fruited = world.blockAt(1, 64, 0) == Block::Pumpkin;
    }
    CHECK(fruited);
    CHECK(world.blockAt(1, 64, 0) == Block::Pumpkin);
}

VF_TEST(saplingBecomesTree) {
    World world;
    world.setBlock(0, 63, 0, Block::Dirt);
    Farming farm(5u);
    Vec3i sapling{0, 64, 0};
    farm.plant(world, sapling, CropType::Sapling);

    for (int i = 0; i < 8000 && farm.stateAt(sapling) != nullptr; ++i) farm.growStep(world, 15);
    CHECK(farm.stateAt(sapling) == nullptr);
    CHECK(world.blockAt(0, 64, 0) == Block::Wood);
    CHECK(world.blockAt(0, 65, 0) == Block::Wood);
    CHECK(countLeaves(world, Vec3i{0, 68, 0}, 2, 64, 72) > 0);
}

VF_TEST(harvestMatureWheatYieldsCropAndClears) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(6u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);
    for (int i = 0; i < 4000 && !farm.stateAt(crop)->mature(); ++i) farm.growStep(world, 15);

    std::vector<HarvestDrop> drops = farm.harvest(world, crop);
    CHECK(!drops.empty());
    CHECK(drops[0].item == Block::Wheat);
    CHECK(drops[0].count >= 1);
    CHECK(world.blockAt(0, 64, 0) == Block::Air);
    CHECK(farm.stateAt(crop) == nullptr);
    CHECK_EQ(farm.trackedCount(), static_cast<std::size_t>(0));
}

VF_TEST(harvestImmatureWheatDropsNothing) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(6u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);

    std::vector<HarvestDrop> drops = farm.harvest(world, crop);
    CHECK(drops.empty());
    CHECK(world.blockAt(0, 64, 0) == Block::Air);
    CHECK(farm.stateAt(crop) == nullptr);
}

VF_TEST(harvestSugarCaneYieldsFullHeight) {
    World world;
    world.setBlock(0, 63, 0, Block::Dirt);
    world.setBlock(1, 63, 0, Block::Water);
    Farming farm(7u);
    Vec3i base{0, 64, 0};
    farm.plant(world, base, CropType::SugarCane);
    for (int i = 0; i < 4000 && farm.stateAt(base)->stage < 2; ++i) farm.growStep(world, 15);

    std::vector<HarvestDrop> drops = farm.harvest(world, base);
    CHECK_EQ(drops.size(), static_cast<std::size_t>(1));
    CHECK(drops[0].item == Block::SugarCane);
    CHECK_EQ(drops[0].count, 3);
    CHECK(world.blockAt(0, 64, 0) == Block::Air);
    CHECK(world.blockAt(0, 65, 0) == Block::Air);
    CHECK(world.blockAt(0, 66, 0) == Block::Air);
}

VF_TEST(bonemealShovesGroundCropForward) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(1u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);

    CHECK(farm.applyBonemeal(world, crop));
    CHECK(farm.stateAt(crop)->stage >= 2);

    World sandWorld;
    sandWorld.setBlock(0, 63, 0, Block::Sand);
    Farming cactusFarm(1u);
    cactusFarm.plant(sandWorld, crop, CropType::Cactus);
    CHECK(!cactusFarm.applyBonemeal(sandWorld, crop));
}

VF_TEST(bonemealCanRipenAndHarvest) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(8u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Carrot);

    for (int i = 0; i < 20 && !farm.stateAt(crop)->mature(); ++i) farm.applyBonemeal(world, crop);
    CHECK(farm.stateAt(crop)->mature());
    std::vector<HarvestDrop> drops = farm.harvest(world, crop);
    CHECK(!drops.empty());
    CHECK(drops[0].item == Block::Carrot);
}

VF_TEST(bonemealEventuallyGrowsSaplingIntoTree) {
    World world;
    world.setBlock(0, 63, 0, Block::Dirt);
    Farming farm(9u);
    Vec3i sapling{0, 64, 0};
    farm.plant(world, sapling, CropType::Sapling);

    for (int i = 0; i < 100 && farm.stateAt(sapling) != nullptr; ++i) farm.applyBonemeal(world, sapling);
    CHECK(farm.stateAt(sapling) == nullptr);
    CHECK(world.blockAt(0, 64, 0) == Block::Wood);
}

VF_TEST(tramplingRevertsFarmlandAndKillsCrop) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    Farming farm(1u);
    Vec3i crop{0, 64, 0};
    farm.plant(world, crop, CropType::Wheat);

    CHECK(farm.trample(world, Vec3i{0, 63, 0}));
    CHECK(world.blockAt(0, 63, 0) == Block::Dirt);
    CHECK(world.blockAt(0, 64, 0) == Block::Air);
    CHECK(farm.stateAt(crop) == nullptr);
    CHECK(!farm.trample(world, Vec3i{5, 5, 5}));
}

VF_TEST(vineHangsDownFromAnchorOnly) {
    World anchored;
    anchored.setBlock(1, 64, 0, Block::Stone);
    Farming climber(2u);
    Vec3i head{0, 64, 0};
    climber.plant(anchored, head, CropType::Vine);
    for (int i = 0; i < 4000 && climber.stateAt(head)->stage < 4; ++i) climber.growStep(anchored, 15);
    CHECK(anchored.blockAt(0, 63, 0) == Block::Vine);
    CHECK(anchored.blockAt(0, 60, 0) == Block::Vine);
    CHECK_EQ(climber.stateAt(head)->stage, 4);

    World floating;
    Farming unanchored(2u);
    unanchored.plant(floating, head, CropType::Vine);
    for (int i = 0; i < 800; ++i) unanchored.growStep(floating, 15);
    CHECK_EQ(unanchored.stateAt(head)->stage, 0);
    CHECK(floating.blockAt(0, 63, 0) == Block::Air);
}

VF_TEST(sameSeedGrowsIdentically) {
    World left;
    left.setBlock(0, 63, 0, Block::Farmland);
    World right;
    right.setBlock(0, 63, 0, Block::Farmland);
    Farming leftFarm(123u);
    Farming rightFarm(123u);
    Vec3i crop{0, 64, 0};
    leftFarm.plant(left, crop, CropType::Wheat);
    rightFarm.plant(right, crop, CropType::Wheat);

    for (int i = 0; i < 50; ++i) {
        leftFarm.growStep(left, 15);
        rightFarm.growStep(right, 15);
    }
    CHECK_EQ(leftFarm.stateAt(crop)->stage, rightFarm.stateAt(crop)->stage);
}
