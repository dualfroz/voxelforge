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
    int viewRadius = 2;
    int verticalChunks = 2;
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

    void cancel(const Vec3i& chunkCoord);
    bool isCancelled(const Vec3i& chunkCoord) const;

    std::vector<ChunkTask> pendingTasks() const;

    void run();

    const std::vector<ChunkTaskResult>& results() const { return completedResults; }
    const ChunkTaskResult* resultFor(const Vec3i& chunkCoord) const;

    PipelineStats stats() const;
    void resetStats();

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

}
