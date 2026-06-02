#pragma once

#include <cstdint>
#include <vector>
#include "Math.h"

namespace vf {

// Deterministic random-tick position selector. For a chunk coordinate and an
// absolute world tick it yields a repeatable list of local cell positions,
// seeded purely by (worldSeed, chunkCoord, tick, index). Two selectors built
// with the same seed produce identical streams, so the same simulation replays
// bit for bit. Draws are independent per index and may repeat, matching how a
// real per-chunk random tick samples with replacement.
class RandomTick {
public:
    RandomTick(uint32_t worldSeed = 1u, int ticksPerChunk = 3);

    int ticksPerChunk() const { return ticks; }
    uint32_t worldSeed() const { return seed; }

    void sampleLocal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const;
    std::vector<Vec3i> sampleLocal(const Vec3i& chunkCoord, long long tick) const;

    void sampleGlobal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const;
    std::vector<Vec3i> sampleGlobal(const Vec3i& chunkCoord, long long tick) const;

    // Raw 64-bit value for (chunkCoord, tick, index) under this seed. Callers can
    // roll their own seeded events off the same stream without touching layout.
    uint64_t hash(const Vec3i& chunkCoord, long long tick, uint32_t index) const;

private:
    uint32_t seed;
    int ticks;
};

} // namespace vf
