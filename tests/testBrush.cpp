#include "testHarness.h"
#include "vf/Brush.h"
#include "vf/Codec.h"
#include "vf/EditHistory.h"
#include "vf/Selection.h"

#include <algorithm>
#include <vector>

using namespace vf;

namespace {

uint64_t worldFingerprint(const World& world) {
    std::vector<Vec3i> coords;
    for (const auto& entry : world.allChunks())
        if (!entry.second->empty()) coords.push_back(entry.first);
    std::sort(coords.begin(), coords.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });

    uint64_t digest = 1469598103934665603ull;
    for (const Vec3i& coord : coords) {
        int32_t header[3] = {coord.x, coord.y, coord.z};
        digest ^= fnv1a64(reinterpret_cast<const uint8_t*>(header), sizeof(header));
        digest *= 1099511628211ull;
        const auto& blocks = world.chunkAt(coord)->raw();
        digest ^= fnv1a64(reinterpret_cast<const uint8_t*>(blocks.data()), blocks.size());
        digest *= 1099511628211ull;
    }
    return digest;
}

// Bumpy stone terrain with a grass skin and one free-standing pillar, so every
// brush has something real to bite into.
World bumpyWorld() {
    World world;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) {
            int height = 4 + ((x + z) % 3);
            for (int y = 0; y <= height; ++y)
                world.setBlock(x, y, z, y == height ? Block::Grass : Block::Stone);
        }
    for (int y = 5; y <= 9; ++y) world.setBlock(8, y, 8, Block::Stone);
    return world;
}

World flatWorld(int span, int surfaceY) {
    World world;
    for (int z = 0; z < span; ++z)
        for (int x = 0; x < span; ++x)
            for (int y = 0; y <= surfaceY; ++y) world.setBlock(x, y, z, Block::Stone);
    return world;
}

World stoneCube(const Vec3i& lo, int span) {
    World world;
    for (int y = 0; y < span; ++y)
        for (int z = 0; z < span; ++z)
            for (int x = 0; x < span; ++x)
                world.setBlock(lo.x + x, lo.y + y, lo.z + z, Block::Stone);
    return world;
}

int surfaceHeight(const World& world, int x, int z, int from) {
    for (int y = from; y >= 0; --y)
        if (!isAir(world.blockAt(x, y, z))) return y;
    return -1;
}

BrushParams baseParams(BrushKind kind) {
    BrushParams params;
    params.kind = kind;
    params.center = {8, 7, 8};
    params.radius = 3;
    params.height = 4;
    params.block = Block::Stone;
    params.secondaryBlock = Block::Dirt;
    params.freeNeighbourThreshold = 2;
    params.seed = 7u;
    return params;
}

} // namespace

VF_TEST(everyBrushKindWritesExactlyOneUndoableBatch) {
    const BrushKind kinds[8] = {BrushKind::Sphere,  BrushKind::Cylinder,   BrushKind::Replace,
                                BrushKind::Smooth,  BrushKind::Erode,      BrushKind::Overlay,
                                BrushKind::Naturalize, BrushKind::Gradient};

    for (BrushKind kind : kinds) {
        World world = bumpyWorld();
        uint64_t before = worldFingerprint(world);
        EditHistory history(1u << 22);
        BrushStroke stroke(world, history);

        BrushResult outcome = stroke.apply(baseParams(kind), 1u);
        CHECK(outcome.recorded);
        CHECK(outcome.changed > 0u);
        CHECK_EQ(history.stats().batchCount, 1u);
        CHECK(worldFingerprint(world) != before);
        CHECK(history.peekUndo()->label == std::string(BrushStroke::kindName(kind)));

        CHECK(history.undo(world));
        CHECK_EQ(worldFingerprint(world), before);
        CHECK_EQ(history.undoDepth(), 0u);
        CHECK(stroke.lastResult().changed == outcome.changed);
    }
}

