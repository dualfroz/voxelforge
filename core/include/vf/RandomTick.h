#pragma once

#include <cstdint>
#include <vector>
#include "Math.h"

namespace vf {

class RandomTick {
public:
    RandomTick(uint32_t worldSeed = 1u, int ticksPerChunk = 3);

    int ticksPerChunk() const { return ticks; }
    uint32_t worldSeed() const { return seed; }

    void sampleLocal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const;
    std::vector<Vec3i> sampleLocal(const Vec3i& chunkCoord, long long tick) const;

    void sampleGlobal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const;
    std::vector<Vec3i> sampleGlobal(const Vec3i& chunkCoord, long long tick) const;

    uint64_t hash(const Vec3i& chunkCoord, long long tick, uint32_t index) const;

private:
    uint32_t seed;
    int ticks;
};

}
