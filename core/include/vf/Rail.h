#pragma once

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "World.h"
#include "Circuit.h"
#include "Block.h"
#include "Math.h"

namespace vf {

constexpr int poweredRailReach = 8;

enum class RailDir : uint8_t {
    North,   // -Z
    South,   // +Z
    East,    // +X
    West,    // -X
};

enum class RailShape : uint8_t {
    NorthSouth,
    EastWest,
    AscendNorth,
    AscendSouth,
    AscendEast,
    AscendWest,
    CurveSouthEast,
    CurveSouthWest,
    CurveNorthWest,
    CurveNorthEast,
};

struct RailStep {
    Vec3i nextPos;
    RailDir exitDir = RailDir::North;
    bool valid = false;
};

inline bool isRailBlock(Block b) {
    return b == Block::Rail || b == Block::PoweredRail ||
           b == Block::DetectorRail || b == Block::ActivatorRail;
}

RailDir opposite(RailDir dir);
Vec3i railDelta(RailDir dir);
bool isAscending(RailShape shape);
RailDir ascendUpDir(RailShape shape);
std::pair<RailDir, RailDir> railEnds(RailShape shape);

// Auto-connects a rail to its neighbours. Curves are only produced for plain
// rails (Block::Rail); powered/detector/activator rails stay straight or
// ascending. When `switched` is true a straight-capable rail that also has a
// perpendicular neighbour takes the turn instead of going straight, modelling a
// powered junction.
RailShape inferRailShape(const World& world, const Vec3i& pos, bool switched = false);

// Follows the track one cell from `pos` given an entry heading. Handles curves
// (heading rotates) and slopes (steps up onto an ascending rail or down onto a
// descending one). `valid` is false when the track dead-ends: a derail.
RailStep advanceOnRail(const World& world, const Vec3i& pos, RailDir moveDir, bool switched = false);

// Set of powered-rail cells that end up energised: every rail directly powered
// by redstone plus connected powered rails within `poweredRailReach` along the
// straight line in each direction.
std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const std::function<bool(const Vec3i&)>& directlyPowered);

std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const RedstoneCircuit& circuit);

// Adjacency graph over a set of rail cells. Each node links to the rail cells
// its two ends connect to, following curves and slopes. Backs cart routing:
// distance queries, one-shot track tracing and connectivity checks.
class RailGraph {
public:
    void build(const World& world, const std::vector<Vec3i>& railCells, bool switched = false);

    bool contains(const Vec3i& pos) const { return adjacency.find(pos) != adjacency.end(); }
    std::vector<Vec3i> neighbours(const Vec3i& pos) const;
    std::size_t size() const { return adjacency.size(); }

    // Walks the track from `start` heading `dir`, returning the ordered cells it
    // visits (including `start`) until a derail, a revisit (closed loop) or
    // `maxSteps` cells.
    std::vector<Vec3i> trace(const World& world, const Vec3i& start, RailDir dir, int maxSteps) const;

    // Cell count of the shortest connection between two rails, or -1 when they
    // are not connected within the graph.
    int distance(const Vec3i& a, const Vec3i& b) const;

private:
    std::unordered_map<Vec3i, std::vector<Vec3i>, Vec3iHash> adjacency;
};

} // namespace vf
