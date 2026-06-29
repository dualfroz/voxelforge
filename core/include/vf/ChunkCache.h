#pragma once

#include <cstddef>
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "vf/Chunk.h"
#include "vf/Math.h"
#include "vf/Mesh.h"
#include "vf/World.h"

namespace vf {

// Lru drops the entry untouched for the longest time. DistanceWeighted mixes
// staleness with the squared chunk distance to the observer, so a chunk far
// behind the player loses to a stale one still under his feet.
enum class CachePolicy : std::uint8_t {
    Lru = 0,
    DistanceWeighted = 1,
};

struct CacheEntry {
    Vec3i coord;
    Chunk chunk;
    ChunkMesh mesh;
    std::uint64_t bytes = 0;
    std::uint64_t lastUse = 0;
    long long distanceSq = 0;
    bool hasMesh = false;
    bool pinned = false;
};

struct CacheStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::uint64_t invalidations = 0;
    std::uint64_t meshInvalidations = 0;
    std::uint64_t bytesUsed = 0;
    std::size_t entries = 0;

    double hitRate() const {
        std::uint64_t lookups = hits + misses;
        if (lookups == 0) return 0.0;
        return static_cast<double>(hits) / static_cast<double>(lookups);
    }
};

// Resident chunks and their baked meshes under a hard byte budget. Entries are
// evicted by policy once a store pushes the cache past the budget; pinned
// entries are never candidates, which is how the pipeline keeps everything
// inside the view radius alive. Every method is safe to call concurrently:
// peek reads share the lock, everything that touches recency or the store takes
// it exclusively.
class ChunkCache {
public:
    explicit ChunkCache(std::uint64_t byteBudget = 64ull * 1024ull * 1024ull,
                        CachePolicy policy = CachePolicy::Lru);

    ChunkCache(const ChunkCache&) = delete;
    ChunkCache& operator=(const ChunkCache&) = delete;

    void setByteBudget(std::uint64_t bytes);
    void setPolicy(CachePolicy policy);
    void setDistanceWeight(std::uint64_t weight);
    void setObserver(const Vec3i& chunkCoord);

    std::uint64_t byteBudget() const;
    CachePolicy policy() const;
    Vec3i observer() const;

    void store(const Vec3i& coord, const Chunk& chunk);
    void store(const Vec3i& coord, const Chunk& chunk, const ChunkMesh& mesh);

    // Counting lookups: they record a hit or a miss and refresh recency.
    bool fetchChunk(const Vec3i& coord, Chunk& out);
    bool fetchMesh(const Vec3i& coord, ChunkMesh& out);

    // Silent lookups for the pipeline workers: no recency, no statistics, and
    // only a shared lock, so many stages can read at once.
    bool peekChunk(const Vec3i& coord, Chunk& out) const;
    bool peekMesh(const Vec3i& coord, ChunkMesh& out) const;
    bool contains(const Vec3i& coord) const;
    bool hasMesh(const Vec3i& coord) const;
    bool entryFor(const Vec3i& coord, CacheEntry& out) const;

    void pin(const Vec3i& coord);
    void unpin(const Vec3i& coord);
    bool isPinned(const Vec3i& coord) const;

    // Drops the owning chunk entirely and the meshes of every neighbour whose
    // ambient occlusion or light sampling can read the edited voxel across the
    // chunk border. Returns the number of neighbour meshes dropped.
    std::size_t invalidateBlock(int gx, int gy, int gz);
    void invalidateChunk(const Vec3i& coord);
    void invalidateMesh(const Vec3i& coord);
    void clear();

    std::size_t size() const;
    std::uint64_t bytesUsed() const;
    CacheStats stats() const;
    void resetStats();

    // Victims first. Pinned entries are excluded because they cannot be chosen.
    std::vector<Vec3i> evictionOrder() const;
    std::vector<Vec3i> residentCoords() const;

    static std::uint64_t entrySizeBytes(const Chunk& chunk, const ChunkMesh& mesh);

private:
    long long distanceSqTo(const Vec3i& coord) const;
    std::uint64_t evictionScoreLocked(const CacheEntry& entry) const;
    void enforceBudgetLocked();
    void touchLocked(CacheEntry& entry);
    void dropMeshLocked(CacheEntry& entry);
    void eraseLocked(const Vec3i& coord);

    mutable std::shared_mutex mutex;
    std::unordered_map<Vec3i, CacheEntry, Vec3iHash> entries;
    CacheStats counters;
    Vec3i observerChunk;
    std::uint64_t budget;
    std::uint64_t used = 0;
    std::uint64_t clock = 0;
    std::uint64_t distanceWeight = 64;
    CachePolicy replacement;
};

} // namespace vf
