#pragma once

#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

struct PathConfig {
    int mobHeight = 2;       // vertical voxels the walker occupies
    int maxStepUp = 1;       // how high a single step can climb
    int maxDrop = 3;         // how far a single step can fall
    int maxExpansions = 40000;   // A* node budget before giving up
};

// A cell is standable when the walker's body fits (non-solid) and there is solid
// ground directly beneath it.
bool isStandable(const World& world, const Vec3i& cell, const PathConfig& config = {});

// A* over standable voxel cells. Returns the cell path from start to goal
// inclusive, or an empty path when the goal is unreachable, not standable, or the
// search exceeds the node budget. Deterministic: ties break on a fixed ordering.
std::vector<Vec3i> findPath(const World& world, const Vec3i& start, const Vec3i& goal,
                            const PathConfig& config = {});

} // namespace vf
