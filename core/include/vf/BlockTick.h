#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "World.h"
#include "Block.h"

namespace vf {

struct ScheduledTick {
    Vec3i pos;
    int dueTick;
    uint32_t sequence;
};

inline bool isFallingBlock(Block b) { return b == Block::Sand || b == Block::Gravel; }

inline bool tryGrowGrass(World& world, const Vec3i& pos) {
    if (world.blockAt(pos.x, pos.y, pos.z) != Block::Dirt) return false;
    if (world.solidAt(pos.x, pos.y + 1, pos.z)) return false;
    world.setBlock(pos.x, pos.y, pos.z, Block::Grass);
    return true;
}

using RandomTickHook = std::function<void(World&, const Vec3i&, Block)>;

class BlockTickScheduler {
public:
    explicit BlockTickScheduler(uint32_t seed = 1u, int randomTicksPerChunk = 3);

    void schedule(const Vec3i& pos, int delayTicks);
    void cancel(const Vec3i& pos);
    bool isScheduled(const Vec3i& pos) const;

    void setRandomTickHook(RandomTickHook hook) { randomTickHook = std::move(hook); }

    std::vector<Vec3i> tick(World& world);

    const std::vector<Vec3i>& lastRandomTicks() const { return randomTickPositions; }
    int currentTick() const { return tickCounter; }
    std::size_t pendingCount() const { return pending.size(); }

private:
    uint32_t nextRandom();
    void fireScheduled(std::vector<Vec3i>& fired);
    void sampleRandomTicks(World& world);
    void applyGravity(World& world);

    int tickCounter = 0;
    uint32_t rngState;
    int randomTicksPerChunk;
    uint32_t sequenceCounter = 0;
    std::vector<ScheduledTick> pending;
    std::vector<Vec3i> randomTickPositions;
    RandomTickHook randomTickHook;
};

}
