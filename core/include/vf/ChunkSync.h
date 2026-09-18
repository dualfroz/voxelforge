#pragma once

#include <cstddef>
#include <set>
#include <unordered_map>
#include "Chunk.h"
#include "Math.h"
#include "Protocol.h"
#include "World.h"

namespace vf {

class ServerChunkSync {
public:
    void markDirty(const Vec3i& chunkCoord, int x, int y, int z);
    void markDirtyIndex(const Vec3i& chunkCoord, int localIndex);

    std::size_t dirtyCount(const Vec3i& chunkCoord) const;
    bool anyDirty() const;
    void clear(const Vec3i& chunkCoord);
    void clearAll() { dirty.clear(); }

    bool buildFull(const World& world, const Vec3i& chunkCoord, ChunkData& out) const;

    bool buildDelta(const World& world, const Vec3i& chunkCoord, MultiBlockChange& out) const;

    bool shouldResendFull(const Vec3i& chunkCoord, std::size_t threshold) const;

private:
    std::unordered_map<Vec3i, std::set<int>, Vec3iHash> dirty;
};

class ClientChunkSync {
public:
    void applyFull(const ChunkData& packet, World& world);
    void applyDelta(const MultiBlockChange& packet, World& world);
    void applyBlockChange(const BlockChange& packet, World& world);
};

}
