#include "testHarness.h"
#include "vf/Portal.h"

#include <cstdlib>

using namespace vf;

namespace {

PortalFrame layFrame(World& world, const Vec3i& origin, int width, int height, bool alongX) {
    PortalFrame frame;
    frame.origin = origin;
    frame.width = width;
    frame.height = height;
    frame.alongX = alongX;

    for (int across = -1; across <= width; ++across) {
        for (int up = -1; up <= height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            bool ring = across == -1 || across == width || up == -1 || up == height;
            world.setBlock(cell.x, cell.y, cell.z, ring ? Block::Obsidian : Block::Air);
        }
    }
    return frame;
}

void chargeTraveller(PortalNetwork& network, int entityId) {
    for (int tick = 0; tick < network.requiredStandingTicks(); ++tick)
        network.tickEntity(entityId, true);
}

int countInterior(const World& world, const PortalFrame& frame, Block block) {
    int total = 0;
    for (int across = 0; across < frame.width; ++across)
        for (int up = 0; up < frame.height; ++up) {
            Vec3i cell = frame.interiorBlock(across, up);
            if (world.blockAt(cell.x, cell.y, cell.z) == block) ++total;
        }
    return total;
}

}

VF_TEST(portalScanFindsTheSmallestLegalObsidianFrame) {
    World world;
    layFrame(world, {4, 40, 4}, 2, 3, true);

    PortalScan scan = PortalScan::detect(world, {5, 41, 4}, DimensionId::Overworld);
    CHECK(scan.found);
    CHECK_EQ(scan.frame.width, 2);
    CHECK_EQ(scan.frame.height, 3);
    CHECK(scan.frame.alongX);
    CHECK_EQ(scan.frame.origin.x, 4);
    CHECK_EQ(scan.frame.origin.y, 40);
    CHECK_EQ(scan.frame.origin.z, 4);
    CHECK_EQ(scan.interiorArea, 6);
    CHECK_EQ(static_cast<int>(scan.frame.dimension), static_cast<int>(DimensionId::Overworld));
}

VF_TEST(portalScanReadsFramesStandingInTheZPlane) {
    World world;
    layFrame(world, {-3, 20, 7}, 3, 4, false);

    PortalScan scan = PortalScan::detect(world, {-3, 22, 8}, DimensionId::Nether);
    CHECK(scan.found);
    CHECK(!scan.frame.alongX);
    CHECK_EQ(scan.frame.width, 3);
    CHECK_EQ(scan.frame.height, 4);
    CHECK_EQ(scan.frame.origin.z, 7);
    CHECK_EQ(scan.frame.origin.x, -3);
    CHECK_EQ(static_cast<int>(scan.frame.dimension), static_cast<int>(DimensionId::Nether));
}

VF_TEST(portalScanAcceptsAWideFrameUpToTheLimit) {
    World world;
    layFrame(world, {0, 30, 0}, 21, 21, true);
    PortalScan wide = PortalScan::detect(world, {10, 40, 0}, DimensionId::Overworld);
    CHECK(wide.found);
    CHECK_EQ(wide.frame.width, 21);
    CHECK_EQ(wide.frame.height, 21);

    World oversized;
    layFrame(oversized, {0, 30, 0}, 22, 3, true);
    CHECK(!PortalScan::detect(oversized, {10, 31, 0}, DimensionId::Overworld).found);
}

VF_TEST(portalScanRejectsFramesUnderTheMinimumSize) {
    World narrow;
    layFrame(narrow, {0, 30, 0}, 1, 3, true);
    CHECK(!PortalScan::detect(narrow, {0, 31, 0}, DimensionId::Overworld).found);

    World squat;
    layFrame(squat, {0, 30, 0}, 2, 2, true);
    CHECK(!PortalScan::detect(squat, {0, 31, 0}, DimensionId::Overworld).found);
}

VF_TEST(portalScanIgnoresMissingCornersButNotMissingSides) {
    World world;
    PortalFrame frame = layFrame(world, {4, 40, 4}, 2, 3, true);

    Vec3i corner = frame.interiorBlock(-1, -1);
    world.setBlock(corner.x, corner.y, corner.z, Block::Air);
    CHECK(PortalScan::detect(world, {5, 41, 4}, DimensionId::Overworld).found);

    Vec3i side = frame.leftColumn(1);
    world.setBlock(side.x, side.y, side.z, Block::Air);
    CHECK(!PortalScan::detect(world, {5, 41, 4}, DimensionId::Overworld).found);
}

VF_TEST(portalScanRefusesAFrameWithSomethingStuckInTheOpening) {
    World world;
    layFrame(world, {4, 40, 4}, 2, 3, true);
    world.setBlock(5, 41, 4, Block::Stone);

    CHECK(!PortalScan::detect(world, {4, 40, 4}, DimensionId::Overworld).found);
}

