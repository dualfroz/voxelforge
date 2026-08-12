#include "testHarness.h"
#include "vf/Dimension.h"

#include <set>
#include <string>

using namespace vf;

VF_TEST(dimensionConfigDescribesEachWorldsOwnRules) {
    DimensionConfig overworld = DimensionConfig::forDimension(DimensionId::Overworld);
    DimensionConfig nether = DimensionConfig::forDimension(DimensionId::Nether);
    DimensionConfig end = DimensionConfig::forDimension(DimensionId::End);

    CHECK(overworld.hasSky);
    CHECK(overworld.hasDayCycle);
    CHECK(!overworld.hasCeiling);
    CHECK(!overworld.bedsExplode);
    CHECK(!overworld.waterEvaporates);
    CHECK_NEAR(overworld.coordinateScale, 1.0f, 1e-6f);

    CHECK(!nether.hasSky);
    CHECK(!nether.hasDayCycle);
    CHECK(nether.hasCeiling);
    CHECK(nether.bedsExplode);
    CHECK(nether.waterEvaporates);
    CHECK_NEAR(nether.coordinateScale, 8.0f, 1e-6f);
    CHECK(nether.ambientLight > overworld.ambientLight);

    CHECK(!end.hasSky);
    CHECK(!end.hasDayCycle);
    CHECK(!end.hasCeiling);
    CHECK(end.bedsExplode);
    CHECK(!end.waterEvaporates);
    CHECK_NEAR(end.coordinateScale, 1.0f, 1e-6f);

    CHECK(std::string(DimensionConfig::nameOf(DimensionId::Nether)) == "nether");
    CHECK(std::string(DimensionConfig::nameOf(DimensionId::End)) == "end");
    CHECK(std::string(DimensionConfig::nameOf(DimensionId::Overworld)) == "overworld");
}

VF_TEST(dimensionConfigClampsToItsOwnPlaceableRange) {
    DimensionConfig nether = DimensionConfig::forDimension(DimensionId::Nether);
    DimensionConfig overworld = DimensionConfig::forDimension(DimensionId::Overworld);

    CHECK_EQ(nether.clampY(-40), nether.lowestPlaceableY());
    CHECK_EQ(nether.clampY(9000), nether.highestPlaceableY());
    CHECK_EQ(nether.clampY(60), 60);

    // A dimension with a solid roof keeps one more block of margin than an
    // open-sky one.
    CHECK(nether.highestPlaceableY() < overworld.highestPlaceableY());
    CHECK_EQ(nether.height(), 128);
}

VF_TEST(dimensionSetHoldsOneIndependentWorldPerDimension) {
    DimensionSet dimensions(2026u);

    CHECK_EQ(static_cast<int>(dimensions.overworld().id()), static_cast<int>(DimensionId::Overworld));
    CHECK_EQ(static_cast<int>(dimensions.nether().id()), static_cast<int>(DimensionId::Nether));
    CHECK_EQ(static_cast<int>(dimensions.end().id()), static_cast<int>(DimensionId::End));

    std::set<uint32_t> seeds;
    seeds.insert(dimensions.overworld().seed());
    seeds.insert(dimensions.nether().seed());
    seeds.insert(dimensions.end().seed());
    CHECK_EQ(seeds.size(), static_cast<std::size_t>(3));

    dimensions.nether().generateColumn(0, 0);
    CHECK(dimensions.nether().world().chunkCount() > 0);
    CHECK_EQ(dimensions.overworld().world().chunkCount(), static_cast<std::size_t>(0));
    CHECK_EQ(dimensions.end().world().chunkCount(), static_cast<std::size_t>(0));
    CHECK_EQ(dimensions.totalChunks(), dimensions.nether().world().chunkCount());
}

VF_TEST(dimensionSeedsFollowTheBaseSeedDeterministically) {
    DimensionSet first(99u);
    DimensionSet second(99u);
    DimensionSet other(100u);

    CHECK_EQ(first.nether().seed(), second.nether().seed());
    CHECK_EQ(first.end().seed(), second.end().seed());
    CHECK(first.nether().seed() != other.nether().seed());
    CHECK(first.overworld().seed() != first.nether().seed());
}

VF_TEST(dimensionsTickIndependentlyOfEachOther) {
    DimensionSet dimensions(7u);

    dimensions.tickOnly(DimensionId::Nether, 500);
    CHECK_EQ(dimensions.nether().ticks(), 500ll);
    CHECK_EQ(dimensions.overworld().ticks(), 0ll);

    dimensions.tickAll(100);
    CHECK_EQ(dimensions.nether().ticks(), 600ll);
    CHECK_EQ(dimensions.overworld().ticks(), 100ll);
    CHECK_EQ(dimensions.end().ticks(), 100ll);

    dimensions.tickAll(11900);
    CHECK_NEAR(dimensions.overworld().dayFraction(), 0.5f, 1e-4f);
    CHECK_NEAR(dimensions.nether().dayFraction(), 0.0f, 1e-6f);
    CHECK_NEAR(dimensions.end().dayFraction(), 0.0f, 1e-6f);
}

