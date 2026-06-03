#include "vf/ChunkSync.h"

namespace vf {

void ServerChunkSync::markDirty(const Vec3i& chunkCoord, int x, int y, int z) {
    dirty[chunkCoord].insert(localIndex(x, y, z));
}

void ServerChunkSync::markDirtyIndex(const Vec3i& chunkCoord, int localIndex) {
    dirty[chunkCoord].insert(localIndex);
}

std::size_t ServerChunkSync::dirtyCount(const Vec3i& chunkCoord) const {
    auto it = dirty.find(chunkCoord);
    return it == dirty.end() ? 0 : it->second.size();
}

bool ServerChunkSync::anyDirty() const {
    for (const auto& entry : dirty)
        if (!entry.second.empty()) return true;
    return false;
}

void ServerChunkSync::clear(const Vec3i& chunkCoord) { dirty.erase(chunkCoord); }

bool ServerChunkSync::buildFull(const World& world, const Vec3i& chunkCoord, ChunkData& out) const {
    const Chunk* chunk = world.chunkAt(chunkCoord);
    if (!chunk) return false;
    out.setFromChunk(chunkCoord.x, chunkCoord.y, chunkCoord.z, *chunk);
    return true;
}

bool ServerChunkSync::buildDelta(const World& world, const Vec3i& chunkCoord,
                                 MultiBlockChange& out) const {
    auto it = dirty.find(chunkCoord);
    if (it == dirty.end() || it->second.empty()) return false;
    const Chunk* chunk = world.chunkAt(chunkCoord);
    if (!chunk) return false;

    out.chunkX = chunkCoord.x;
    out.chunkY = chunkCoord.y;
    out.chunkZ = chunkCoord.z;
    out.changes.clear();
    out.changes.reserve(it->second.size());
    const std::array<Block, chunkVolume>& blocks = chunk->raw();
    for (int index : it->second)
        out.changes.push_back({static_cast<uint16_t>(index), blocks[static_cast<std::size_t>(index)]});
    return true;
}

bool ServerChunkSync::shouldResendFull(const Vec3i& chunkCoord, std::size_t threshold) const {
    return dirtyCount(chunkCoord) >= threshold;
}

void ClientChunkSync::applyFull(const ChunkData& packet, World& world) {
    Chunk& chunk = world.ensureChunk({packet.chunkX, packet.chunkY, packet.chunkZ});
    packet.toChunk(chunk);
}

void ClientChunkSync::applyDelta(const MultiBlockChange& packet, World& world) {
    Chunk& chunk = world.ensureChunk({packet.chunkX, packet.chunkY, packet.chunkZ});
    std::array<Block, chunkVolume>& blocks = chunk.raw();
    for (const MultiBlockChange::Change& change : packet.changes)
        blocks[change.localIndex] = change.block;
}

void ClientChunkSync::applyBlockChange(const BlockChange& packet, World& world) {
    world.setBlock(packet.x, packet.y, packet.z, packet.block);
}

} // namespace vf