VF_TEST(theSphereBrushFillsHollowsAndShellsTheSameVolume) {
    Selection ball = Selection::sphere({16, 16, 16}, 4);
    Selection core = Selection::sphere({16, 16, 16}, 3);

    World world;
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams fill = baseParams(BrushKind::Sphere);
    fill.center = {16, 16, 16};
    fill.radius = 4;
    BrushResult filled = stroke.apply(fill, 1u);
    CHECK_EQ(filled.changed, ball.count());
    CHECK(world.blockAt(16, 16, 16) == Block::Stone);
    CHECK(world.blockAt(16, 21, 16) == Block::Air);

    BrushParams hollow = fill;
    hollow.mode = BrushParams::VolumeMode::Hollow;
    hollow.radius = 3;
    BrushResult emptied = stroke.apply(hollow, 2u);
    CHECK_EQ(emptied.changed, core.count());
    CHECK(world.blockAt(16, 16, 16) == Block::Air);
    CHECK(world.blockAt(20, 16, 16) == Block::Stone);

    World shellWorld;
    EditHistory shellHistory(1u << 22);
    BrushStroke shellStroke(shellWorld, shellHistory);
    BrushParams shell = fill;
    shell.mode = BrushParams::VolumeMode::Shell;
    shell.shellThickness = 1;
    BrushResult carved = shellStroke.apply(shell, 3u);
    CHECK_EQ(carved.changed, ball.count() - core.count());
    CHECK(shellWorld.blockAt(16, 16, 16) == Block::Air);
    CHECK(shellWorld.blockAt(20, 16, 16) == Block::Stone);
}

VF_TEST(theCylinderBrushFillsATubeOfTheRequestedHeight) {
    World world;
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Cylinder);
    params.center = {20, 10, 20};
    params.radius = 3;
    params.height = 5;
    params.block = Block::Planks;

    BrushResult outcome = stroke.apply(params, 1u);
    CHECK_EQ(outcome.changed, Selection::cylinder({20, 10, 20}, 3, 5).count());
    CHECK(world.blockAt(20, 10, 20) == Block::Planks);
    CHECK(world.blockAt(20, 14, 20) == Block::Planks);
    CHECK(world.blockAt(20, 15, 20) == Block::Air);
    CHECK(world.blockAt(20, 9, 20) == Block::Air);
    CHECK(world.blockAt(24, 12, 20) == Block::Air);

    BrushParams shell = params;
    shell.mode = BrushParams::VolumeMode::Shell;
    shell.shellThickness = 1;
    World tubeWorld;
    EditHistory tubeHistory(1u << 22);
    BrushStroke tubeStroke(tubeWorld, tubeHistory);
    BrushResult carved = tubeStroke.apply(shell, 2u);
    CHECK(carved.changed < outcome.changed);
    CHECK(tubeWorld.blockAt(20, 10, 20) == Block::Planks);
    CHECK(tubeWorld.blockAt(20, 11, 20) == Block::Air);
    CHECK(tubeWorld.blockAt(20, 14, 20) == Block::Planks);
    CHECK(tubeWorld.blockAt(23, 12, 20) == Block::Planks);
}

VF_TEST(theReplaceBrushOnlyTouchesTheListedBlockTypes) {
    World world = bumpyWorld();
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Replace);
    params.center = {8, 4, 8};
    params.radius = 4;
    params.block = Block::Sand;
    params.onlyReplace = {Block::Grass};

    Selection region = BrushStroke::regionOf(params);
    std::size_t grassBefore = region.stats(world).countOf(Block::Grass);
    CHECK(grassBefore > 0u);

    BrushResult outcome = stroke.apply(params, 1u);
    CHECK_EQ(outcome.changed, grassBefore);
    CHECK_EQ(region.stats(world).countOf(Block::Grass), 0u);
    CHECK_EQ(region.stats(world).countOf(Block::Sand), grassBefore);
    CHECK(region.stats(world).countOf(Block::Stone) > 0u);

    BrushParams everything = params;
    everything.onlyReplace.clear();
    everything.block = Block::Glass;
    BrushResult sweep = stroke.apply(everything, 2u);
    SelectionStats after = region.stats(world);
    CHECK_EQ(after.countOf(Block::Stone), 0u);
    CHECK_EQ(after.countOf(Block::Sand), 0u);
    CHECK(sweep.changed > 0u);
    CHECK(after.countOf(Block::Air) > 0u);
}

