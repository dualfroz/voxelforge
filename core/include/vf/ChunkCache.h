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

    bool fetchChunk(const Vec3i& coord, Chunk& out);
    bool fetchMesh(const Vec3i& coord, ChunkMesh& out);

    bool peekChunk(const Vec3i& coord, Chunk& out) const;
    bool peekMesh(const Vec3i& coord, ChunkMesh& out) const;
    bool contains(const Vec3i& coord) const;
    bool hasMesh(const Vec3i& coord) const;
    bool entryFor(const Vec3i& coord, CacheEntry& out) const;

    void pin(const Vec3i& coord);
    void unpin(const Vec3i& coord);
    bool isPinned(const Vec3i& coord) const;

    std::size_t invalidateBlock(int gx, int gy, int gz);
    void invalidateChunk(const Vec3i& coord);
    void invalidateMesh(const Vec3i& coord);
    void clear();

    std::size_t size() const;
    std::uint64_t bytesUsed() const;
    CacheStats stats() const;
    void resetStats();

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

}
