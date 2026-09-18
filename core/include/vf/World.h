#pragma once

#include <unordered_map>
#include <memory>
#include "Chunk.h"
#include "Math.h"

namespace vf {

inline int floorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

inline int floorMod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

class World {
public:
    Chunk* chunkAt(const Vec3i& coord) {
        auto it = chunks.find(coord);
        return it == chunks.end() ? nullptr : it->second.get();
    }

    const Chunk* chunkAt(const Vec3i& coord) const {
        auto it = chunks.find(coord);
        return it == chunks.end() ? nullptr : it->second.get();
    }

    Chunk& ensureChunk(const Vec3i& coord) {
        auto it = chunks.find(coord);
        if (it != chunks.end()) return *it->second;
        auto inserted = chunks.emplace(coord, std::make_unique<Chunk>());
        return *inserted.first->second;
    }

    bool removeChunk(const Vec3i& coord) { return chunks.erase(coord) != 0; }

    Block blockAt(int gx, int gy, int gz) const {
        Vec3i coord{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
        const Chunk* chunk = chunkAt(coord);
        if (!chunk) return Block::Air;
        return chunk->at(floorMod(gx, chunkSize), floorMod(gy, chunkSize), floorMod(gz, chunkSize));
    }

    void setBlock(int gx, int gy, int gz, Block b) {
        Vec3i coord{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
        Chunk& chunk = ensureChunk(coord);
        chunk.set(floorMod(gx, chunkSize), floorMod(gy, chunkSize), floorMod(gz, chunkSize), b);
    }

    bool solidAt(int gx, int gy, int gz) const { return isSolid(blockAt(gx, gy, gz)); }

    const std::unordered_map<Vec3i, std::unique_ptr<Chunk>, Vec3iHash>& allChunks() const {
        return chunks;
    }

    std::size_t chunkCount() const { return chunks.size(); }

private:
    std::unordered_map<Vec3i, std::unique_ptr<Chunk>, Vec3iHash> chunks;
};

}