VF_TEST(lightingAFrameFillsExactlyItsOpening) {
    World world;
    PortalFrame frame = layFrame(world, {4, 40, 4}, 3, 4, true);

    CHECK_EQ(PortalScan::light(world, frame), 12);
    CHECK_EQ(countInterior(world, frame, Block::NetherPortal), 12);
    CHECK_EQ(PortalScan::light(world, frame), 0);

    Vec3i outside = frame.interiorBlock(0, 0);
    CHECK_EQ(world.blockAt(outside.x, outside.y, outside.z - 1), Block::Air);
    Vec3i ringBlock = frame.bottomRow(0);
    CHECK_EQ(world.blockAt(ringBlock.x, ringBlock.y, ringBlock.z), Block::Obsidian);

    CHECK_EQ(PortalScan::douse(world, frame), 12);
    CHECK_EQ(countInterior(world, frame, Block::Air), 12);
}

VF_TEST(breakingTheFrameUnmakesTheLitPortal) {
    DimensionSet dimensions(77u);
    PortalNetwork network(dimensions);
    World& world = dimensions.overworld().world();

    PortalFrame frame = layFrame(world, {4, 40, 4}, 2, 3, true);
    int id = network.ignite(DimensionId::Overworld, {4, 41, 4});
    CHECK(id >= 0);
    CHECK_EQ(countInterior(world, frame, Block::NetherPortal), 6);
    CHECK_EQ(network.activeCountIn(DimensionId::Overworld), 1);

    Vec3i corner = frame.interiorBlock(-1, 3);
    world.setBlock(corner.x, corner.y, corner.z, Block::Air);
    CHECK_EQ(network.validate(DimensionId::Overworld), 0);
    CHECK_EQ(countInterior(world, frame, Block::NetherPortal), 6);

    Vec3i side = frame.topRow(1);
    world.setBlock(side.x, side.y, side.z, Block::Air);
    CHECK_EQ(network.validate(DimensionId::Overworld), 1);
    CHECK_EQ(countInterior(world, frame, Block::NetherPortal), 0);
    CHECK_EQ(network.activeCountIn(DimensionId::Overworld), 0);
    CHECK(!network.portal(id)->active);
}

VF_TEST(ignitingTheSameFrameTwiceReusesItsRegistration) {
    DimensionSet dimensions(78u);
    PortalNetwork network(dimensions);
    World& world = dimensions.overworld().world();
    layFrame(world, {4, 40, 4}, 2, 3, true);

    int first = network.ignite(DimensionId::Overworld, {4, 41, 4});
    int second = network.ignite(DimensionId::Overworld, {5, 42, 4});
    CHECK_EQ(first, second);
    CHECK_EQ(network.links().size(), static_cast<std::size_t>(1));

    CHECK_EQ(network.ignite(DimensionId::Overworld, {40, 40, 40}), -1);
}

VF_TEST(travellersChargeInThePortalAndThenSitOutACooldown) {
    DimensionSet dimensions(80u);
    PortalNetwork network(dimensions);
    World& world = dimensions.overworld().world();
    layFrame(world, {4, 40, 4}, 2, 3, true);
    int source = network.ignite(DimensionId::Overworld, {4, 41, 4});

    for (int tick = 0; tick < network.requiredStandingTicks() - 1; ++tick)
        network.tickEntity(1, true);
    CHECK_EQ(network.standingTicks(1), network.requiredStandingTicks() - 1);
    CHECK(!network.readyToTravel(1));

    PortalSearch tooEarly = network.travel(1, source, DimensionId::Nether);
    CHECK(!tooEarly.found);
    CHECK(tooEarly.blocked);

    network.tickEntity(1, true);
    CHECK(network.readyToTravel(1));

    PortalSearch crossing = network.travel(1, source, DimensionId::Nether);
    CHECK(crossing.found);
    CHECK_EQ(network.cooldownTicks(1), network.cooldownLength());
    CHECK_EQ(network.standingTicks(1), 0);

    PortalSearch immediate = network.travel(1, source, DimensionId::Nether);
    CHECK(immediate.blocked);

    for (int tick = 0; tick < network.cooldownLength(); ++tick) network.tickEntity(1, false);
    CHECK_EQ(network.cooldownTicks(1), 0);
    CHECK_EQ(network.standingTicks(1), 0);
    CHECK(!network.readyToTravel(1));
}

