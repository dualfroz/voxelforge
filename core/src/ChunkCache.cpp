#include "vf/ChunkCache.h"

#include <algorithm>
#include <mutex>

namespace vf {

namespace {

bool coordLess(const Vec3i& a, const Vec3i& b) {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

std::uint64_t meshBytes(const ChunkMesh& mesh) {
    std::uint64_t total = 0;
    total += static_cast<std::uint64_t>(mesh.opaque.vertices.size()) * sizeof(Vertex);
    total += static_cast<std::uint64_t>(mesh.opaque.indices.size()) * sizeof(std::uint32_t);
    total += static_cast<std::uint64_t>(mesh.transparent.vertices.size()) * sizeof(Vertex);
    total += static_cast<std::uint64_t>(mesh.transparent.indices.size()) * sizeof(std::uint32_t);
    return total;
}

}

ChunkCache::ChunkCache(std::uint64_t byteBudget, CachePolicy policy)
    : budget(byteBudget), replacement(policy) {}

std::uint64_t ChunkCache::entrySizeBytes(const Chunk& chunk, const ChunkMesh& mesh) {
    return static_cast<std::uint64_t>(chunk.raw().size() * sizeof(Block)) + meshBytes(mesh);
}

void ChunkCache::setByteBudget(std::uint64_t bytes) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    budget = bytes;
    enforceBudgetLocked();
}

void ChunkCache::setPolicy(CachePolicy policy) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    replacement = policy;
}

void ChunkCache::setDistanceWeight(std::uint64_t weight) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    distanceWeight = weight;
}

void ChunkCache::setObserver(const Vec3i& chunkCoord) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    observerChunk = chunkCoord;
    for (auto& slot : entries) slot.second.distanceSq = distanceSqTo(slot.first);
}

std::uint64_t ChunkCache::byteBudget() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return budget;
}

CachePolicy ChunkCache::policy() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return replacement;
}

Vec3i ChunkCache::observer() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return observerChunk;
}

long long ChunkCache::distanceSqTo(const Vec3i& coord) const {
    long long dx = coord.x - observerChunk.x;
    long long dy = coord.y - observerChunk.y;
    long long dz = coord.z - observerChunk.z;
    return dx * dx + dy * dy + dz * dz;
}

std::uint64_t ChunkCache::evictionScoreLocked(const CacheEntry& entry) const {
    std::uint64_t age = clock - entry.lastUse;
    if (replacement == CachePolicy::Lru) return age;
    return age + distanceWeight * static_cast<std::uint64_t>(entry.distanceSq);
}

void ChunkCache::touchLocked(CacheEntry& entry) {
    entry.lastUse = ++clock;
}

void ChunkCache::dropMeshLocked(CacheEntry& entry) {
    used -= meshBytes(entry.mesh);
    entry.mesh = ChunkMesh{};
    entry.hasMesh = false;
    entry.bytes = entrySizeBytes(entry.chunk, entry.mesh);
}

void ChunkCache::eraseLocked(const Vec3i& coord) {
    auto slot = entries.find(coord);
    if (slot == entries.end()) return;
    used -= slot->second.bytes;
    entries.erase(slot);
}

void ChunkCache::enforceBudgetLocked() {
    while (used > budget) {
        const CacheEntry* victim = nullptr;
        std::uint64_t bestScore = 0;
        for (const auto& slot : entries) {
            if (slot.second.pinned) continue;
            std::uint64_t score = evictionScoreLocked(slot.second);
            if (victim == nullptr || score > bestScore ||
                (score == bestScore && coordLess(slot.first, victim->coord))) {
                victim = &slot.second;
                bestScore = score;
            }
        }
        if (victim == nullptr) return;
        Vec3i victimCoord = victim->coord;
        eraseLocked(victimCoord);
        ++counters.evictions;
    }
}

void ChunkCache::store(const Vec3i& coord, const Chunk& chunk) {
    store(coord, chunk, ChunkMesh{});
}

void ChunkCache::store(const Vec3i& coord, const Chunk& chunk, const ChunkMesh& mesh) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    bool wasPinned = slot != entries.end() && slot->second.pinned;
    if (slot != entries.end()) used -= slot->second.bytes;

    CacheEntry entry;
    entry.coord = coord;
    entry.chunk = chunk;
    entry.mesh = mesh;
    entry.hasMesh = !mesh.empty();
    entry.pinned = wasPinned;
    entry.distanceSq = distanceSqTo(coord);
    entry.bytes = entrySizeBytes(chunk, mesh);
    entry.lastUse = ++clock;
    used += entry.bytes;
    entries[coord] = std::move(entry);

    enforceBudgetLocked();
}

