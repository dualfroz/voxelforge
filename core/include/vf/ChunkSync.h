#pragma once

#include <cstddef>
#include <set>
#include <unordered_map>
#include "Chunk.h"
#include "Math.h"
#include "Protocol.h"
#include "World.h"

namespace vf {

// Server side of chunk replication: records which block slots in each chunk
// changed since the last flush and turns them into wire packets.
class ServerChunkSync {
public:
    void markDirty(const Vec3i& chunkCoord, int x, int y, int z);
    void markDirtyIndex(const Vec3i& chunkCoord, int localIndex);

    std::size_t dirtyCount(const Vec3i& chunkCoord) const;
    bool anyDirty() const;
    void clear(const Vec3i& chunkCoord);
    void clearAll() { dirty.clear(); }

    // A full chunk snapshot for the initial send; false when the chunk is absent.
    bool buildFull(const World& world, const Vec3i& chunkCoord, ChunkData& out) const;

    // An incremental update from the dirty set; false when nothing is dirty.
    bool buildDelta(const World& world, const Vec3i& chunkCoord, MultiBlockChange& out) const;

    // Whether a full resend is cheaper than a per-block delta at this dirty size.
    bool shouldResendFull(const Vec3i& chunkCoord, std::size_t threshold) const;

private:
    std::unordered_map<Vec3i, std::set<int>, Vec3iHash> dirty;
};

// Client side: applies received packets into a local world mirror.
class ClientChunkSync {
public:
    void applyFull(const ChunkData& packet, World& world);
    void applyDelta(const MultiBlockChange& packet, World& world);
    void applyBlockChange(const BlockChange& packet, World& world);
};

} // namespace vf
