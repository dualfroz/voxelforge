#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "World.h"

namespace vf {

constexpr int regionSize = 16;  // chunk columns per region side (X and Z)

struct RegionCoord {
    int x = 0;
    int z = 0;
    bool operator==(const RegionCoord& o) const { return x == o.x && z == o.z; }
};

struct RegionCoordHash {
    std::size_t operator()(const RegionCoord& c) const {
        std::uint64_t h = 1469598103934665603ull;
        h ^= static_cast<std::uint32_t>(c.x); h *= 1099511628211ull;
        h ^= static_cast<std::uint32_t>(c.z); h *= 1099511628211ull;
        return static_cast<std::size_t>(h);
    }
};

RegionCoord regionForChunk(const Vec3i& chunkCoord);

// One region file: the chunks whose columns fall inside a regionSize x regionSize
// tile, across all Y. Each chunk is stored run-length encoded; the header carries
// an index so a single chunk can be pulled back without decoding the rest.
class Region {
public:
    explicit Region(RegionCoord coord = {}) : coord(coord) {}

    RegionCoord location() const { return coord; }
    std::size_t chunkCount() const { return chunks.size(); }
    bool hasChunk(const Vec3i& chunkCoord) const { return chunks.count(chunkCoord) != 0; }

    void putChunk(const Vec3i& chunkCoord, const Chunk& chunk);
    bool getChunk(const Vec3i& chunkCoord, Chunk& out) const;

    const std::unordered_map<Vec3i, Chunk, Vec3iHash>& allChunks() const { return chunks; }

    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* data, std::size_t size, Region& out);

private:
    RegionCoord coord;
    std::unordered_map<Vec3i, Chunk, Vec3iHash> chunks;
};

// Splits a whole world into its regions.
std::unordered_map<RegionCoord, Region, RegionCoordHash> splitWorldIntoRegions(const World& world);

// Convenience for the browser demo: one blob holding every non-empty region.
std::vector<uint8_t> serializeRegions(const World& world);
bool deserializeRegions(const uint8_t* data, std::size_t size, World& out);

} // namespace vf
