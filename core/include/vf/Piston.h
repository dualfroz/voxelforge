#pragma once

#include <vector>
#include "World.h"
#include "Block.h"

namespace vf {

constexpr int pistonMaxPush = 12;

struct PistonMoveResult {
    bool moved = false;
    int blocksPushed = 0;
    std::vector<Vec3i> destroyed;
};

bool isPistonImmovable(Block b);
bool isPistonFragile(Block b);
bool isPistonBlock(Block b);

PistonMoveResult extendPiston(World& world, const Vec3i& pos, Face facing);
PistonMoveResult retractPiston(World& world, const Vec3i& pos, Face facing);

} // namespace vf