bool ChunkCache::fetchChunk(const Vec3i& coord, Chunk& out) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end()) {
        ++counters.misses;
        return false;
    }
    ++counters.hits;
    touchLocked(slot->second);
    out = slot->second.chunk;
    return true;
}

bool ChunkCache::fetchMesh(const Vec3i& coord, ChunkMesh& out) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end() || !slot->second.hasMesh) {
        ++counters.misses;
        return false;
    }
    ++counters.hits;
    touchLocked(slot->second);
    out = slot->second.mesh;
    return true;
}

bool ChunkCache::peekChunk(const Vec3i& coord, Chunk& out) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end()) return false;
    out = slot->second.chunk;
    return true;
}

bool ChunkCache::peekMesh(const Vec3i& coord, ChunkMesh& out) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end() || !slot->second.hasMesh) return false;
    out = slot->second.mesh;
    return true;
}

bool ChunkCache::contains(const Vec3i& coord) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return entries.find(coord) != entries.end();
}

bool ChunkCache::hasMesh(const Vec3i& coord) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    return slot != entries.end() && slot->second.hasMesh;
}

bool ChunkCache::entryFor(const Vec3i& coord, CacheEntry& out) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end()) return false;
    out = slot->second;
    return true;
}

void ChunkCache::pin(const Vec3i& coord) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot != entries.end()) slot->second.pinned = true;
}

void ChunkCache::unpin(const Vec3i& coord) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end()) return;
    slot->second.pinned = false;
    enforceBudgetLocked();
}

bool ChunkCache::isPinned(const Vec3i& coord) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    return slot != entries.end() && slot->second.pinned;
}

std::size_t ChunkCache::invalidateBlock(int gx, int gy, int gz) {
    Vec3i owner{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
    int lx = floorMod(gx, chunkSize);
    int ly = floorMod(gy, chunkSize);
    int lz = floorMod(gz, chunkSize);
    const int local[3] = {lx, ly, lz};

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (entries.find(owner) != entries.end()) {
        eraseLocked(owner);
        ++counters.invalidations;
    }

    std::size_t neighbourMeshes = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                const int step[3] = {dx, dy, dz};
                bool reaches = true;
                for (int axis = 0; axis < 3; ++axis) {
                    if (step[axis] == 0) continue;
                    int edge = step[axis] < 0 ? 0 : chunkSize - 1;
                    if (local[axis] != edge) reaches = false;
                }
                if (!reaches) continue;

                Vec3i neighbour{owner.x + dx, owner.y + dy, owner.z + dz};
                auto slot = entries.find(neighbour);
                if (slot == entries.end() || !slot->second.hasMesh) continue;
                dropMeshLocked(slot->second);
                ++counters.meshInvalidations;
                ++neighbourMeshes;
            }
        }
    }
    return neighbourMeshes;
}

void ChunkCache::invalidateChunk(const Vec3i& coord) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    if (entries.find(coord) == entries.end()) return;
    eraseLocked(coord);
    ++counters.invalidations;
}

void ChunkCache::invalidateMesh(const Vec3i& coord) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    auto slot = entries.find(coord);
    if (slot == entries.end() || !slot->second.hasMesh) return;
    dropMeshLocked(slot->second);
    ++counters.meshInvalidations;
}

void ChunkCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    entries.clear();
    used = 0;
}

std::size_t ChunkCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return entries.size();
}

std::uint64_t ChunkCache::bytesUsed() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return used;
}

CacheStats ChunkCache::stats() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    CacheStats snapshot = counters;
    snapshot.bytesUsed = used;
    snapshot.entries = entries.size();
    return snapshot;
}

void ChunkCache::resetStats() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    counters = CacheStats{};
}

std::vector<Vec3i> ChunkCache::evictionOrder() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    std::vector<std::pair<std::uint64_t, Vec3i>> scored;
    scored.reserve(entries.size());
    for (const auto& slot : entries) {
        if (slot.second.pinned) continue;
        scored.emplace_back(evictionScoreLocked(slot.second), slot.first);
    }
    std::sort(scored.begin(), scored.end(),
              [](const std::pair<std::uint64_t, Vec3i>& a,
                 const std::pair<std::uint64_t, Vec3i>& b) {
                  if (a.first != b.first) return a.first > b.first;
                  return coordLess(a.second, b.second);
              });
    std::vector<Vec3i> order;
    order.reserve(scored.size());
    for (const auto& pair : scored) order.push_back(pair.second);
    return order;
}

std::vector<Vec3i> ChunkCache::residentCoords() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    std::vector<Vec3i> coords;
    coords.reserve(entries.size());
    for (const auto& slot : entries) coords.push_back(slot.first);
    std::sort(coords.begin(), coords.end(), coordLess);
    return coords;
}

}
