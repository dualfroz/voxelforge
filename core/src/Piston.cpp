#include "vf/Piston.h"

namespace vf {

namespace {

Vec3i faceOffset(Face f) {
    switch (f) {
        case Face::PosX: return {1, 0, 0};
        case Face::NegX: return {-1, 0, 0};
        case Face::PosY: return {0, 1, 0};
        case Face::NegY: return {0, -1, 0};
        case Face::PosZ: return {0, 0, 1};
        case Face::NegZ: return {0, 0, -1};
    }
    return {0, 0, 0};
}

}

bool isPistonImmovable(Block b) { return b == Block::Bedrock || b == Block::Obsidian; }

bool isPistonFragile(Block b) {
    switch (b) {
        case Block::Torch:
        case Block::RedstoneTorch:
        case Block::RedstoneWire:
        case Block::Lever:
        case Block::Button:
            return true;
        default:
            return false;
    }
}

bool isPistonBlock(Block b) { return b == Block::Piston || b == Block::PistonSticky; }

PistonMoveResult extendPiston(World& world, const Vec3i& pos, Face facing) {
    PistonMoveResult result;
    if (!isPistonBlock(world.blockAt(pos.x, pos.y, pos.z))) return result;

    Vec3i offset = faceOffset(facing);
    Vec3i headPos = pos + offset;
    Vec3i cursor = headPos;
    std::vector<Vec3i> chain;
    Vec3i fragileAt{};
    bool hasFragile = false;

    for (int step = 0; step <= pistonMaxPush; ++step) {
        Block b = world.blockAt(cursor.x, cursor.y, cursor.z);
        if (b == Block::Air) break;
        if (isPistonImmovable(b)) return result;
        if (isPistonFragile(b)) {
            fragileAt = cursor;
            hasFragile = true;
            break;
        }
        if (static_cast<int>(chain.size()) == pistonMaxPush) return result;
        chain.push_back(cursor);
        cursor = cursor + offset;
    }

    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        Vec3i dest = *it + offset;
        Block moving = world.blockAt(it->x, it->y, it->z);
        world.setBlock(dest.x, dest.y, dest.z, moving);
    }
    if (hasFragile) {
        world.setBlock(fragileAt.x, fragileAt.y, fragileAt.z, Block::Air);
        result.destroyed.push_back(fragileAt);
    }
    world.setBlock(headPos.x, headPos.y, headPos.z, Block::PistonHead);

    result.moved = true;
    result.blocksPushed = static_cast<int>(chain.size());
    return result;
}

PistonMoveResult retractPiston(World& world, const Vec3i& pos, Face facing) {
    PistonMoveResult result;
    Block base = world.blockAt(pos.x, pos.y, pos.z);
    if (!isPistonBlock(base)) return result;

    Vec3i offset = faceOffset(facing);
    Vec3i headPos = pos + offset;
    if (world.blockAt(headPos.x, headPos.y, headPos.z) != Block::PistonHead) return result;

    world.setBlock(headPos.x, headPos.y, headPos.z, Block::Air);
    result.moved = true;

    if (base == Block::PistonSticky) {
        Vec3i pullPos = headPos + offset;
        Block pullBlock = world.blockAt(pullPos.x, pullPos.y, pullPos.z);
        if (pullBlock != Block::Air && !isPistonImmovable(pullBlock) && !isPistonFragile(pullBlock)) {
            world.setBlock(headPos.x, headPos.y, headPos.z, pullBlock);
            world.setBlock(pullPos.x, pullPos.y, pullPos.z, Block::Air);
            result.blocksPushed = 1;
        }
    }
    return result;
}

}
