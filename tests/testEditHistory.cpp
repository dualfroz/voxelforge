#include "testHarness.h"
#include "vf/Codec.h"
#include "vf/EditHistory.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace vf;

namespace {

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint32_t next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(state >> 32);
    }
    int below(int limit) { return static_cast<int>(next() % static_cast<uint32_t>(limit)); }
};

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
        const Chunk* chunk = world.chunkAt(coord);
        const auto& blocks = chunk->raw();
        digest ^= fnv1a64(reinterpret_cast<const uint8_t*>(blocks.data()), blocks.size());
        digest *= 1099511628211ull;
    }
    return digest;
}

World layeredWorld(int span, int depth) {
    World world;
    for (int z = 0; z < span; ++z)
        for (int x = 0; x < span; ++x)
            for (int y = 0; y < depth; ++y) {
                Block block = y + 1 == depth ? Block::Grass : Block::Stone;
                world.setBlock(x, y, z, block);
            }
    return world;
}

}

VF_TEST(recordingAnEditCapturesThePreviousBlockAndWritesTheNewOne) {
    World world;
    world.setBlock(1, 2, 3, Block::Stone);
    EditHistory history;

    CHECK(history.beginBatch("place", 1u));
    CHECK(history.batchOpen());
    CHECK(history.recordSet(world, {1, 2, 3}, Block::Planks));
    CHECK(world.blockAt(1, 2, 3) == Block::Planks);
    CHECK_EQ(history.openBatchSize(), 1u);
    CHECK(history.endBatch());
    CHECK(!history.batchOpen());

    const EditBatch* batch = history.peekUndo();
    CHECK(batch != nullptr);
    CHECK(batch->label == std::string("place"));
    CHECK_EQ(batch->ops.size(), 1u);
    CHECK(batch->ops[0].before == Block::Stone);
    CHECK(batch->ops[0].after == Block::Planks);
    CHECK(batch->ops[0].position == Vec3i(1, 2, 3));
}

VF_TEST(editsRecordedWithoutAnOpenBatchAreRejected) {
    World world;
    EditHistory history;
    CHECK(!history.recordSet(world, {0, 0, 0}, Block::Stone));
    CHECK(world.blockAt(0, 0, 0) == Block::Air);
    CHECK_EQ(history.undoDepth(), 0u);
    CHECK(!history.endBatch());

    CHECK(history.beginBatch("first", 0u));
    CHECK(!history.beginBatch("second", 0u));
    CHECK(history.recordSet(world, {0, 0, 0}, Block::Stone));
    CHECK(history.endBatch());
    CHECK_EQ(history.undoDepth(), 1u);
}

VF_TEST(undoTakesBackAWholeBatchAndRedoReplaysIt) {
    World world = layeredWorld(4, 3);
    uint64_t original = worldFingerprint(world);
    EditHistory history;

    history.beginBatch("tower", 10u);
    for (int y = 3; y < 9; ++y) history.recordSet(world, {2, y, 2}, Block::Cobblestone);
    CHECK(history.endBatch());
    uint64_t built = worldFingerprint(world);
    CHECK(built != original);
    CHECK_EQ(history.stats().opCount, 6u);

    CHECK(history.undo(world));
    CHECK_EQ(worldFingerprint(world), original);
    CHECK_EQ(history.undoDepth(), 0u);
    CHECK_EQ(history.redoDepth(), 1u);
    CHECK(!history.undo(world));

    CHECK(history.redo(world));
    CHECK_EQ(worldFingerprint(world), built);
    CHECK_EQ(history.undoDepth(), 1u);
    CHECK(!history.redo(world));
    CHECK_EQ(history.stats().undoCount, 1u);
    CHECK_EQ(history.stats().redoCount, 1u);
}

VF_TEST(undoingARandomSequenceOfTwoHundredEditsRestoresTheWorldByteForByte) {
    World world = layeredWorld(8, 4);
    EditHistory history(1u << 22);
    const Block palette[5] = {Block::Air, Block::Stone, Block::Dirt, Block::Sand, Block::Glass};

    std::vector<uint64_t> marks;
    marks.push_back(worldFingerprint(world));

    Lcg rng(0xfeedfaceull);
    for (int batch = 0; batch < 200; ++batch) {
        history.beginBatch("stroke" + std::to_string(batch), static_cast<uint64_t>(batch));
        int edits = 1 + rng.below(6);
        for (int edit = 0; edit < edits; ++edit) {
            Vec3i cell{rng.below(8), rng.below(6), rng.below(8)};
            history.recordSet(world, cell, palette[rng.below(5)]);
        }
        if (history.endBatch()) marks.push_back(worldFingerprint(world));
    }

    CHECK(marks.size() > 150u);
    CHECK_EQ(history.undoDepth(), marks.size() - 1u);
    CHECK_EQ(history.stats().droppedBatches, 0u);

    for (std::size_t step = marks.size() - 1u; step > 0u; --step) {
        CHECK(history.undo(world));
        CHECK_EQ(worldFingerprint(world), marks[step - 1u]);
    }
    CHECK(!history.undo(world));
    CHECK_EQ(worldFingerprint(world), marks.front());

    for (std::size_t step = 1u; step < marks.size(); ++step) {
        CHECK(history.redo(world));
        CHECK_EQ(worldFingerprint(world), marks[step]);
    }
    CHECK(!history.redo(world));
}

