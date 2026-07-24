#include "vf/ChunkPipeline.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <string>

namespace vf {

namespace {

bool coordLess(const Vec3i& a, const Vec3i& b) {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

std::uint64_t nanosSince(const std::chrono::steady_clock::time_point& start) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

std::string coordLabel(const Vec3i& coord) {
    return "chunk(" + std::to_string(coord.x) + "," + std::to_string(coord.y) + "," +
           std::to_string(coord.z) + ")";
}

} // namespace

const char* chunkStageName(ChunkStage stage) {
    switch (stage) {
        case ChunkStage::Generate: return "generate";
        case ChunkStage::Light: return "light";
        case ChunkStage::Mesh: return "mesh";
        case ChunkStage::Ready: return "ready";
    }
    return "unknown";
}

double PipelineStats::averageStageMs(ChunkStage stage) const {
    std::size_t index = static_cast<std::size_t>(stage);
    if (stageCompleted[index] == 0) return 0.0;
    double total = static_cast<double>(stageNanos[index]);
    return total / static_cast<double>(stageCompleted[index]) / 1000000.0;
}

ChunkPipeline::ChunkPipeline(const PipelineConfig& config)
    : config(config),
      generator(config.gen),
      chunkStore(config.cacheBudgetBytes, config.cachePolicy),
      jobs(config.threadCount) {}

void ChunkPipeline::setObserver(const Vec3i& chunkCoord) {
    {
        std::lock_guard<std::mutex> lock(controlMutex);
        observerChunk = chunkCoord;
    }
    chunkStore.setObserver(chunkCoord);
}

Vec3i ChunkPipeline::observer() const {
    std::lock_guard<std::mutex> lock(controlMutex);
    return observerChunk;
}

long long ChunkPipeline::distanceSqLocked(const Vec3i& chunkCoord) const {
    long long dx = chunkCoord.x - observerChunk.x;
    long long dy = chunkCoord.y - observerChunk.y;
    long long dz = chunkCoord.z - observerChunk.z;
    return dx * dx + dy * dy + dz * dz;
}

bool ChunkPipeline::inViewVolumeLocked(const Vec3i& chunkCoord) const {
    if (chunkCoord.y < 0 || chunkCoord.y >= config.verticalChunks) return false;
    long long dx = chunkCoord.x - observerChunk.x;
    long long dz = chunkCoord.z - observerChunk.z;
    long long radius = config.viewRadius;
    return dx * dx + dz * dz <= radius * radius;
}

bool ChunkPipeline::inViewVolume(const Vec3i& chunkCoord) const {
    std::lock_guard<std::mutex> lock(controlMutex);
    return inViewVolumeLocked(chunkCoord);
}

JobPriority ChunkPipeline::priorityFor(long long distanceSq) const {
    long long radius = config.viewRadius;
    long long radiusSq = radius * radius;
    if (distanceSq * 9 <= radiusSq) return JobPriority::High;
    if (distanceSq * 9 <= radiusSq * 4) return JobPriority::Normal;
    return JobPriority::Low;
}

void ChunkPipeline::enqueue(const Vec3i& chunkCoord) {
    std::lock_guard<std::mutex> lock(controlMutex);
    if (!pendingSet.insert(chunkCoord).second) return;
    pending.push_back(chunkCoord);
}

std::size_t ChunkPipeline::enqueueViewVolume() {
    std::vector<Vec3i> volume;
    {
        std::lock_guard<std::mutex> lock(controlMutex);
        int radius = config.viewRadius;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int y = 0; y < config.verticalChunks; ++y) {
                    Vec3i coord{observerChunk.x + dx, y, observerChunk.z + dz};
                    if (inViewVolumeLocked(coord)) volume.push_back(coord);
                }
            }
        }
    }
    std::sort(volume.begin(), volume.end(), coordLess);
    std::size_t added = 0;
    for (const Vec3i& coord : volume) {
        std::lock_guard<std::mutex> lock(controlMutex);
        if (!pendingSet.insert(coord).second) continue;
        pending.push_back(coord);
        ++added;
    }
    return added;
}