VF_TEST(eachDimensionOwnsExactlyOneGenerator) {
    DimensionSet dimensions(11u);

    CHECK(dimensions.overworld().overworldGenerator() != nullptr);
    CHECK(dimensions.overworld().netherGenerator() == nullptr);
    CHECK(dimensions.overworld().endGenerator() == nullptr);

    CHECK(dimensions.nether().netherGenerator() != nullptr);
    CHECK(dimensions.nether().overworldGenerator() == nullptr);

    CHECK(dimensions.end().endGenerator() != nullptr);
    CHECK(dimensions.end().netherGenerator() == nullptr);
}

VF_TEST(dimensionGeneratesTerrainFromItsOwnGenerator) {
    DimensionSet dimensions(31u);

    CHECK(!dimensions.nether().columnGenerated(2, -1));
    dimensions.nether().generateColumn(2, -1);
    CHECK(dimensions.nether().columnGenerated(2, -1));
    CHECK_EQ(dimensions.nether().generatedColumnCount(), static_cast<std::size_t>(1));

    int netherrack = 0;
    int endStone = 0;
    for (int gx = 32; gx < 48; ++gx)
        for (int gz = -16; gz < 0; ++gz)
            for (int gy = 0; gy < 128; ++gy)
                if (dimensions.nether().world().blockAt(gx, gy, gz) == Block::Netherrack) ++netherrack;
    CHECK(netherrack > 500);

    dimensions.end().generateColumn(0, 0);
    for (int gy = 0; gy < 128; ++gy)
        if (dimensions.end().world().blockAt(4, gy, 4) == Block::EndStone) ++endStone;
    CHECK(endStone > 0);

    // A second request for the same column is a no-op, not a regeneration.
    std::size_t before = dimensions.nether().world().chunkCount();
    dimensions.nether().generateColumn(2, -1);
    CHECK_EQ(dimensions.nether().world().chunkCount(), before);
}

VF_TEST(generateAreaFillsTheWholeNeighbourhoodOnce) {
    DimensionSet dimensions(5u);
    int made = dimensions.end().generateArea(0, 0, 1);
    CHECK_EQ(made, 9);
    CHECK_EQ(dimensions.end().generateArea(0, 0, 1), 0);
    CHECK_EQ(dimensions.end().generatedColumnCount(), static_cast<std::size_t>(9));
}

VF_TEST(standingSpotNeedsSolidGroundAndHeadroom) {
    DimensionSet dimensions(3u);
    World& world = dimensions.overworld().world();

    for (int gy = 0; gy <= 40; ++gy) world.setBlock(10, gy, 10, Block::Stone);
    int feetY = 0;
    CHECK(dimensions.overworld().standingSpot(10, 10, 0, 3, feetY));
    CHECK_EQ(feetY, 41);

    // A ceiling one block above the ground leaves no room to stand.
    world.setBlock(10, 42, 10, Block::Stone);
    CHECK(dimensions.overworld().standingSpot(10, 10, 0, 3, feetY));
    CHECK_EQ(feetY, 43);

    CHECK(!dimensions.overworld().standingSpot(11, 11, 0, 3, feetY));
    CHECK_EQ(dimensions.overworld().highestSolidY(10, 10), 42);
    CHECK_EQ(dimensions.overworld().highestSolidY(11, 11),
             dimensions.overworld().config().floorY - 1);
}

VF_TEST(netherTravelDividesHorizontalCoordinatesByEight) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);

    TravelResult result = travel.plan(DimensionId::Overworld, DimensionId::Nether,
                                      {800, 60, -400});
    CHECK_EQ(result.target.x, 100);
    CHECK_EQ(result.target.z, -50);
    CHECK_EQ(result.target.y, 60);
    CHECK(!result.clampedToFloor);
    CHECK(!result.clampedToCeiling);
    CHECK_EQ(result.searchRadius, 128);
    CHECK(result.needsPortal);

    // Rounding is a floor, so a coordinate that is not a multiple of eight
    // lands on the block below rather than being pulled towards zero.
    TravelResult negative = travel.plan(DimensionId::Overworld, DimensionId::Nether,
                                        {-9, 60, -1});
    CHECK_EQ(negative.target.x, -2);
    CHECK_EQ(negative.target.z, -1);
}

VF_TEST(netherTravelMultipliesBackByEightOnTheWayHome) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);

    TravelResult home = travel.plan(DimensionId::Nether, DimensionId::Overworld,
                                    {100, 60, -50});
    CHECK_EQ(home.target.x, 800);
    CHECK_EQ(home.target.z, -400);
    CHECK_EQ(home.searchRadius, 16);

    Vec3i start{1600, 70, 2400};
    TravelResult down = travel.plan(DimensionId::Overworld, DimensionId::Nether, start);
    TravelResult up = travel.plan(DimensionId::Nether, DimensionId::Overworld, down.target);
    CHECK_EQ(up.target.x, start.x);
    CHECK_EQ(up.target.z, start.z);
}

