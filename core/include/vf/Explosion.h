#pragma once

#include <unordered_map>
#include <vector>
#include "World.h"
#include "Block.h"
#include "Math.h"
#include "BlockTick.h"

namespace vf {

struct ExplosionConfig {
    float power = 4.0f;
    int rayGridSize = 6;
};

struct BlockDrop {
    Vec3i pos;
    Block block;
};

struct ExplosionResult {
    std::vector<Vec3i> destroyedBlocks;
    std::vector<BlockDrop> drops;
    std::vector<Vec3i> ignitedTnt;
};

float blastResistance(Block b);
Block dropForBlock(Block b);

ExplosionResult explode(World& world, const Vec3i& center, const ExplosionConfig& config = {});

Vec3 explosionKnockback(const Vec3i& center, const Vec3& entityPos, float power);

void igniteTnt(BlockTickScheduler& scheduler, const Vec3i& pos, int fuseTicks = 4);

}