VF_TEST(aNewEditThrowsAwayTheRedoStack) {
    World world;
    EditHistory history;

    history.beginBatch("one", 1u);
    history.recordSet(world, {0, 0, 0}, Block::Stone);
    history.endBatch();
    CHECK(history.undo(world));
    CHECK_EQ(history.redoDepth(), 1u);

    history.beginBatch("two", 2u);
    history.recordSet(world, {5, 0, 0}, Block::Dirt);
    history.endBatch();
    CHECK_EQ(history.redoDepth(), 0u);
    CHECK(!history.redo(world));
    CHECK(world.blockAt(0, 0, 0) == Block::Air);
    CHECK(world.blockAt(5, 0, 0) == Block::Dirt);
}

VF_TEST(theMemoryBudgetDropsTheOldestBatchesFirst) {
    World probe;
    EditHistory sizer;
    sizer.beginBatch("probe", 0u);
    for (int z = 0; z < 10; ++z) sizer.recordSet(probe, {0, 0, z}, Block::Stone);
    sizer.endBatch();
    std::size_t batchBytes = sizer.peekUndo()->byteSize();
    CHECK(batchBytes > 0u);

    World world;
    EditHistory history(batchBytes * 3u);
    for (int batch = 0; batch < 10; ++batch) {
        history.beginBatch("probe", static_cast<uint64_t>(batch));
        for (int z = 0; z < 10; ++z) history.recordSet(world, {batch, 0, z}, Block::Stone);
        CHECK(history.endBatch());
    }

    CHECK_EQ(history.undoDepth(), 3u);
    CHECK_EQ(history.stats().droppedBatches, 7u);
    CHECK(history.stats().bytes <= batchBytes * 3u);

    while (history.undo(world)) {
    }
    for (int batch = 0; batch < 7; ++batch) CHECK(world.blockAt(batch, 0, 5) == Block::Stone);
    for (int batch = 7; batch < 10; ++batch) CHECK(world.blockAt(batch, 0, 5) == Block::Air);
}

VF_TEST(shrinkingTheBudgetLaterAlsoEvictsTheOldestBatches) {
    World world;
    EditHistory history(1u << 20);
    for (int batch = 0; batch < 6; ++batch) {
        history.beginBatch("fill", static_cast<uint64_t>(batch));
        for (int z = 0; z < 4; ++z) history.recordSet(world, {batch, 0, z}, Block::Sand);
        history.endBatch();
    }
    CHECK_EQ(history.undoDepth(), 6u);

    std::size_t oneBatch = history.peekUndo()->byteSize();
    history.setMemoryBudget(oneBatch * 2u);
    CHECK_EQ(history.undoDepth(), 2u);
    CHECK_EQ(history.stats().droppedBatches, 4u);
    CHECK_EQ(history.memoryBudget(), oneBatch * 2u);
}

VF_TEST(batchesTouchingTheSameBlocksCloseInTimeMergeIntoOne) {
    World world;
    EditHistory history;
    history.setMergeWindow(5u);
    CHECK_EQ(history.mergeWindowTicks(), 5u);

    history.beginBatch("dab", 100u);
    history.recordSet(world, {0, 0, 0}, Block::Stone);
    history.endBatch();

    history.beginBatch("dab", 102u);
    history.recordSet(world, {0, 0, 0}, Block::Dirt);
    history.recordSet(world, {1, 0, 0}, Block::Dirt);
    history.endBatch();

    CHECK_EQ(history.undoDepth(), 1u);
    CHECK_EQ(history.stats().mergedBatches, 1u);
    CHECK_EQ(history.stats().opCount, 2u);

    const EditBatch* merged = history.peekUndo();
    CHECK(merged->touches({0, 0, 0}));
    CHECK(merged->touches({1, 0, 0}));
    for (const EditOp& op : merged->ops) {
        if (op.position == Vec3i(0, 0, 0)) {
            CHECK(op.before == Block::Air);
            CHECK(op.after == Block::Dirt);
        }
    }

    CHECK(history.undo(world));
    CHECK(world.blockAt(0, 0, 0) == Block::Air);
    CHECK(world.blockAt(1, 0, 0) == Block::Air);
    CHECK_EQ(history.undoDepth(), 0u);
}