VF_TEST(theSmoothBrushCutsDownASpikeAndFillsAPit) {
    World world = flatWorld(20, 4);
    for (int y = 5; y <= 10; ++y) world.setBlock(8, y, 8, Block::Stone);
    CHECK_EQ(surfaceHeight(world, 8, 8, 12), 10);

    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);
    BrushParams params = baseParams(BrushKind::Smooth);
    params.center = {8, 6, 8};
    params.radius = 4;

    Selection region = Selection::box({4, 0, 4}, {12, 12, 12});
    BrushResult outcome = stroke.applyWithin(params, region, 1u);
    CHECK_EQ(outcome.changed, 6u);
    CHECK_EQ(surfaceHeight(world, 8, 8, 12), 4);
    CHECK_EQ(surfaceHeight(world, 7, 8, 12), 4);
    CHECK(world.blockAt(8, 4, 8) == Block::Stone);

    World pitWorld = flatWorld(20, 4);
    pitWorld.setBlock(8, 4, 8, Block::Air);
    CHECK_EQ(surfaceHeight(pitWorld, 8, 8, 12), 3);
    EditHistory pitHistory(1u << 22);
    BrushStroke pitStroke(pitWorld, pitHistory);
    BrushResult filled = pitStroke.applyWithin(params, region, 2u);
    CHECK_EQ(filled.changed, 1u);
    CHECK(pitWorld.blockAt(8, 4, 8) == Block::Stone);
    CHECK_EQ(surfaceHeight(pitWorld, 8, 8, 12), 4);
}

VF_TEST(theErodeBrushRemovesOnlyBlocksWithEnoughFreeNeighbours) {
    World world = stoneCube({10, 10, 10}, 3);
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Erode);
    params.center = {11, 11, 11};
    params.radius = 2;
    params.freeNeighbourThreshold = 3;

    BrushResult outcome = stroke.apply(params, 1u);
    // Only the eight corners of a 3x3x3 cube see three empty faces.
    CHECK_EQ(outcome.changed, 8u);
    CHECK(world.blockAt(10, 10, 10) == Block::Air);
    CHECK(world.blockAt(12, 12, 12) == Block::Air);
    CHECK(world.blockAt(11, 10, 10) == Block::Stone);
    CHECK(world.blockAt(11, 11, 11) == Block::Stone);

    CHECK(history.undo(world));
    CHECK(world.blockAt(10, 10, 10) == Block::Stone);

    BrushParams gentler = params;
    gentler.freeNeighbourThreshold = 4;
    BrushResult none = stroke.apply(gentler, 2u);
    CHECK_EQ(none.changed, 0u);
    CHECK(!none.recorded);
}

VF_TEST(theOverlayBrushDecidesFromTheStateBeforeTheStroke) {
    World world = flatWorld(5, 4);
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Overlay);
    params.block = Block::Snow;
    params.freeNeighbourThreshold = 1;
    Selection layer = Selection::box({0, 5, 0}, {4, 5, 4});

    BrushResult outcome = stroke.applyWithin(params, layer, 1u);
    CHECK_EQ(outcome.changed, 25u);
    CHECK(world.blockAt(0, 5, 0) == Block::Snow);
    CHECK(world.blockAt(2, 5, 2) == Block::Snow);
    CHECK(world.blockAt(2, 6, 2) == Block::Air);

    // A wall along x = 0 gives the column next to it two solid neighbours, and
    // nothing else in the layer has more than one. A brush that wrote as it
    // walked would let that first column qualify its neighbour, and the whole
    // row would fall over like dominoes.
    World walled = flatWorld(5, 4);
    for (int z = 0; z < 5; ++z) walled.setBlock(0, 5, z, Block::Stone);
    EditHistory walledHistory(1u << 22);
    BrushStroke walledStroke(walled, walledHistory);
    BrushParams strict = params;
    strict.freeNeighbourThreshold = 2;
    Selection openLayer = Selection::box({1, 5, 0}, {4, 5, 4});

    BrushResult edged = walledStroke.applyWithin(strict, openLayer, 1u);
    CHECK_EQ(edged.changed, 5u);
    for (int z = 0; z < 5; ++z) {
        CHECK(walled.blockAt(1, 5, z) == Block::Snow);
        CHECK(walled.blockAt(2, 5, z) == Block::Air);
        CHECK(walled.blockAt(3, 5, z) == Block::Air);
        CHECK(walled.blockAt(4, 5, z) == Block::Air);
    }

    // The same layer with no wall to lean on stays untouched at that threshold.
    World bare = flatWorld(5, 4);
    EditHistory bareHistory(1u << 22);
    BrushStroke bareStroke(bare, bareHistory);
    BrushResult quiet = bareStroke.applyWithin(strict, layer, 1u);
    CHECK_EQ(quiet.changed, 0u);
    CHECK_EQ(bareHistory.stats().batchCount, 0u);
}