VF_TEST(travelBetweenEqualScalesLeavesCoordinatesAlone) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);

    TravelResult result = travel.plan(DimensionId::Overworld, DimensionId::End, {123, 64, -77});
    CHECK_EQ(result.target.x, 123);
    CHECK_EQ(result.target.z, -77);
    CHECK_EQ(result.target.y, 64);
    CHECK_EQ(result.searchRadius, 16);
}

VF_TEST(travelClampsArrivalIntoTheDestinationBounds) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);
    DimensionConfig nether = DimensionConfig::forDimension(DimensionId::Nether);

    TravelResult high = travel.plan(DimensionId::Overworld, DimensionId::Nether, {0, 300, 0});
    CHECK(high.clampedToCeiling);
    CHECK(!high.clampedToFloor);
    CHECK_EQ(high.target.y, nether.highestPlaceableY());
    CHECK_EQ(high.scaledTarget.y, 300);

    TravelResult low = travel.plan(DimensionId::Overworld, DimensionId::Nether, {0, -12, 0});
    CHECK(low.clampedToFloor);
    CHECK_EQ(low.target.y, nether.lowestPlaceableY());
}

VF_TEST(travelReusesAKnownPortalOnlyInsideTheSearchRadius) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);

    std::vector<Vec3i> nearby{{500, 60, 500}, {104, 62, -48}};
    TravelResult reuse = travel.plan(DimensionId::Overworld, DimensionId::Nether,
                                     {800, 60, -400}, nearby);
    CHECK(!reuse.needsPortal);
    CHECK_EQ(reuse.nearestIndex, 1);
    CHECK_EQ(reuse.nearestDistanceSquared, 4 * 4 + 2 * 2 + 2 * 2);

    std::vector<Vec3i> faraway{{500, 60, 500}};
    TravelResult build = travel.plan(DimensionId::Overworld, DimensionId::Nether,
                                     {800, 60, -400}, faraway);
    CHECK(build.needsPortal);
    CHECK_EQ(build.nearestIndex, -1);

    // The overworld radius is far tighter, so the same offset no longer counts.
    std::vector<Vec3i> homeSide{{820, 60, -400}};
    TravelResult tooFar = travel.plan(DimensionId::Nether, DimensionId::Overworld,
                                      {100, 60, -50}, homeSide);
    CHECK(tooFar.needsPortal);
}

VF_TEST(travelPicksTheClosestOfSeveralCandidates) {
    DimensionSet dimensions(1u);
    DimensionTravel travel(dimensions);

    std::vector<Vec3i> candidates{{110, 60, -50}, {102, 60, -50}, {100, 60, -56}};
    TravelResult result = travel.plan(DimensionId::Overworld, DimensionId::Nether,
                                      {800, 60, -400}, candidates);
    CHECK(!result.needsPortal);
    CHECK_EQ(result.nearestIndex, 1);
    CHECK_EQ(result.nearestDistanceSquared, 4);
}

VF_TEST(findSafeSpotPrefersGroundNearTheArrivalHeight) {
    DimensionSet dimensions(17u);
    World& world = dimensions.overworld().world();

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            world.setBlock(50 + dx, 5, 50 + dz, Block::Stone);
            world.setBlock(50 + dx, 60, 50 + dz, Block::Stone);
        }
    }

    DimensionTravel travel(dimensions);
    Vec3i spot;
    CHECK(travel.findSafeSpot(DimensionId::Overworld, {50, 58, 50}, 4, 4, spot));
    CHECK_EQ(spot.y, 61);
    CHECK_EQ(spot.x, 50);
    CHECK_EQ(spot.z, 50);

    // Nothing at all within reach means the caller has to build its own floor.
    CHECK(!travel.findSafeSpot(DimensionId::Overworld, {900, 60, 900}, 3, 4, spot));
}

VF_TEST(carvePlatformLaysAFloorAndClearsHeadroom) {
    DimensionSet dimensions(19u);
    DimensionTravel travel(dimensions);

    for (int gy = 40; gy < 50; ++gy)
        for (int dx = -4; dx <= 4; ++dx)
            for (int dz = -4; dz <= 4; ++dz)
                dimensions.end().world().setBlock(dx, gy, dz, Block::EndStone);

    travel.carvePlatform(DimensionId::End, {0, 45, 0}, 2, 4);
    const World& world = dimensions.end().world();

    CHECK_EQ(world.blockAt(0, 44, 0), Block::Obsidian);
    CHECK_EQ(world.blockAt(2, 44, -2), Block::Obsidian);
    CHECK_EQ(world.blockAt(0, 45, 0), Block::Air);
    CHECK_EQ(world.blockAt(0, 48, 0), Block::Air);
    CHECK_EQ(world.blockAt(0, 49, 0), Block::EndStone);
    CHECK_EQ(world.blockAt(3, 44, 0), Block::EndStone);
}