VF_TEST(batchesStaySeparateWhenTheyMissInSpaceOrInTime) {
    World world;
    EditHistory apart;
    apart.setMergeWindow(5u);
    apart.beginBatch("a", 100u);
    apart.recordSet(world, {0, 0, 0}, Block::Stone);
    apart.endBatch();
    apart.beginBatch("b", 101u);
    apart.recordSet(world, {9, 0, 0}, Block::Stone);
    apart.endBatch();
    CHECK_EQ(apart.undoDepth(), 2u);
    CHECK_EQ(apart.stats().mergedBatches, 0u);

    World other;
    EditHistory late;
    late.setMergeWindow(5u);
    late.beginBatch("a", 100u);
    late.recordSet(other, {0, 0, 0}, Block::Stone);
    late.endBatch();
    late.beginBatch("b", 400u);
    late.recordSet(other, {0, 0, 0}, Block::Dirt);
    late.endBatch();
    CHECK_EQ(late.undoDepth(), 2u);
    CHECK_EQ(late.stats().mergedBatches, 0u);

    CHECK(late.undo(other));
    CHECK(other.blockAt(0, 0, 0) == Block::Stone);
    CHECK(late.undo(other));
    CHECK(other.blockAt(0, 0, 0) == Block::Air);
}

VF_TEST(aBatchThatChangesNothingIsNotRecorded) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    EditHistory history;

    history.beginBatch("same", 1u);
    CHECK(!history.recordSet(world, {0, 0, 0}, Block::Stone));
    CHECK(!history.endBatch());
    CHECK_EQ(history.undoDepth(), 0u);

    history.beginBatch("thereAndBack", 2u);
    CHECK(history.recordSet(world, {0, 0, 0}, Block::Dirt));
    CHECK(history.recordSet(world, {0, 0, 0}, Block::Stone));
    CHECK(!history.endBatch());
    CHECK_EQ(history.undoDepth(), 0u);
    CHECK(world.blockAt(0, 0, 0) == Block::Stone);
}

VF_TEST(repaintingOneBlockInsideABatchKeepsASingleOpAndUndoesToTheOriginal) {
    World world;
    world.setBlock(4, 4, 4, Block::Stone);
    EditHistory history;

    history.beginBatch("repaint", 1u);
    history.recordSet(world, {4, 4, 4}, Block::Dirt);
    history.recordSet(world, {4, 4, 4}, Block::Sand);
    history.recordSet(world, {4, 4, 4}, Block::Glass);
    CHECK_EQ(history.openBatchSize(), 1u);
    CHECK(history.endBatch());

    CHECK_EQ(history.stats().opCount, 1u);
    CHECK(world.blockAt(4, 4, 4) == Block::Glass);
    CHECK(history.undo(world));
    CHECK(world.blockAt(4, 4, 4) == Block::Stone);
}

VF_TEST(cancellingAnOpenBatchRollsTheWorldBackAndRecordsNothing) {
    World world = layeredWorld(3, 2);
    uint64_t before = worldFingerprint(world);
    EditHistory history;

    history.beginBatch("oops", 1u);
    for (int x = 0; x < 3; ++x) history.recordSet(world, {x, 5, 0}, Block::Lava);
    CHECK(worldFingerprint(world) != before);

    CHECK(history.cancelBatch(world));
    CHECK_EQ(worldFingerprint(world), before);
    CHECK_EQ(history.undoDepth(), 0u);
    CHECK(!history.batchOpen());
    CHECK(!history.cancelBatch(world));
}

VF_TEST(historyStatsReportBatchesOperationsBytesAndCounters) {
    World world;
    EditHistory history;
    CHECK_EQ(history.stats().bytes, 0u);

    for (int batch = 0; batch < 3; ++batch) {
        history.beginBatch("b", static_cast<uint64_t>(batch));
        for (int i = 0; i < 4; ++i)
            history.recordSet(world, {batch, 0, i}, Block::Stone);
        history.endBatch();
    }

    HistoryStats summary = history.stats();
    CHECK_EQ(summary.batchCount, 3u);
    CHECK_EQ(summary.opCount, 12u);
    CHECK(summary.bytes >= 12u * sizeof(EditOp));
    CHECK_EQ(summary.undoCount, 0u);
    CHECK_EQ(summary.redoBatchCount, 0u);

    history.undo(world);
    history.undo(world);
    summary = history.stats();
    CHECK_EQ(summary.batchCount, 1u);
    CHECK_EQ(summary.redoBatchCount, 2u);
    CHECK_EQ(summary.opCount, 4u);
    CHECK_EQ(summary.undoCount, 2u);

    history.clear();
    summary = history.stats();
    CHECK_EQ(summary.batchCount, 0u);
    CHECK_EQ(summary.redoBatchCount, 0u);
    CHECK_EQ(summary.bytes, 0u);
    CHECK(history.peekUndo() == nullptr);
    CHECK(history.peekRedo() == nullptr);
}
