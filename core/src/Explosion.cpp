#include "vf/Explosion.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

int floorToInt(float v) { return static_cast<int>(std::floor(v)); }

constexpr float rayStepLength = 0.3f;
constexpr int rayMaxSteps = 128;

std::vector<Vec3> rayDirections(int gridSizeIn) {
    int gridSize = gridSizeIn < 1 ? 1 : gridSizeIn;
    std::vector<Vec3> dirs;
    dirs.reserve(static_cast<std::size_t>((gridSize + 1) * (gridSize + 1) * 6));
    float step = 2.0f / static_cast<float>(gridSize);

    for (int axis = 0; axis < 3; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
            for (int i = 0; i <= gridSize; ++i) {
                for (int j = 0; j <= gridSize; ++j) {
                    float a = -1.0f + step * static_cast<float>(i);
                    float b = -1.0f + step * static_cast<float>(j);
                    Vec3 point;
                    if (axis == 0) point = Vec3(static_cast<float>(sign), a, b);
                    else if (axis == 1) point = Vec3(a, static_cast<float>(sign), b);
                    else point = Vec3(a, b, static_cast<float>(sign));
                    dirs.push_back(normalize(point));
                }
            }
        }
    }
    return dirs;
}

}

float blastResistance(Block b) {
    switch (b) {
        case Block::Air:
            return 0.0f;
        case Block::Water:
        case Block::Lava:
            return 500.0f;
        case Block::Bedrock:
            return 3600.0f;
        case Block::Obsidian:
            return 1200.0f;
        case Block::Tnt:
            return 0.0f;
        case Block::Dirt:
        case Block::Grass:
        case Block::Sand:
        case Block::Gravel:
        case Block::Snow:
        case Block::Ice:
            return 0.5f;
        case Block::Wood:
        case Block::Planks:
        case Block::Leaves:
        case Block::Ladder:
        case Block::Torch:
        case Block::RedstoneWire:
        case Block::RedstoneTorch:
        case Block::Lever:
        case Block::Button:
        case Block::Repeater:
        case Block::Comparator:
            return 1.0f;
        default:
            return 6.0f;
    }
}

Block dropForBlock(Block b) {
    switch (b) {
        case Block::Grass:
            return Block::Dirt;
        case Block::FurnaceLit:
            return Block::Furnace;
        case Block::RedstoneLampLit:
            return Block::RedstoneLamp;
        case Block::Leaves:
        case Block::Ice:
        case Block::Torch:
        case Block::RedstoneWire:
        case Block::RedstoneTorch:
        case Block::Lever:
        case Block::Button:
        case Block::Repeater:
        case Block::Comparator:
        case Block::Ladder:
            return Block::Air;
        default:
            return b;
    }
}

ExplosionResult explode(World& world, const Vec3i& center, const ExplosionConfig& config) {
    ExplosionResult result;
    Vec3 originF(static_cast<float>(center.x) + 0.5f, static_cast<float>(center.y) + 0.5f,
                static_cast<float>(center.z) + 0.5f);
    std::vector<Vec3> dirs = rayDirections(config.rayGridSize);

    std::unordered_map<Vec3i, float, Vec3iHash> hitEnergy;

    for (const Vec3& dir : dirs) {
        Vec3 pos = originF;
        float remaining = config.power;
        for (int step = 0; step < rayMaxSteps; ++step) {
            pos = pos + dir * rayStepLength;
            Vec3i cell{floorToInt(pos.x), floorToInt(pos.y), floorToInt(pos.z)};
            Block b = world.blockAt(cell.x, cell.y, cell.z);
            float resistance = blastResistance(b);
            remaining -= (resistance / 5.0f + 0.3f) * rayStepLength;
            if (remaining <= 0.0f) break;
            if (b == Block::Air) continue;

            auto it = hitEnergy.find(cell);
            if (it == hitEnergy.end() || remaining > it->second) hitEnergy[cell] = remaining;
        }
    }

    std::vector<Vec3i> touchedCells;
    touchedCells.reserve(hitEnergy.size());
    for (const auto& entry : hitEnergy) touchedCells.push_back(entry.first);
    std::sort(touchedCells.begin(), touchedCells.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });

    for (const Vec3i& cell : touchedCells) {
        float strength = hitEnergy[cell];
        Block b = world.blockAt(cell.x, cell.y, cell.z);
        if (b == Block::Air) continue;

        if (b == Block::Tnt && cell != center) {
            result.ignitedTnt.push_back(cell);
            continue;
        }

        world.setBlock(cell.x, cell.y, cell.z, Block::Air);
        result.destroyedBlocks.push_back(cell);

        bool vaporized = strength > config.power * 0.5f;
        if (!vaporized) {
            Block dropped = dropForBlock(b);
            if (dropped != Block::Air) result.drops.push_back({cell, dropped});
        }
    }

    return result;
}

Vec3 explosionKnockback(const Vec3i& center, const Vec3& entityPos, float power) {
    Vec3 centerF(static_cast<float>(center.x) + 0.5f, static_cast<float>(center.y) + 0.5f,
                static_cast<float>(center.z) + 0.5f);
    Vec3 delta = entityPos - centerF;
    float dist = length(delta);
    if (dist < 1e-4f) return Vec3(0.0f, 0.0f, 0.0f);

    float range = power * 2.0f;
    float falloff = 1.0f - dist / range;
    if (falloff <= 0.0f) return Vec3(0.0f, 0.0f, 0.0f);

    Vec3 dir = normalize(delta);
    float strength = falloff * range;
    return dir * strength;
}

void igniteTnt(BlockTickScheduler& scheduler, const Vec3i& pos, int fuseTicks) {
    scheduler.schedule(pos, fuseTicks);
}

}