std::size_t ChunkPipeline::pendingCount() const {
    std::lock_guard<std::mutex> lock(controlMutex);
    return pending.size();
}

void ChunkPipeline::cancel(const Vec3i& chunkCoord) {
    std::lock_guard<std::mutex> lock(controlMutex);
    cancelledCoords.insert(chunkCoord);
    auto active = activeIndex.find(chunkCoord);
    if (active != activeIndex.end())
        cancelFlags[active->second]->store(true, std::memory_order_release);
}

bool ChunkPipeline::isCancelled(const Vec3i& chunkCoord) const {
    std::lock_guard<std::mutex> lock(controlMutex);
    return cancelledCoords.count(chunkCoord) != 0;
}

std::vector<ChunkTask> ChunkPipeline::pendingTasks() const {
    std::lock_guard<std::mutex> lock(controlMutex);
    std::vector<ChunkTask> tasks;
    tasks.reserve(pending.size());
    for (const Vec3i& coord : pending) {
        ChunkTask task;
        task.coord = coord;
        task.stage = ChunkStage::Generate;
        task.distanceSq = distanceSqLocked(coord);
        task.priority = priorityFor(task.distanceSq);
        task.inRange = inViewVolumeLocked(coord);
        tasks.push_back(task);
    }
    std::sort(tasks.begin(), tasks.end(), [](const ChunkTask& a, const ChunkTask& b) {
        if (a.distanceSq != b.distanceSq) return a.distanceSq < b.distanceSq;
        return coordLess(a.coord, b.coord);
    });
    return tasks;
}

void ChunkPipeline::stageGenerate(ChunkTaskResult& slot, Chunk* target,
                                  std::atomic<bool>& cancelled) {
    if (cancelled.load(std::memory_order_acquire)) {
        slot.cancelled = true;
        return;
    }
    auto start = std::chrono::steady_clock::now();
    generator.generateChunk(*target, slot.coord);
    slot.stage = ChunkStage::Generate;
    slot.stageNanos[static_cast<int>(ChunkStage::Generate)] = nanosSince(start);
}

void ChunkPipeline::stageLight(ChunkTaskResult& slot, std::atomic<bool>& cancelled) {
    if (cancelled.load(std::memory_order_acquire) || slot.cancelled) {
        slot.cancelled = true;
        return;
    }
    auto start = std::chrono::steady_clock::now();
    Vec3i origin{slot.coord.x * chunkSize - 1, slot.coord.y * chunkSize - 1,
                 slot.coord.z * chunkSize - 1};
    Vec3i dims{chunkSize + 2, chunkSize + 2, chunkSize + 2};
    slot.light = computeLight(world, origin, dims);
    slot.stage = ChunkStage::Light;
    slot.stageNanos[static_cast<int>(ChunkStage::Light)] = nanosSince(start);
}

void ChunkPipeline::stageMesh(ChunkTaskResult& slot, std::atomic<bool>& cancelled) {
    if (cancelled.load(std::memory_order_acquire) || slot.cancelled) {
        slot.cancelled = true;
        return;
    }
    auto start = std::chrono::steady_clock::now();
    slot.mesh = meshChunkLit(world, slot.coord, slot.light);
    slot.stage = ChunkStage::Mesh;
    slot.stageNanos[static_cast<int>(ChunkStage::Mesh)] = nanosSince(start);
}

void ChunkPipeline::stageReady(ChunkTaskResult& slot, const Chunk* source,
                               std::atomic<bool>& cancelled) {
    if (cancelled.load(std::memory_order_acquire) || slot.cancelled) {
        slot.cancelled = true;
        return;
    }
    auto start = std::chrono::steady_clock::now();
    slot.chunk = *source;
    slot.entryBytes = ChunkCache::entrySizeBytes(slot.chunk, slot.mesh);
    slot.stage = ChunkStage::Ready;
    slot.stageNanos[static_cast<int>(ChunkStage::Ready)] = nanosSince(start);
}

