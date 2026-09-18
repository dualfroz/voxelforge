#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace vf {

struct ColumnCoord {
    int x = 0;
    int z = 0;
    bool operator==(const ColumnCoord& o) const { return x == o.x && z == o.z; }
};

struct ColumnCoordHash {
    std::size_t operator()(const ColumnCoord& c) const {
        std::uint64_t h = 1469598103934665603ull;
        h ^= static_cast<std::uint32_t>(c.x); h *= 1099511628211ull;
        h ^= static_cast<std::uint32_t>(c.z); h *= 1099511628211ull;
        return static_cast<std::size_t>(h);
    }
};

struct StreamingConfig {
    int loadRadius = 8;
    int unloadRadius = 10;
};

struct StreamDelta {
    std::vector<ColumnCoord> load;
    std::vector<ColumnCoord> unload;
};

class ChunkStreamer {
public:
    explicit ChunkStreamer(StreamingConfig config = {}) : config(config) {}

    StreamDelta update(ColumnCoord centre);

    bool isLoaded(ColumnCoord column) const { return resident.count(column) != 0; }
    std::size_t loadedCount() const { return resident.size(); }
    const StreamingConfig& settings() const { return config; }

private:
    StreamingConfig config;
    std::unordered_set<ColumnCoord, ColumnCoordHash> resident;
};

std::vector<ColumnCoord> columnsInRadius(ColumnCoord centre, int radius);

}