VF_TEST(theNaturalizeBrushLaysGrassOverSoilOverStone) {
    World world;
    for (int y = 0; y <= 6; ++y) world.setBlock(3, y, 3, Block::Cobblestone);
    for (int y = 10; y <= 12; ++y) world.setBlock(3, y, 3, Block::Cobblestone);

    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);
    BrushParams params = baseParams(BrushKind::Naturalize);
    params.soilDepth = 2;

    Selection column = Selection::box({3, 0, 3}, {3, 12, 3});
    BrushResult outcome = stroke.applyWithin(params, column, 1u);
    CHECK(outcome.changed > 0u);

    CHECK(world.blockAt(3, 12, 3) == Block::Grass);
    CHECK(world.blockAt(3, 11, 3) == Block::Dirt);
    CHECK(world.blockAt(3, 10, 3) == Block::Dirt);
    CHECK(world.blockAt(3, 9, 3) == Block::Air);
    // The lower stack is a separate surface and gets its own grass skin.
    CHECK(world.blockAt(3, 6, 3) == Block::Grass);
    CHECK(world.blockAt(3, 5, 3) == Block::Dirt);
    CHECK(world.blockAt(3, 4, 3) == Block::Dirt);
    CHECK(world.blockAt(3, 3, 3) == Block::Stone);
    CHECK(world.blockAt(3, 0, 3) == Block::Stone);

    CHECK(history.undo(world));
    CHECK(world.blockAt(3, 12, 3) == Block::Cobblestone);
    CHECK(world.blockAt(3, 0, 3) == Block::Cobblestone);
}

VF_TEST(theGradientBrushRepeatsExactlyForTheSameSeed) {
    BrushParams params = baseParams(BrushKind::Gradient);
    params.center = {20, 20, 20};
    params.radius = 6;
    params.block = Block::Stone;
    params.secondaryBlock = Block::Sand;
    params.seed = 4242u;

    World first;
    EditHistory firstHistory(1u << 22);
    BrushStroke firstStroke(first, firstHistory);
    BrushResult outcome = firstStroke.apply(params, 1u);
    CHECK(outcome.changed > 0u);

    World second;
    EditHistory secondHistory(1u << 22);
    BrushStroke secondStroke(second, secondHistory);
    secondStroke.apply(params, 99u);
    CHECK_EQ(worldFingerprint(first), worldFingerprint(second));

    World third;
    EditHistory thirdHistory(1u << 22);
    BrushStroke thirdStroke(third, thirdHistory);
    BrushParams other = params;
    other.seed = 4243u;
    thirdStroke.apply(other, 1u);
    CHECK(worldFingerprint(third) != worldFingerprint(first));

    // The mix follows the distance: the centre is always the primary block and
    // the outer shell is mostly the secondary one.
    CHECK(first.blockAt(20, 20, 20) == Block::Stone);
    Selection ball = Selection::sphere({20, 20, 20}, 6);
    Selection inner = Selection::sphere({20, 20, 20}, 2);
    SelectionStats innerStats = inner.stats(first);
    SelectionStats shellStats = ball.differenceWith(Selection::sphere({20, 20, 20}, 5)).stats(first);
    CHECK(innerStats.countOf(Block::Stone) > innerStats.countOf(Block::Sand));
    CHECK(shellStats.countOf(Block::Sand) > shellStats.countOf(Block::Stone));
    CHECK_EQ(innerStats.countOf(Block::Air), 0u);
}

