#pragma once

#include <array>
#include "Block.h"

namespace vf {

constexpr int chunkSize = 16;
constexpr int chunkVolume = chunkSize * chunkSize * chunkSize;

inline bool inChunkBounds(int x, int y, int z) {
    return x >= 0 && x < chunkSize && y >= 0 && y < chunkSize && z >= 0 && z < chunkSize;
}

inline int localIndex(int x, int y, int z) {
    return (y * chunkSize + z) * chunkSize + x;
}

class Chunk {
public:
    Chunk() { blocks.fill(Block::Air); }

    Block at(int x, int y, int z) const { return blocks[localIndex(x, y, z)]; }
    void set(int x, int y, int z, Block b) { blocks[localIndex(x, y, z)] = b; }

    bool empty() const {
        for (Block b : blocks)
            if (b != Block::Air) return false;
        return true;
    }

    const std::array<Block, chunkVolume>& raw() const { return blocks; }
    std::array<Block, chunkVolume>& raw() { return blocks; }

private:
    std::array<Block, chunkVolume> blocks;
};

}
