#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "Block.h"
#include "Math.h"
#include "World.h"

namespace vf {

struct EditOp {
    Vec3i position{0, 0, 0};
    Block before = Block::Air;
    Block after = Block::Air;
};

struct EditBatch {
    std::string label;
    uint64_t tick = 0;
    std::vector<EditOp> ops;

    std::size_t byteSize() const;
    bool touches(const Vec3i& position) const;
};

struct HistoryStats {
    std::size_t batchCount = 0;
    std::size_t redoBatchCount = 0;
    std::size_t opCount = 0;
    std::size_t bytes = 0;
    std::size_t undoCount = 0;
    std::size_t redoCount = 0;
    std::size_t droppedBatches = 0;
    std::size_t mergedBatches = 0;
};

class EditHistory {
public:
    explicit EditHistory(std::size_t memoryBudgetBytes = 1u << 20);

    void setMemoryBudget(std::size_t bytes);
    std::size_t memoryBudget() const { return budget; }

    void setMergeWindow(uint64_t ticks) { mergeWindow = ticks; }
    uint64_t mergeWindowTicks() const { return mergeWindow; }

    bool beginBatch(const std::string& label, uint64_t tick);
    bool batchOpen() const { return open; }
    std::size_t openBatchSize() const { return pending.ops.size(); }

    bool recordSet(World& world, const Vec3i& position, Block block);
    bool endBatch();
    bool cancelBatch(World& world);

    bool undo(World& world);
    bool redo(World& world);

    std::size_t undoDepth() const { return undoStack.size(); }
    std::size_t redoDepth() const { return redoStack.size(); }
    const EditBatch* peekUndo() const;
    const EditBatch* peekRedo() const;
    void clear();

    HistoryStats stats() const;

private:
    void pushBatch(EditBatch&& batch);
    bool mergeIntoPrevious(const EditBatch& batch);
    void enforceBudget();
    void recomputeBytes();
    static void compact(EditBatch& batch);

    std::deque<EditBatch> undoStack;
    std::deque<EditBatch> redoStack;
    EditBatch pending;
    std::unordered_map<Vec3i, std::size_t, Vec3iHash> pendingIndex;
    bool open = false;

    std::size_t budget = 1u << 20;
    std::size_t bytesUsed = 0;
    uint64_t mergeWindow = 0;
    std::size_t undoCount = 0;
    std::size_t redoCount = 0;
    std::size_t droppedBatches = 0;
    std::size_t mergedBatches = 0;
};

}
