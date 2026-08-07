#include "testHarness.h"
#include "vf/ChunkPipeline.h"

#include <algorithm>
#include <cstring>
#include <vector>

using namespace vf;

namespace {

PipelineConfig baseConfig(unsigned threadCount) {
    PipelineConfig config;
    config.threadCount = threadCount;
    config.viewRadius = 2;
    config.verticalChunks = 2;
    config.gen.seed = 20250904u;
    return config;
}

void primePipeline(ChunkPipeline& pipeline) {
    pipeline.setObserver({0, 0, 0});
    pipeline.enqueueViewVolume();
    pipeline.run();
}

std::vector<std::uint8_t> digestFor(unsigned threadCount) {
    ChunkPipeline pipeline(baseConfig(threadCount));
    primePipeline(pipeline);
    return pipeline.digest();
}

bool sameBytes(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) {
    if (a.size() != b.size()) return false;
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

bool sameMesh(const Mesh& a, const Mesh& b) {
    if (a.vertices.size() != b.vertices.size()) return false;
    if (a.indices.size() != b.indices.size()) return false;
    if (!a.vertices.empty() &&
        std::memcmp(a.vertices.data(), b.vertices.data(), a.vertices.size() * sizeof(Vertex)) != 0)
        return false;
    if (!a.indices.empty() &&
        std::memcmp(a.indices.data(), b.indices.data(),
                    a.indices.size() * sizeof(std::uint32_t)) != 0)
        return false;
    return true;
}

std::size_t viewVolumeChunkCount(const PipelineConfig& config) {
    std::size_t count = 0;
    for (int dx = -config.viewRadius; dx <= config.viewRadius; ++dx)
        for (int dz = -config.viewRadius; dz <= config.viewRadius; ++dz)
            if (dx * dx + dz * dz <= config.viewRadius * config.viewRadius)
                count += static_cast<std::size_t>(config.verticalChunks);
    return count;
}

} // namespace

VF_TEST(pipelineTakesEveryQueuedChunkThroughAllFourStages) {
    ChunkPipeline pipeline(baseConfig(4));
    std::size_t queued = 0;
    pipeline.setObserver({0, 0, 0});
    queued = pipeline.enqueueViewVolume();
    CHECK_EQ(queued, viewVolumeChunkCount(pipeline.settings()));
    CHECK_EQ(pipeline.pendingCount(), queued);

    pipeline.run();
    CHECK_EQ(pipeline.pendingCount(), static_cast<std::size_t>(0));
    CHECK_EQ(pipeline.results().size(), queued);
    for (const ChunkTaskResult& produced : pipeline.results()) {
        CHECK(produced.stage == ChunkStage::Ready);
        CHECK(!produced.cancelled);
        CHECK(produced.entryBytes > 0);
        CHECK_EQ(produced.light.sun.size(),
                 static_cast<std::size_t>((chunkSize + 2) * (chunkSize + 2) * (chunkSize + 2)));
    }
    CHECK_EQ(pipeline.worldView().chunkCount(), queued);
}

VF_TEST(pipelineResultsComeBackInChunkCoordinateOrder) {
    ChunkPipeline pipeline(baseConfig(8));
    primePipeline(pipeline);
    const std::vector<ChunkTaskResult>& produced = pipeline.results();
    CHECK(produced.size() > 1);
    for (std::size_t i = 1; i < produced.size(); ++i) {
        const Vec3i& previous = produced[i - 1].coord;
        const Vec3i& current = produced[i].coord;
        bool ascending = previous.x < current.x ||
                         (previous.x == current.x && previous.y < current.y) ||
                         (previous.x == current.x && previous.y == current.y &&
                          previous.z < current.z);
        CHECK(ascending);
    }
}

VF_TEST(oneWorkerAndEightWorkersProduceByteIdenticalOutput) {
    std::vector<std::uint8_t> serial = digestFor(0);
    CHECK(serial.size() > 100000);
    CHECK(sameBytes(serial, digestFor(1)));
    CHECK(sameBytes(serial, digestFor(4)));
    CHECK(sameBytes(serial, digestFor(8)));
}

VF_TEST(repeatedEightWorkerRunsAgreeWithThemselves) {
    std::vector<std::uint8_t> first = digestFor(8);
    std::vector<std::uint8_t> second = digestFor(8);
    CHECK(sameBytes(first, second));
}

VF_TEST(cancelledChunkNeverReachesResultsOrCache) {
    ChunkPipeline pipeline(baseConfig(4));
    pipeline.setObserver({0, 0, 0});
    std::size_t queued = pipeline.enqueueViewVolume();
    const Vec3i dropped{1, 0, 0};
    pipeline.cancel(dropped);
    CHECK(pipeline.isCancelled(dropped));

    pipeline.run();
    CHECK_EQ(pipeline.results().size(), queued - 1);
    CHECK(pipeline.resultFor(dropped) == nullptr);
    CHECK(!pipeline.cache().contains(dropped));
    CHECK(pipeline.worldView().chunkAt(dropped) == nullptr);
    CHECK_EQ(pipeline.stats().cancelled, static_cast<std::uint64_t>(1));
    CHECK(pipeline.resultFor({0, 0, 0}) != nullptr);
}

VF_TEST(chunkOutsideTheViewRadiusIsCancelledBeforeAnyStageRuns) {
    ChunkPipeline pipeline(baseConfig(4));
    pipeline.setObserver({0, 0, 0});
    std::size_t queued = pipeline.enqueueViewVolume();
    const Vec3i faraway{9, 0, 9};
    pipeline.enqueue(faraway);
    CHECK(!pipeline.inViewVolume(faraway));
    CHECK_EQ(pipeline.pendingCount(), queued + 1);

    pipeline.run();
    CHECK_EQ(pipeline.results().size(), queued);
    CHECK(pipeline.resultFor(faraway) == nullptr);
    CHECK(pipeline.worldView().chunkAt(faraway) == nullptr);
    CHECK(!pipeline.cache().contains(faraway));
    CHECK_EQ(pipeline.stats().cancelled, static_cast<std::uint64_t>(1));

    PipelineStats counted = pipeline.stats();
    CHECK_EQ(counted.stageCompleted[static_cast<std::size_t>(ChunkStage::Generate)],
             static_cast<std::uint64_t>(queued));
}

VF_TEST(readyChunksLandInTheCachePinnedInsideTheRadius) {
    ChunkPipeline pipeline(baseConfig(4));
    primePipeline(pipeline);

    CHECK_EQ(pipeline.cache().size(), pipeline.results().size());
    for (const ChunkTaskResult& produced : pipeline.results()) {
        CHECK(pipeline.cache().contains(produced.coord));
        CHECK(pipeline.cache().isPinned(produced.coord));
        Chunk cached;
        CHECK(pipeline.cache().peekChunk(produced.coord, cached));
        CHECK_EQ(std::memcmp(cached.raw().data(), produced.chunk.raw().data(), chunkVolume), 0);
    }
    CHECK(pipeline.cache().bytesUsed() > 0);
    CHECK_EQ(pipeline.cache().bytesUsed(), pipeline.stats().cachedBytes);
}

VF_TEST(pipelineStatsCountEveryStageAndCancellation) {
    ChunkPipeline pipeline(baseConfig(4));
    primePipeline(pipeline);
    PipelineStats counted = pipeline.stats();
    std::uint64_t ready = static_cast<std::uint64_t>(pipeline.results().size());

    CHECK_EQ(counted.chunksReady, ready);
    CHECK_EQ(counted.cancelled, static_cast<std::uint64_t>(0));
    for (int stage = 0; stage < chunkStageCount; ++stage) {
        CHECK_EQ(counted.stageCompleted[static_cast<std::size_t>(stage)], ready);
        CHECK(counted.averageStageMs(static_cast<ChunkStage>(stage)) >= 0.0);
    }
    CHECK(counted.stageNanos[static_cast<std::size_t>(ChunkStage::Mesh)] > 0);

    pipeline.resetStats();
    CHECK_EQ(pipeline.stats().chunksReady, static_cast<std::uint64_t>(0));
}

VF_TEST(nearChunksOutrankFarOnesInThePendingQueue) {
    PipelineConfig config = baseConfig(0);
    config.viewRadius = 6;
    config.verticalChunks = 1;
    ChunkPipeline pipeline(config);
    pipeline.setObserver({0, 0, 0});
    pipeline.enqueueViewVolume();

    std::vector<ChunkTask> tasks = pipeline.pendingTasks();
    CHECK(tasks.size() > 20);
    CHECK(tasks.front().coord == (Vec3i{0, 0, 0}));
    CHECK(tasks.front().priority == JobPriority::High);
    CHECK(tasks.back().priority == JobPriority::Low);
    for (std::size_t i = 1; i < tasks.size(); ++i) {
        CHECK(tasks[i - 1].distanceSq <= tasks[i].distanceSq);
        CHECK(static_cast<int>(tasks[i - 1].priority) <= static_cast<int>(tasks[i].priority));
        CHECK(tasks[i].inRange);
    }
}

VF_TEST(pipelineBlocksMatchDirectWorldGenOutput) {
    ChunkPipeline pipeline(baseConfig(8));
    primePipeline(pipeline);

    WorldGen reference(pipeline.settings().gen);
    int compared = 0;
    for (const ChunkTaskResult& produced : pipeline.results()) {
        Chunk expected;
        reference.generateChunk(expected, produced.coord);
        CHECK_EQ(std::memcmp(expected.raw().data(), produced.chunk.raw().data(), chunkVolume), 0);
        const Chunk* resident = pipeline.worldView().chunkAt(produced.coord);
        CHECK(resident != nullptr);
        CHECK_EQ(std::memcmp(expected.raw().data(), resident->raw().data(), chunkVolume), 0);
        ++compared;
    }
    CHECK(compared > 0);
    CHECK(!pipeline.worldView().chunkAt({0, 0, 0})->empty());
}

VF_TEST(pipelineMeshMatchesDirectLitMeshing) {
    ChunkPipeline pipeline(baseConfig(8));
    primePipeline(pipeline);

    const Vec3i probe{0, 1, 0};
    const ChunkTaskResult* produced = pipeline.resultFor(probe);
    CHECK(produced != nullptr);

    Vec3i origin{probe.x * chunkSize - 1, probe.y * chunkSize - 1, probe.z * chunkSize - 1};
    Vec3i dims{chunkSize + 2, chunkSize + 2, chunkSize + 2};
    LightField expectedLight = computeLight(pipeline.worldView(), origin, dims);
    CHECK_EQ(expectedLight.sun.size(), produced->light.sun.size());
    CHECK_EQ(std::memcmp(expectedLight.sun.data(), produced->light.sun.data(),
                         expectedLight.sun.size()),
             0);
    CHECK_EQ(std::memcmp(expectedLight.block.data(), produced->light.block.data(),
                         expectedLight.block.size()),
             0);

    ChunkMesh expectedMesh = meshChunkLit(pipeline.worldView(), probe, expectedLight);
    CHECK(sameMesh(expectedMesh.opaque, produced->mesh.opaque));
    CHECK(sameMesh(expectedMesh.transparent, produced->mesh.transparent));
    CHECK(!produced->mesh.opaque.empty());
}

VF_TEST(pipelineDrivesItsWorkThroughTheJobSystem) {
    ChunkPipeline pipeline(baseConfig(4));
    primePipeline(pipeline);
    JobStats counted = pipeline.jobSystem().stats();
    std::uint64_t nodes = static_cast<std::uint64_t>(pipeline.results().size()) * chunkStageCount;

    CHECK_EQ(counted.executed, nodes);
    CHECK_EQ(counted.submitted, nodes);
    CHECK_EQ(counted.failed, static_cast<std::uint64_t>(0));
    CHECK(counted.peakBusyWorkers >= 1u);
    CHECK(counted.peakBusyWorkers <= 4u);
}

VF_TEST(secondRunAfterMovingTheObserverUnpinsWhatLeftTheRadius) {
    ChunkPipeline pipeline(baseConfig(4));
    primePipeline(pipeline);
    const Vec3i edge{-2, 0, 0};
    CHECK(pipeline.cache().isPinned(edge));

    pipeline.setObserver({4, 0, 0});
    pipeline.enqueueViewVolume();
    pipeline.run();

    CHECK(!pipeline.inViewVolume(edge));
    CHECK(pipeline.cache().contains(edge));
    CHECK(!pipeline.cache().isPinned(edge));
    CHECK(pipeline.cache().isPinned({4, 0, 0}));
}