void ChunkPipeline::run() {
    std::vector<ChunkTask> tasks = pendingTasks();
    if (tasks.empty()) return;

    std::size_t count = tasks.size();
    std::vector<std::atomic<bool>*> flags(count, nullptr);
    {
        std::lock_guard<std::mutex> lock(controlMutex);
        activeIndex.clear();
        cancelFlags.clear();
        cancelFlags.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            activeIndex[tasks[i].coord] = i;
            bool cancelled = !tasks[i].inRange || cancelledCoords.count(tasks[i].coord) != 0;
            cancelFlags.push_back(std::unique_ptr<std::atomic<bool>>(
                new std::atomic<bool>(cancelled)));
            flags[i] = cancelFlags.back().get();
        }
    }

    std::vector<ChunkTaskResult> slots(count);
    std::vector<Chunk*> targets(count, nullptr);
    std::unordered_map<Vec3i, std::size_t, Vec3iHash> indexOf;
    indexOf.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        slots[i].coord = tasks[i].coord;
        indexOf[tasks[i].coord] = i;
        if (!flags[i]->load(std::memory_order_acquire))
            targets[i] = &world.ensureChunk(tasks[i].coord);
    }

    JobGraph graph;
    std::vector<std::array<std::size_t, chunkStageCount>> nodes(count);
    for (std::size_t i = 0; i < count; ++i) {
        JobPriority priority = tasks[i].priority;
        std::string label = coordLabel(tasks[i].coord);
        nodes[i][0] = graph.addNode(label + " generate",
                                    [this, i, &slots, &targets, &flags] {
                                        stageGenerate(slots[i], targets[i], *flags[i]);
                                    },
                                    priority);
        nodes[i][1] = graph.addNode(label + " light",
                                    [this, i, &slots, &flags] {
                                        stageLight(slots[i], *flags[i]);
                                    },
                                    priority);
        nodes[i][2] = graph.addNode(label + " mesh",
                                    [this, i, &slots, &flags] {
                                        stageMesh(slots[i], *flags[i]);
                                    },
                                    priority);
        nodes[i][3] = graph.addNode(label + " ready",
                                    [this, i, &slots, &targets, &flags] {
                                        stageReady(slots[i], targets[i], *flags[i]);
                                    },
                                    priority);
    }

    for (std::size_t i = 0; i < count; ++i) {
        graph.addDependency(nodes[i][1], nodes[i][0]);
        graph.addDependency(nodes[i][2], nodes[i][1]);
        graph.addDependency(nodes[i][3], nodes[i][2]);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    Vec3i neighbour = tasks[i].coord + Vec3i{dx, dy, dz};
                    auto found = indexOf.find(neighbour);
                    if (found == indexOf.end()) continue;
                    graph.addDependency(nodes[i][1], nodes[found->second][0]);
                    graph.addDependency(nodes[i][2], nodes[found->second][0]);
                }
            }
        }
    }

    JobBatch batch = jobs.runGraph(graph);
    batch.wait();
    jobs.waitAll();

    std::vector<std::size_t> mergeOrder(count);
    std::iota(mergeOrder.begin(), mergeOrder.end(), std::size_t{0});
    std::sort(mergeOrder.begin(), mergeOrder.end(), [&tasks](std::size_t a, std::size_t b) {
        return coordLess(tasks[a].coord, tasks[b].coord);
    });

    PipelineStats delta;
    completedResults.clear();
    resultIndex.clear();
    completedResults.reserve(count);
    for (std::size_t index : mergeOrder) {
        ChunkTaskResult& slot = slots[index];
        for (int stage = 0; stage < chunkStageCount; ++stage)
            delta.stageNanos[static_cast<std::size_t>(stage)] += slot.stageNanos[stage];
        if (slot.cancelled) {
            ++delta.cancelled;
            continue;
        }
        for (int stage = 0; stage <= static_cast<int>(slot.stage); ++stage)
            ++delta.stageCompleted[static_cast<std::size_t>(stage)];
        ++delta.chunksReady;
        chunkStore.store(slot.coord, slot.chunk, slot.mesh);
        resultIndex[slot.coord] = completedResults.size();
        completedResults.push_back(std::move(slot));
    }

    for (const Vec3i& coord : chunkStore.residentCoords()) {
        if (inViewVolume(coord)) chunkStore.pin(coord);
        else chunkStore.unpin(coord);
    }

    std::lock_guard<std::mutex> lock(controlMutex);
    for (int stage = 0; stage < chunkStageCount; ++stage) {
        std::size_t at = static_cast<std::size_t>(stage);
        counters.stageCompleted[at] += delta.stageCompleted[at];
        counters.stageNanos[at] += delta.stageNanos[at];
    }
    counters.cancelled += delta.cancelled;
    counters.chunksReady += delta.chunksReady;
    counters.cachedBytes = chunkStore.bytesUsed();
    pending.clear();
    pendingSet.clear();
    activeIndex.clear();
    cancelFlags.clear();
}

