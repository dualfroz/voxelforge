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
    North,
    South,
    East,
    West,
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

RailShape inferRailShape(const World& world, const Vec3i& pos, bool switched = false);

RailStep advanceOnRail(const World& world, const Vec3i& pos, RailDir moveDir, bool switched = false);

std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const std::function<bool(const Vec3i&)>& directlyPowered);

std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const RedstoneCircuit& circuit);

class RailGraph {
public:
    void build(const World& world, const std::vector<Vec3i>& railCells, bool switched = false);

    bool contains(const Vec3i& pos) const { return adjacency.find(pos) != adjacency.end(); }
    std::vector<Vec3i> neighbours(const Vec3i& pos) const;
    std::size_t size() const { return adjacency.size(); }

    std::vector<Vec3i> trace(const World& world, const Vec3i& start, RailDir dir, int maxSteps) const;

    int distance(const Vec3i& a, const Vec3i& b) const;

private:
    std::unordered_map<Vec3i, std::vector<Vec3i>, Vec3iHash> adjacency;
};

}
