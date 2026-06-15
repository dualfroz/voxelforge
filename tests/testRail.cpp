#include "testHarness.h"
#include "vf/Rail.h"

using namespace vf;

namespace {

void placeRail(World& world, int x, int y, int z, Block kind = Block::Rail) {
    world.setBlock(x, y, z, kind);
}

} // namespace

VF_TEST(straightRailInferredFromOppositeNeighbours) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, 0, 0, 1);
    placeRail(world, 0, 0, -1);
    CHECK(inferRailShape(world, {0, 0, 0}) == RailShape::NorthSouth);

    World ew;
    placeRail(ew, 0, 0, 0);
    placeRail(ew, 1, 0, 0);
    placeRail(ew, -1, 0, 0);
    CHECK(inferRailShape(ew, {0, 0, 0}) == RailShape::EastWest);
}

VF_TEST(loneRailDefaultsToNorthSouth) {
    World world;
    placeRail(world, 0, 0, 0);
    CHECK(inferRailShape(world, {0, 0, 0}) == RailShape::NorthSouth);
}

VF_TEST(plainRailCurvesBetweenPerpendicularNeighbours) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, 0, 0, 1);   // south
    placeRail(world, 1, 0, 0);   // east
    CHECK(inferRailShape(world, {0, 0, 0}) == RailShape::CurveSouthEast);
}

VF_TEST(poweredRailNeverCurves) {
    World world;
    placeRail(world, 0, 0, 0, Block::PoweredRail);
    placeRail(world, 0, 0, 1);   // south
    placeRail(world, 1, 0, 0);   // east
    RailShape shape = inferRailShape(world, {0, 0, 0});
    CHECK(shape == RailShape::NorthSouth || shape == RailShape::EastWest);
}

VF_TEST(ascendingShapeInferredFromRaisedNeighbour) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, -1, 0, 0);   // west, flat
    placeRail(world, 1, 1, 0);    // east, raised
    CHECK(inferRailShape(world, {0, 0, 0}) == RailShape::AscendEast);
}

VF_TEST(advanceFollowsStraightTrack) {
    World world;
    for (int x = 0; x <= 3; ++x) placeRail(world, x, 0, 0);
    RailStep step = advanceOnRail(world, {1, 0, 0}, RailDir::East);
    CHECK(step.valid);
    CHECK(step.nextPos == Vec3i(2, 0, 0));
    CHECK(step.exitDir == RailDir::East);
}

VF_TEST(advanceTurnsThroughCurve) {
    World world;
    placeRail(world, 0, 0, 0);   // curve SE
    placeRail(world, 0, 0, 1);   // south neighbour
    placeRail(world, 1, 0, 0);   // east neighbour
    RailStep step = advanceOnRail(world, {0, 0, 0}, RailDir::North);
    CHECK(step.valid);
    CHECK(step.exitDir == RailDir::East);
    CHECK(step.nextPos == Vec3i(1, 0, 0));
}

VF_TEST(advanceClimbsAndDescendsSlopes) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, -1, 0, 0);
    placeRail(world, 1, 1, 0);

    RailStep up = advanceOnRail(world, {0, 0, 0}, RailDir::East);
    CHECK(up.valid);
    CHECK(up.nextPos == Vec3i(1, 1, 0));

    RailStep down = advanceOnRail(world, {1, 1, 0}, RailDir::West);
    CHECK(down.valid);
    CHECK(down.nextPos == Vec3i(0, 0, 0));
}

VF_TEST(advanceDerailsAtTrackEnd) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, 1, 0, 0);
    RailStep step = advanceOnRail(world, {1, 0, 0}, RailDir::East);
    CHECK(!step.valid);
}

VF_TEST(switchedJunctionTakesTheTurn) {
    World world;
    placeRail(world, 0, 0, 0);
    placeRail(world, 0, 0, -1);   // north
    placeRail(world, 0, 0, 1);    // south
    placeRail(world, 1, 0, 0);    // east branch
    CHECK(inferRailShape(world, {0, 0, 0}, false) == RailShape::NorthSouth);
    CHECK(inferRailShape(world, {0, 0, 0}, true) == RailShape::CurveSouthEast);
}

