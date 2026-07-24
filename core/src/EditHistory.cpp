#include "vf/EditHistory.h"

#include <algorithm>

namespace vf {

std::size_t EditBatch::byteSize() const {
    return sizeof(EditBatch) + label.size() + ops.size() * sizeof(EditOp);
}

bool EditBatch::touches(const Vec3i& position) const {
    for (const EditOp& op : ops)
        if (op.position == position) return true;
    return false;
}

EditHistory::EditHistory(std::size_t memoryBudgetBytes) : budget(memoryBudgetBytes) {}

void EditHistory::setMemoryBudget(std::size_t bytes) {
    budget = bytes;
    enforceBudget();
}

bool EditHistory::beginBatch(const std::string& label, uint64_t tick) {
    if (open) return false;
    pending = EditBatch{};
    pending.label = label;
    pending.tick = tick;
    pendingIndex.clear();
    open = true;
    return true;
}

bool EditHistory::recordSet(World& world, const Vec3i& position, Block block) {
    if (!open) return false;
    Block current = world.blockAt(position.x, position.y, position.z);
    if (current == block) return false;

    auto known = pendingIndex.find(position);
    if (known == pendingIndex.end()) {
        pendingIndex.emplace(position, pending.ops.size());
        pending.ops.push_back(EditOp{position, current, block});
    } else {
        pending.ops[known->second].after = block;
    }
    world.setBlock(position.x, position.y, position.z, block);
    return true;
}

void EditHistory::compact(EditBatch& batch) {
    batch.ops.erase(std::remove_if(batch.ops.begin(), batch.ops.end(),
                                   [](const EditOp& op) { return op.before == op.after; }),
                    batch.ops.end());
}

bool EditHistory::endBatch() {
    if (!open) return false;
    open = false;
    pendingIndex.clear();
    compact(pending);
    if (pending.ops.empty()) {
        pending = EditBatch{};
        return false;
    }

    redoStack.clear();
    if (!mergeIntoPrevious(pending)) pushBatch(std::move(pending));
    pending = EditBatch{};
    enforceBudget();
    return true;
}

bool EditHistory::cancelBatch(World& world) {
    if (!open) return false;
    open = false;
    pendingIndex.clear();
    for (std::size_t i = pending.ops.size(); i > 0; --i) {
        const EditOp& op = pending.ops[i - 1];
        world.setBlock(op.position.x, op.position.y, op.position.z, op.before);
    }
    pending = EditBatch{};
    return true;
}

bool EditHistory::mergeIntoPrevious(const EditBatch& batch) {
    if (mergeWindow == 0u || undoStack.empty()) return false;
    EditBatch& previous = undoStack.back();
    uint64_t gap = batch.tick >= previous.tick ? batch.tick - previous.tick
                                               : previous.tick - batch.tick;
    if (gap > mergeWindow) return false;

    bool overlaps = false;
    for (const EditOp& op : batch.ops) {
        if (previous.touches(op.position)) {
            overlaps = true;
            break;
        }
    }
    if (!overlaps) return false;

    std::unordered_map<Vec3i, std::size_t, Vec3iHash> index;
    index.reserve(previous.ops.size());
    for (std::size_t i = 0; i < previous.ops.size(); ++i) index.emplace(previous.ops[i].position, i);

    for (const EditOp& op : batch.ops) {
        auto known = index.find(op.position);
        if (known == index.end()) {
            index.emplace(op.position, previous.ops.size());
            previous.ops.push_back(op);
        } else {
            previous.ops[known->second].after = op.after;
        }
    }
    compact(previous);
    previous.tick = batch.tick;
    ++mergedBatches;
    if (previous.ops.empty()) undoStack.pop_back();
    return true;
}

void EditHistory::pushBatch(EditBatch&& batch) { undoStack.push_back(std::move(batch)); }

void EditHistory::recomputeBytes() {
    std::size_t total = 0;
    for (const EditBatch& batch : undoStack) total += batch.byteSize();
    for (const EditBatch& batch : redoStack) total += batch.byteSize();
    bytesUsed = total;
}

void EditHistory::enforceBudget() {
    recomputeBytes();
    while (bytesUsed > budget && !undoStack.empty()) {
        bytesUsed -= undoStack.front().byteSize();
        undoStack.pop_front();
        ++droppedBatches;
    }
    while (bytesUsed > budget && !redoStack.empty()) {
        bytesUsed -= redoStack.front().byteSize();
        redoStack.pop_front();
        ++droppedBatches;
    }
}

bool EditHistory::undo(World& world) {
    if (undoStack.empty()) return false;
    EditBatch batch = std::move(undoStack.back());
    undoStack.pop_back();
    for (std::size_t i = batch.ops.size(); i > 0; --i) {
        const EditOp& op = batch.ops[i - 1];
        world.setBlock(op.position.x, op.position.y, op.position.z, op.before);
    }
    redoStack.push_back(std::move(batch));
    ++undoCount;
    enforceBudget();
    return true;
}

bool EditHistory::redo(World& world) {
    if (redoStack.empty()) return false;
    EditBatch batch = std::move(redoStack.back());
    redoStack.pop_back();
    for (const EditOp& op : batch.ops)
        world.setBlock(op.position.x, op.position.y, op.position.z, op.after);
    undoStack.push_back(std::move(batch));
    ++redoCount;
    enforceBudget();
    return true;
}

const EditBatch* EditHistory::peekUndo() const {
    return undoStack.empty() ? nullptr : &undoStack.back();
}

const EditBatch* EditHistory::peekRedo() const {
    return redoStack.empty() ? nullptr : &redoStack.back();
}

void EditHistory::clear() {
    undoStack.clear();
    redoStack.clear();
    pending = EditBatch{};
    pendingIndex.clear();
    open = false;
    bytesUsed = 0;
}

HistoryStats EditHistory::stats() const {
    HistoryStats summary;
    summary.batchCount = undoStack.size();
    summary.redoBatchCount = redoStack.size();
    for (const EditBatch& batch : undoStack) summary.opCount += batch.ops.size();
    summary.bytes = bytesUsed;
    summary.undoCount = undoCount;
    summary.redoCount = redoCount;
    summary.droppedBatches = droppedBatches;
    summary.mergedBatches = mergedBatches;
    return summary;
}

} // namespace vf