const ChunkTaskResult* ChunkPipeline::resultFor(const Vec3i& chunkCoord) const {
    auto found = resultIndex.find(chunkCoord);
    if (found == resultIndex.end()) return nullptr;
    return &completedResults[found->second];
}

PipelineStats ChunkPipeline::stats() const {
    std::lock_guard<std::mutex> lock(controlMutex);
    return counters;
}

void ChunkPipeline::resetStats() {
    std::lock_guard<std::mutex> lock(controlMutex);
    counters = PipelineStats{};
}

std::vector<std::uint8_t> ChunkPipeline::digest() const {
    std::vector<std::uint8_t> bytes;
    auto appendRaw = [&bytes](const void* source, std::size_t size) {
        const std::uint8_t* first = static_cast<const std::uint8_t*>(source);
        bytes.insert(bytes.end(), first, first + size);
    };
    auto appendInt = [&appendRaw](int value) {
        std::int32_t narrowed = value;
        appendRaw(&narrowed, sizeof(narrowed));
    };
    auto appendCount = [&appendRaw](std::uint64_t value) { appendRaw(&value, sizeof(value)); };
    auto appendMesh = [&](const Mesh& mesh) {
        appendCount(mesh.vertices.size());
        if (!mesh.vertices.empty())
            appendRaw(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
        appendCount(mesh.indices.size());
        if (!mesh.indices.empty())
            appendRaw(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    };

    std::vector<Vec3i> worldCoords;
    worldCoords.reserve(world.chunkCount());
    for (const auto& slot : world.allChunks()) worldCoords.push_back(slot.first);
    std::sort(worldCoords.begin(), worldCoords.end(), coordLess);

    appendCount(worldCoords.size());
    for (const Vec3i& coord : worldCoords) {
        appendInt(coord.x);
        appendInt(coord.y);
        appendInt(coord.z);
        const Chunk* chunk = world.chunkAt(coord);
        appendRaw(chunk->raw().data(), chunk->raw().size() * sizeof(Block));
    }

    appendCount(completedResults.size());
    for (const ChunkTaskResult& produced : completedResults) {
        appendInt(produced.coord.x);
        appendInt(produced.coord.y);
        appendInt(produced.coord.z);
        bytes.push_back(static_cast<std::uint8_t>(produced.stage));
        bytes.push_back(produced.cancelled ? 1u : 0u);
        appendRaw(produced.chunk.raw().data(), produced.chunk.raw().size() * sizeof(Block));
        appendCount(produced.light.sun.size());
        if (!produced.light.sun.empty())
            appendRaw(produced.light.sun.data(), produced.light.sun.size());
        if (!produced.light.block.empty())
            appendRaw(produced.light.block.data(), produced.light.block.size());
        appendMesh(produced.mesh.opaque);
        appendMesh(produced.mesh.transparent);
        appendCount(produced.entryBytes);
    }

    std::vector<Vec3i> cached = chunkStore.residentCoords();
    appendCount(cached.size());
    appendCount(chunkStore.bytesUsed());
    for (const Vec3i& coord : cached) {
        appendInt(coord.x);
        appendInt(coord.y);
        appendInt(coord.z);
        bytes.push_back(chunkStore.hasMesh(coord) ? 1u : 0u);
        bytes.push_back(chunkStore.isPinned(coord) ? 1u : 0u);
    }
    return bytes;
}

} // namespace vf