VF_TEST(poweredRailPropagatesWithReachFalloff) {
    World world;
    std::vector<Vec3i> rails;
    for (int x = 0; x <= 12; ++x) {
        world.setBlock(x, 0, 0, Block::PoweredRail);
        rails.push_back({x, 0, 0});
    }

    auto active = activePoweredRails(world, rails, [](const Vec3i& p) {
        return p == Vec3i(0, 0, 0);
    });

    CHECK(active.count({0, 0, 0}) == 1);
    CHECK(active.count({8, 0, 0}) == 1);
    CHECK(active.count({9, 0, 0}) == 0);
    CHECK(active.count({12, 0, 0}) == 0);
}

VF_TEST(poweredRailPropagatesBothDirectionsFromMiddleSource) {
    World world;
    std::vector<Vec3i> rails;
    for (int x = -10; x <= 10; ++x) {
        world.setBlock(x, 0, 0, Block::PoweredRail);
        rails.push_back({x, 0, 0});
    }

    auto active = activePoweredRails(world, rails, [](const Vec3i& p) {
        return p == Vec3i(0, 0, 0);
    });

    CHECK(active.count({8, 0, 0}) == 1);
    CHECK(active.count({-8, 0, 0}) == 1);
    CHECK(active.count({9, 0, 0}) == 0);
    CHECK(active.count({-9, 0, 0}) == 0);
}

VF_TEST(unpoweredRailNetworkStaysInactive) {
    World world;
    std::vector<Vec3i> rails;
    for (int x = 0; x <= 5; ++x) {
        world.setBlock(x, 0, 0, Block::PoweredRail);
        rails.push_back({x, 0, 0});
    }
    auto active = activePoweredRails(world, rails, [](const Vec3i&) { return false; });
    CHECK(active.empty());
}

VF_TEST(railGraphMeasuresDistanceAlongLine) {
    World world;
    std::vector<Vec3i> cells;
    for (int x = 0; x <= 5; ++x) {
        placeRail(world, x, 0, 0);
        cells.push_back({x, 0, 0});
    }
    RailGraph graph;
    graph.build(world, cells);

    CHECK_EQ(static_cast<int>(graph.size()), 6);
    CHECK(graph.contains({3, 0, 0}));
    CHECK(!graph.contains({100, 0, 0}));
    CHECK_EQ(graph.distance({0, 0, 0}, {5, 0, 0}), 5);
    CHECK_EQ(graph.distance({0, 0, 0}, {100, 0, 0}), -1);
    CHECK_EQ(static_cast<int>(graph.neighbours({0, 0, 0}).size()), 1);
    CHECK_EQ(static_cast<int>(graph.neighbours({3, 0, 0}).size()), 2);
}

VF_TEST(railGraphTracesAClosedLoop) {
    World world;
    std::vector<Vec3i> cells = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}};
    for (const Vec3i& c : cells) placeRail(world, c.x, c.y, c.z);

    RailGraph graph;
    graph.build(world, cells);

    std::vector<Vec3i> path = graph.trace(world, {0, 0, 0}, RailDir::North, 20);
    CHECK_EQ(static_cast<int>(path.size()), 4);
    CHECK_EQ(graph.distance({0, 0, 0}, {1, 0, 1}), 2);
    CHECK_EQ(static_cast<int>(graph.neighbours({0, 0, 0}).size()), 2);
}

VF_TEST(railGraphTraceStopsAtDeadEnd) {
    World world;
    std::vector<Vec3i> cells;
    for (int x = 0; x <= 3; ++x) {
        placeRail(world, x, 0, 0);
        cells.push_back({x, 0, 0});
    }
    RailGraph graph;
    graph.build(world, cells);

    std::vector<Vec3i> path = graph.trace(world, {0, 0, 0}, RailDir::East, 20);
    CHECK_EQ(static_cast<int>(path.size()), 4);
    CHECK(path.back() == Vec3i(3, 0, 0));
}
