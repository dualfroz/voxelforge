#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace vf {

// A chunk column address (x, z); a column owns the full vertical stack of chunks.
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
    int loadRadius = 8;     // columns within this circle are kept resident
    int unloadRadius = 10;  // columns are only evicted past this larger circle
};

struct StreamDelta {
    std::vector<ColumnCoord> load;    // to generate, nearest-to-player first
    std::vector<ColumnCoord> unload;  // now beyond the unload ring
};

// Tracks which chunk columns are resident around a moving centre. The load and
// unload rings differ (hysteresis) so a player pacing across a boundary does not
// thrash the same columns in and out every step.
class ChunkStreamer {
public:
    explicit ChunkStreamer(StreamingConfig config = {}) : config(config) {}

    // Recomputes residency around `centre`, mutating the resident set and
    // returning the columns that entered (to load) and left (to unload).
    StreamDelta update(ColumnCoord centre);

    bool isLoaded(ColumnCoord column) const { return resident.count(column) != 0; }
    std::size_t loadedCount() const { return resident.size(); }
    const StreamingConfig& settings() const { return config; }

private:
    StreamingConfig config;
    std::unordered_set<ColumnCoord, ColumnCoordHash> resident;
};

// Columns whose centre lies within `radius` of (0,0), sorted nearest first.
// Distance is Euclidean so the loaded area is a disc, not a square.
std::vector<ColumnCoord> columnsInRadius(ColumnCoord centre, int radius);

} // namespace vf
