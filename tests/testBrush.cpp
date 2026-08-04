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