VF_TEST(crossingToAnEmptyNetherRaisesANewPortalThere) {
    DimensionSet dimensions(4242u);
    PortalNetwork network(dimensions);
    dimensions.overworld().generateArea(0, 0, 1);
    layFrame(dimensions.overworld().world(), {800, 40, 400}, 2, 3, true);

    int source = network.ignite(DimensionId::Overworld, {800, 41, 400});
    CHECK(source >= 0);
    chargeTraveller(network, 3);

    PortalSearch crossing = network.travel(3, source, DimensionId::Nether);
    CHECK(crossing.found);
    CHECK(crossing.built);
    CHECK_EQ(crossing.plan.scaledTarget.x, 100);
    CHECK_EQ(crossing.plan.scaledTarget.z, 50);
    CHECK(crossing.plan.needsPortal);
    CHECK_EQ(network.activeCountIn(DimensionId::Nether), 1);

    const PortalLink* raised = network.portal(crossing.portalId);
    CHECK_EQ(static_cast<int>(raised->dimension), static_cast<int>(DimensionId::Nether));
    CHECK(PortalScan::intact(dimensions.nether().world(), raised->frame));
    CHECK_EQ(countInterior(dimensions.nether().world(), raised->frame, Block::NetherPortal), 6);

    CHECK(std::abs(crossing.position.x - 100) <= DimensionTravel::buildRadiusFor(DimensionId::Nether));
    CHECK(std::abs(crossing.position.z - 50) <= DimensionTravel::buildRadiusFor(DimensionId::Nether));

    CHECK_EQ(network.portal(source)->linkedId, crossing.portalId);
    CHECK_EQ(raised->linkedId, source);
}

VF_TEST(crossingReusesAPortalThatIsAlreadyOnTheFarSide) {
    DimensionSet dimensions(4242u);
    PortalNetwork network(dimensions);
    dimensions.overworld().generateArea(50, 25, 1);
    layFrame(dimensions.overworld().world(), {800, 40, 400}, 2, 3, true);
    int source = network.ignite(DimensionId::Overworld, {800, 41, 400});

    PortalFrame waiting;
    waiting.origin = {104, 44, 52};
    waiting.width = 2;
    waiting.height = 3;
    waiting.alongX = true;
    PortalScan::buildFrame(dimensions.nether().world(), waiting);
    PortalScan::light(dimensions.nether().world(), waiting);
    int existing = network.registerPortal(DimensionId::Nether, waiting);

    chargeTraveller(network, 5);
    PortalSearch crossing = network.travel(5, source, DimensionId::Nether);
    CHECK(crossing.found);
    CHECK(!crossing.built);
    CHECK_EQ(crossing.portalId, existing);
    CHECK_EQ(network.activeCountIn(DimensionId::Nether), 1);
    CHECK_EQ(crossing.position.x, waiting.entry().x);
    CHECK_EQ(crossing.position.y, waiting.entry().y);
    CHECK_EQ(crossing.position.z, waiting.entry().z);
}

VF_TEST(findNearestOnlyReportsLivePortalsInRange) {
    DimensionSet dimensions(91u);
    PortalNetwork network(dimensions);
    World& world = dimensions.overworld().world();

    layFrame(world, {0, 40, 0}, 2, 3, true);
    layFrame(world, {60, 40, 0}, 2, 3, true);
    int close = network.ignite(DimensionId::Overworld, {0, 41, 0});
    int far = network.ignite(DimensionId::Overworld, {60, 41, 0});
    CHECK(close != far);

    PortalSearch found = network.findNearest(DimensionId::Overworld, {3, 41, 0}, 16);
    CHECK(found.found);
    CHECK_EQ(found.portalId, close);

    CHECK(!network.findNearest(DimensionId::Nether, {0, 41, 0}, 128).found);
    CHECK(!network.findNearest(DimensionId::Overworld, {300, 41, 0}, 16).found);

    PortalSearch reach = network.findNearest(DimensionId::Overworld, {30, 41, 0}, 64);
    CHECK(reach.found);
}

VF_TEST(relinkingAPortalDropsItsPreviousPartner) {
    DimensionSet dimensions(93u);
    PortalNetwork network(dimensions);
    World& world = dimensions.overworld().world();

    layFrame(world, {0, 40, 0}, 2, 3, true);
    layFrame(world, {20, 40, 0}, 2, 3, true);
    layFrame(world, {40, 40, 0}, 2, 3, true);
    int first = network.ignite(DimensionId::Overworld, {0, 41, 0});
    int second = network.ignite(DimensionId::Overworld, {20, 41, 0});
    int third = network.ignite(DimensionId::Overworld, {40, 41, 0});

    network.linkPortals(first, second);
    CHECK_EQ(network.portal(first)->linkedId, second);
    CHECK_EQ(network.portal(second)->linkedId, first);

    network.linkPortals(first, third);
    CHECK_EQ(network.portal(first)->linkedId, third);
    CHECK_EQ(network.portal(third)->linkedId, first);
    CHECK_EQ(network.portal(second)->linkedId, -1);
}