VF_TEST(aBrushRestrictedToASelectionNeverPaintsOutsideIt) {
    World world;
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Sphere);
    params.center = {30, 30, 30};
    params.radius = 5;
    params.block = Block::Bricks;

    Selection half = Selection::box({30, 25, 25}, {35, 35, 35});
    BrushResult outcome = stroke.applyWithin(params, half, 1u);
    CHECK(outcome.changed > 0u);
    CHECK_EQ(outcome.changed, BrushStroke::regionOf(params).intersectWith(half).count());

    CHECK(world.blockAt(30, 30, 30) == Block::Bricks);
    CHECK(world.blockAt(29, 30, 30) == Block::Air);
    CHECK(world.blockAt(26, 30, 30) == Block::Air);

    const EditBatch* batch = history.peekUndo();
    for (const EditOp& op : batch->ops) CHECK(half.contains(op.position));
}

VF_TEST(aStrokeReportsWhenItReachesBeyondTheLoadedWorld) {
    World loaded;
    for (int y = 0; y < 16; ++y)
        for (int z = 0; z < 16; ++z)
            for (int x = 0; x < 16; ++x) loaded.setBlock(x, y, z, Block::Stone);
    CHECK_EQ(loaded.chunkCount(), 1u);

    EditHistory history(1u << 22);
    BrushStroke inside(loaded, history);
    BrushParams params = baseParams(BrushKind::Sphere);
    params.center = {8, 8, 8};
    params.radius = 3;
    params.block = Block::Glass;
    BrushResult contained = inside.apply(params, 1u);
    CHECK(!contained.outsideLoadedWorld);
    CHECK(contained.changed > 0u);

    BrushParams overhang = params;
    overhang.center = {14, 8, 8};
    overhang.radius = 4;
    BrushResult spilled = inside.apply(overhang, 2u);
    CHECK(spilled.outsideLoadedWorld);
    CHECK(spilled.changed > 0u);
    CHECK(loaded.chunkCount() > 1u);
}

VF_TEST(aStrokeIsClippedAtTheWorldHeightLimits) {
    World world;
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams params = baseParams(BrushKind::Sphere);
    params.center = {8, 2, 8};
    params.radius = 4;
    params.block = Block::Stone;
    params.worldMinY = 0;
    params.worldMaxY = 5;

    BrushResult outcome = stroke.apply(params, 1u);
    CHECK(outcome.clippedAtHeightLimit);
    CHECK(outcome.changed > 0u);
    CHECK(outcome.visited > outcome.changed);
    for (int x = 4; x <= 12; ++x) {
        CHECK(world.blockAt(x, -1, 8) == Block::Air);
        CHECK(world.blockAt(x, 5, 8) == Block::Air);
    }
    CHECK(world.blockAt(8, 0, 8) == Block::Stone);
    CHECK(world.blockAt(8, 4, 8) == Block::Stone);
}

VF_TEST(consecutiveStrokesUndoInReverseOrder) {
    World world = bumpyWorld();
    uint64_t start = worldFingerprint(world);
    EditHistory history(1u << 22);
    BrushStroke stroke(world, history);

    BrushParams first = baseParams(BrushKind::Sphere);
    first.center = {4, 10, 4};
    first.block = Block::Wood;
    stroke.apply(first, 1u);
    uint64_t afterFirst = worldFingerprint(world);

    BrushParams second = baseParams(BrushKind::Cylinder);
    second.center = {12, 10, 12};
    second.block = Block::Leaves;
    stroke.apply(second, 2u);
    uint64_t afterSecond = worldFingerprint(world);

    CHECK_EQ(history.stats().batchCount, 2u);
    CHECK(afterFirst != start);
    CHECK(afterSecond != afterFirst);

    CHECK(history.undo(world));
    CHECK_EQ(worldFingerprint(world), afterFirst);
    CHECK(history.undo(world));
    CHECK_EQ(worldFingerprint(world), start);
    CHECK(history.redo(world));
    CHECK_EQ(worldFingerprint(world), afterFirst);
}
