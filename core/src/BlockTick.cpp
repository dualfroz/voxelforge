#include "vf/BlockTick.h"

#include <algorithm>

namespace vf {

BlockTickScheduler::BlockTickScheduler(uint32_t seed, int randomTicksPerChunk)
    : rngState(seed == 0 ? 1u : seed), randomTicksPerChunk(randomTicksPerChunk) {}

uint32_t BlockTickScheduler::nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

void BlockTickScheduler::schedule(const Vec3i& pos, int delayTicks) {
    int delay = delayTicks < 1 ? 1 : delayTicks;
    pending.push_back({pos, tickCounter + delay, sequenceCounter++});
}

void BlockTickScheduler::cancel(const Vec3i& pos) {
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [&pos](const ScheduledTick& t) { return t.pos == pos; }),
                 pending.end());
}

bool BlockTickScheduler::isScheduled(const Vec3i& pos) const {
    for (const ScheduledTick& t : pending)
        if (t.pos == pos) return true;
    return false;
}

void BlockTickScheduler::fireScheduled(std::vector<Vec3i>& fired) {
    std::vector<ScheduledTick> due;
    std::vector<ScheduledTick> remaining;
    for (const ScheduledTick& t : pending) {
        if (t.dueTick <= tickCounter) due.push_back(t);
        else remaining.push_back(t);
    }
    std::sort(due.begin(), due.end(), [](const ScheduledTick& a, const ScheduledTick& b) {
        if (a.dueTick != b.dueTick) return a.dueTick < b.dueTick;
        return a.sequence < b.sequence;
    });
    pending.swap(remaining);
    for (const ScheduledTick& t : due) fired.push_back(t.pos);
}

namespace {

std::vector<Vec3i> sortedChunkCoords(const World& world) {
    std::vector<Vec3i> coords;
    coords.reserve(world.allChunks().size());
    for (const auto& entry : world.allChunks()) coords.push_back(entry.first);
    std::sort(coords.begin(), coords.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.z != b.z) return a.z < b.z;
        return a.y < b.y;
    });
    return coords;
}

}

void BlockTickScheduler::sampleRandomTicks(World& world) {
    randomTickPositions.clear();
    std::vector<Vec3i> chunkCoords = sortedChunkCoords(world);

    for (const Vec3i& chunkCoord : chunkCoords) {
        for (int i = 0; i < randomTicksPerChunk; ++i) {
            int lx = static_cast<int>(nextRandom() % chunkSize);
            int ly = static_cast<int>(nextRandom() % chunkSize);
            int lz = static_cast<int>(nextRandom() % chunkSize);
            Vec3i pos{chunkCoord.x * chunkSize + lx, chunkCoord.y * chunkSize + ly,
                     chunkCoord.z * chunkSize + lz};
            randomTickPositions.push_back(pos);
            if (randomTickHook) randomTickHook(world, pos, world.blockAt(pos.x, pos.y, pos.z));
        }
    }
}

void BlockTickScheduler::applyGravity(World& world) {
    std::vector<Vec3i> chunkCoords = sortedChunkCoords(world);

    for (const Vec3i& chunkCoord : chunkCoords) {
        for (int ly = 0; ly < chunkSize; ++ly) {
            for (int lz = 0; lz < chunkSize; ++lz) {
                for (int lx = 0; lx < chunkSize; ++lx) {
                    int gx = chunkCoord.x * chunkSize + lx;
                    int gy = chunkCoord.y * chunkSize + ly;
                    int gz = chunkCoord.z * chunkSize + lz;
                    Block b = world.blockAt(gx, gy, gz);
                    if (!isFallingBlock(b)) continue;
                    if (world.blockAt(gx, gy - 1, gz) != Block::Air) continue;
                    world.setBlock(gx, gy, gz, Block::Air);
                    world.setBlock(gx, gy - 1, gz, b);
                }
            }
        }
    }
}

std::vector<Vec3i> BlockTickScheduler::tick(World& world) {
    ++tickCounter;
    std::vector<Vec3i> fired;
    fireScheduled(fired);
    sampleRandomTicks(world);
    applyGravity(world);
    return fired;
}

}
