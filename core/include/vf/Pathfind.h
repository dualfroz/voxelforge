#pragma once

#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

struct PathConfig {
    int mobHeight = 2;
    int maxStepUp = 1;
    int maxDrop = 3;
    int maxExpansions = 40000;
};

bool isStandable(const World& world, const Vec3i& cell, const PathConfig& config = {});

std::vector<Vec3i> findPath(const World& world, const Vec3i& start, const Vec3i& goal,
                            const PathConfig& config = {});

}