VF_TEST(builtFrameCarvesItsOwnApproachOutOfTheTerrain) {
    DimensionSet dimensions(95u);
    World& world = dimensions.nether().world();

    for (int gx = 0; gx < 10; ++gx)
        for (int gy = 40; gy < 50; ++gy)
            for (int gz = 0; gz < 10; ++gz)
                world.setBlock(gx, gy, gz, Block::Netherrack);

    PortalFrame frame;
    frame.origin = {4, 42, 5};
    frame.width = 2;
    frame.height = 3;
    frame.alongX = true;
    frame.dimension = DimensionId::Nether;

    CHECK(PortalScan::siteUsable(world, frame));
    PortalScan::buildFrame(world, frame);

    CHECK(PortalScan::intact(world, frame));
    CHECK(PortalScan::interiorClear(world, frame));
    CHECK_EQ(world.blockAt(4, 42, 4), Block::Air);
    CHECK_EQ(world.blockAt(5, 42, 6), Block::Air);
    CHECK_EQ(world.blockAt(4, 41, 4), Block::Obsidian);
    CHECK_EQ(world.blockAt(4, 41, 6), Block::Obsidian);
    CHECK_EQ(world.blockAt(0, 42, 0), Block::Netherrack);
    CHECK_EQ(PortalScan::light(world, frame), 6);
}

VF_TEST(bedrockMakesASiteUnusable) {
    World world;
    PortalFrame frame;
    frame.origin = {0, 2, 0};
    frame.width = 2;
    frame.height = 3;
    frame.alongX = true;

    CHECK(PortalScan::siteUsable(world, frame));
    world.setBlock(0, 1, 0, Block::Bedrock);
    CHECK(!PortalScan::siteUsable(world, frame));
}

VF_TEST(endPortalIgnitesOnlyOnceEveryEyeIsSeated) {
    DimensionSet dimensions(121u);
    PortalNetwork network(dimensions);
    Vec3i center{40, 60, 40};

    CHECK_EQ(network.buildEndFrame(DimensionId::End, center), PortalNetwork::endFrameSlots);
    const World& world = dimensions.end().world();
    CHECK_EQ(world.blockAt(38, 60, 40), Block::EndPortalFrame);
    CHECK_EQ(world.blockAt(40, 60, 42), Block::EndPortalFrame);
    CHECK_EQ(world.blockAt(38, 60, 38), Block::Air);
    CHECK_EQ(world.blockAt(42, 60, 42), Block::Air);
    CHECK_EQ(world.blockAt(40, 60, 40), Block::Air);

    const int offsetX[PortalNetwork::endFrameSlots] = {-2, -2, -2, 2, 2, 2, -1, 0, 1, -1, 0, 1};
    const int offsetZ[PortalNetwork::endFrameSlots] = {-1, 0, 1, -1, 0, 1, -2, -2, -2, 2, 2, 2};

    for (int slot = 0; slot < PortalNetwork::endFrameSlots - 1; ++slot) {
        Vec3i socket{center.x + offsetX[slot], center.y, center.z + offsetZ[slot]};
        CHECK(network.insertEndEye(DimensionId::End, socket));
        CHECK(network.hasEndEye(DimensionId::End, socket));
        CHECK_EQ(network.endEyeCount(DimensionId::End, center), slot + 1);
        CHECK(!network.endFrameComplete(DimensionId::End, center));
        CHECK_EQ(world.blockAt(center.x, center.y, center.z), Block::Air);
    }

    Vec3i last{center.x + offsetX[11], center.y, center.z + offsetZ[11]};
    CHECK(network.insertEndEye(DimensionId::End, last));
    CHECK(network.endFrameComplete(DimensionId::End, center));

    int portalCells = 0;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            if (world.blockAt(center.x + dx, center.y, center.z + dz) == Block::EndPortal)
                ++portalCells;
    CHECK_EQ(portalCells, 9);
}

VF_TEST(endPortalEyesOnlyGoIntoRealFrameBlocks) {
    DimensionSet dimensions(122u);
    PortalNetwork network(dimensions);
    Vec3i center{0, 50, 0};
    network.buildEndFrame(DimensionId::End, center);

    Vec3i socket{-2, 50, 0};
    CHECK(network.insertEndEye(DimensionId::End, socket));
    CHECK(!network.insertEndEye(DimensionId::End, socket));
    CHECK(!network.insertEndEye(DimensionId::End, {-2, 50, -2}));
    CHECK(!network.insertEndEye(DimensionId::End, {9, 9, 9}));
    CHECK_EQ(network.endEyeCount(DimensionId::End, center), 1);

    CHECK(!network.hasEndEye(DimensionId::Overworld, socket));
    CHECK(!network.activateEndPortal(DimensionId::End, center));
}
