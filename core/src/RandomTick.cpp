#include "vf/RandomTick.h"
#include "vf/Chunk.h"

namespace vf {

namespace {

uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

uint64_t fold(uint64_t acc, uint64_t value) {
    return mix64(acc ^ (mix64(value) + 0x9e3779b97f4a7c15ull + (acc << 6) + (acc >> 2)));
}

uint64_t asBits(int v) {
    return static_cast<uint64_t>(static_cast<uint32_t>(v));
}

} // namespace

RandomTick::RandomTick(uint32_t worldSeed, int ticksPerChunk)
    : seed(worldSeed), ticks(ticksPerChunk < 0 ? 0 : ticksPerChunk) {}

uint64_t RandomTick::hash(const Vec3i& chunkCoord, long long tick, uint32_t index) const {
    uint64_t h = mix64(static_cast<uint64_t>(seed) + 0x100000001b3ull);
    h = fold(h, asBits(chunkCoord.x));
    h = fold(h, asBits(chunkCoord.y));
    h = fold(h, asBits(chunkCoord.z));
    h = fold(h, static_cast<uint64_t>(tick));
    h = fold(h, static_cast<uint64_t>(index));
    return h;
}

void RandomTick::sampleLocal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const {
    out.clear();
    out.reserve(static_cast<std::size_t>(ticks));
    for (int i = 0; i < ticks; ++i) {
        uint64_t h = hash(chunkCoord, tick, static_cast<uint32_t>(i));
        int x = static_cast<int>((h & 0xffffull) % static_cast<uint64_t>(chunkSize));
        int y = static_cast<int>(((h >> 21) & 0xffffull) % static_cast<uint64_t>(chunkSize));
        int z = static_cast<int>(((h >> 42) & 0xffffull) % static_cast<uint64_t>(chunkSize));
        out.push_back(Vec3i{x, y, z});
    }
}

std::vector<Vec3i> RandomTick::sampleLocal(const Vec3i& chunkCoord, long long tick) const {
    std::vector<Vec3i> out;
    sampleLocal(chunkCoord, tick, out);
    return out;
}

void RandomTick::sampleGlobal(const Vec3i& chunkCoord, long long tick, std::vector<Vec3i>& out) const {
    sampleLocal(chunkCoord, tick, out);
    for (Vec3i& cell : out) {
        cell.x += chunkCoord.x * chunkSize;
        cell.y += chunkCoord.y * chunkSize;
        cell.z += chunkCoord.z * chunkSize;
    }
}

std::vector<Vec3i> RandomTick::sampleGlobal(const Vec3i& chunkCoord, long long tick) const {
    std::vector<Vec3i> out;
    sampleGlobal(chunkCoord, tick, out);
    return out;
}

} // namespace vf
