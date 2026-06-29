#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vf/ChunkCache.h"
#include "vf/JobSystem.h"
#include "vf/Lighting.h"
#include "vf/LitMesh.h"
#include "vf/Math.h"
#include "vf/Mesh.h"
#include "vf/World.h"
#include "vf/WorldGen.h"

namespace vf {

enum class ChunkStage : std::uint8_t {
    Generate = 0,
    Light = 1,
    Mesh = 2,
    Ready = 3,
};

constexpr int chunkStageCount = 4;

const char* chunkStageName(ChunkStage stage);

struct ChunkTask {
    Vec3i coord;
    ChunkStage stage = ChunkStage::Generate;
    JobPriority priority = JobPriority::Normal;
    long long distanceSq = 0;
    bool inRange = true;
};

struct ChunkTaskResult {
    Vec3i coord;
    ChunkStage stage = ChunkStage::Generate;
    bool cancelled = false;
    Chunk chunk;
    LightField light;
    ChunkMesh mesh;
    std::uint64_t entryBytes = 0;
    std::uint64_t stageNanos[chunkStageCount] = {0, 0, 0, 0};
};

struct PipelineConfig {
    unsigned threadCount = 0;
    int viewRadius = 2;          // chunk columns kept around the observer
    int verticalChunks = 2;      // chunk layers stacked upwards from y = 0
    std::uint64_t cacheBudgetBytes = 256ull * 1024ull * 1024ull;
    CachePolicy cachePolicy = CachePolicy::DistanceWeighted;
    WorldGenParams gen;
};

struct PipelineStats {
    std::array<std::uint64_t, chunkStageCount> stageCompleted{};
    std::array<std::uint64_t, chunkStageCount> stageNanos{};
    std::uint64_t cancelled = 0;
    std::uint64_t chunksReady = 0;
    std::uint64_t cachedBytes = 0;

    double averageStageMs(ChunkStage stage) const;
};

// Chunk production line running on a JobSystem.
//
// Every queued chunk becomes four graph nodes: Generate fills its blocks, Light
// floods a box one voxel wider than the chunk on each side, Mesh bakes the lit
// and ambient-occluded geometry, Ready snapshots the entry for the cache. Light
// and Mesh both read across the chunk border, so their nodes depend on the
// Generate nodes of every one of the 26 neighbours that is part of the same run.
//
// The output is order independent by construction: workers only ever write into
// their own preallocated slot, and the slots are folded back in chunk
// coordinate order once the graph has drained. The same configuration with one
// worker and with eight therefore produces the same bytes, which digest() makes
// directly comparable.
class ChunkPipeline {
public:
    explicit ChunkPipeline(const PipelineConfig& config = {});

    ChunkPipeline(const ChunkPipeline&) = delete;
    ChunkPipeline& operator=(const ChunkPipeline&) = delete;

    const PipelineConfig& settings() const { return config; }
    JobSystem& jobSystem() { return jobs; }
    ChunkCache& cache() { return chunkStore; }
    const ChunkCache& cache() const { return chunkStore; }
    const World& worldView() const { return world; }
    const WorldGen& worldGenerator() const { return generator; }

    void setObserver(const Vec3i& chunkCoord);
    Vec3i observer() const;
    bool inViewVolume(const Vec3i& chunkCoord) const;

    void enqueue(const Vec3i& chunkCoord);
    std::size_t enqueueViewVolume();
    std::size_t pendingCount() const;

    // Marks a chunk cancelled. Every stage tests the flag as it starts, so a
    // chunk that leaves the view radius stops at the next stage boundary and
    // never reaches the result list or the cache.
    void cancel(const Vec3i& chunkCoord);
    bool isCancelled(const Vec3i& chunkCoord) const;

    // Distance ordered, nearest first, each tagged with its service class.
    std::vector<ChunkTask> pendingTasks() const;

    // Drains the pending queue through all four stages and folds the results in.
    void run();

    const std::vector<ChunkTaskResult>& results() const { return completedResults; }
    const ChunkTaskResult* resultFor(const Vec3i& chunkCoord) const;

    PipelineStats stats() const;
    void resetStats();

    // Deterministic byte image of the world, the merged results and the cache
    // footprint. Two runs that agree here agree on every produced byte.
    std::vector<std::uint8_t> digest() const;

private:
    long long distanceSqLocked(const Vec3i& chunkCoord) const;
    bool inViewVolumeLocked(const Vec3i& chunkCoord) const;
    JobPriority priorityFor(long long distanceSq) const;

    void stageGenerate(ChunkTaskResult& slot, Chunk* target, std::atomic<bool>& cancelled);
    void stageLight(ChunkTaskResult& slot, std::atomic<bool>& cancelled);
    void stageMesh(ChunkTaskResult& slot, std::atomic<bool>& cancelled);
    void stageReady(ChunkTaskResult& slot, const Chunk* source, std::atomic<bool>& cancelled);

    PipelineConfig config;
    WorldGen generator;
    World world;
    ChunkCache chunkStore;
    JobSystem jobs;

    mutable std::mutex controlMutex;
    Vec3i observerChunk;
    std::vector<Vec3i> pending;
    std::unordered_set<Vec3i, Vec3iHash> pendingSet;
    std::unordered_set<Vec3i, Vec3iHash> cancelledCoords;
    std::unordered_map<Vec3i, std::size_t, Vec3iHash> activeIndex;
    std::vector<std::unique_ptr<std::atomic<bool>>> cancelFlags;

    std::vector<ChunkTaskResult> completedResults;
    std::unordered_map<Vec3i, std::size_t, Vec3iHash> resultIndex;
    PipelineStats counters;
};

} // namespace vf
