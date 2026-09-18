#include "vf/Circuit.h"

#include <algorithm>

namespace vf {

namespace {

const Vec3i neighbourOffsets[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

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

Face oppositeFace(Face f) {
    switch (f) {
        case Face::PosX: return Face::NegX;
        case Face::NegX: return Face::PosX;
        case Face::PosY: return Face::NegY;
        case Face::NegY: return Face::PosY;
        case Face::PosZ: return Face::NegZ;
        case Face::NegZ: return Face::PosZ;
    }
    return Face::PosX;
}

void sideFaces(Face facing, Face& sideA, Face& sideB) {
    switch (facing) {
        case Face::PosX:
        case Face::NegX:
            sideA = Face::PosZ;
            sideB = Face::NegZ;
            return;
        case Face::PosZ:
        case Face::NegZ:
            sideA = Face::PosX;
            sideB = Face::NegX;
            return;
        case Face::PosY:
        case Face::NegY:
            sideA = Face::PosX;
            sideB = Face::NegX;
            return;
    }
    sideA = Face::PosX;
    sideB = Face::NegX;
}

int clampDelay(int delayTicks) {
    if (delayTicks < 1) return 1;
    if (delayTicks > 4) return 4;
    return delayTicks;
}

}

RedstoneCircuit::RedstoneCircuit(World& world, const Vec3i& origin, const Vec3i& dims)
    : world(world), origin(origin), dims(dims) {
    std::size_t cells = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
    power.assign(cells, 0);
    previousPower.assign(cells, 0);
}

bool RedstoneCircuit::inBox(int lx, int ly, int lz) const {
    return lx >= 0 && lx < dims.x && ly >= 0 && ly < dims.y && lz >= 0 && lz < dims.z;
}

bool RedstoneCircuit::toLocal(int gx, int gy, int gz, int& lx, int& ly, int& lz) const {
    lx = gx - origin.x;
    ly = gy - origin.y;
    lz = gz - origin.z;
    return inBox(lx, ly, lz);
}

int RedstoneCircuit::fieldAt(const std::vector<uint8_t>& field, int gx, int gy, int gz) const {
    int lx, ly, lz;
    if (!toLocal(gx, gy, gz, lx, ly, lz)) return 0;
    return field[index(lx, ly, lz)];
}

void RedstoneCircuit::setLever(const Vec3i& pos, bool on) { leverStates[pos] = on; }

bool RedstoneCircuit::leverOn(const Vec3i& pos) const {
    auto it = leverStates.find(pos);
    return it != leverStates.end() && it->second;
}

void RedstoneCircuit::pressButton(const Vec3i& pos, int durationTicks) {
    buttonTimers[pos] = durationTicks;
}

bool RedstoneCircuit::buttonActive(const Vec3i& pos) const {
    auto it = buttonTimers.find(pos);
    return it != buttonTimers.end() && it->second > 0;
}

void RedstoneCircuit::setRepeater(const Vec3i& pos, Face facing, int delayTicks) {
    RepeaterState state;
    state.facing = facing;
    state.delayTicks = clampDelay(delayTicks);
    repeaters[pos] = state;
}

void RedstoneCircuit::setComparator(const Vec3i& pos, Face facing, ComparatorMode mode) {
    ComparatorState state;
    state.facing = facing;
    state.mode = mode;
    comparators[pos] = state;
}

bool RedstoneCircuit::repeaterOutputOn(const Vec3i& pos) const {
    auto it = repeaters.find(pos);
    return it != repeaters.end() && it->second.outputOn;
}

void RedstoneCircuit::advanceComponents() {
    for (auto& entry : buttonTimers)
        if (entry.second > 0) --entry.second;

    for (auto& entry : repeaters) {
        RepeaterState& state = entry.second;
        Vec3i backPos = entry.first + faceOffset(oppositeFace(state.facing));
        bool input = fieldAt(power, backPos.x, backPos.y, backPos.z) > 0;
        if (input != state.lastInput) {
            state.lastInput = input;
            state.pendingOutput = input;
            state.countdown = state.delayTicks;
        }
        if (state.countdown > 0) {
            --state.countdown;
            if (state.countdown == 0) state.outputOn = state.pendingOutput;
        }
    }
}

int RedstoneCircuit::comparatorOutput(const Vec3i& pos, const ComparatorState& state) const {
    Vec3i backPos = pos + faceOffset(oppositeFace(state.facing));
    int backLevel = fieldAt(previousPower, backPos.x, backPos.y, backPos.z);

    Face sideA, sideB;
    sideFaces(state.facing, sideA, sideB);
    Vec3i sideAPos = pos + faceOffset(sideA);
    Vec3i sideBPos = pos + faceOffset(sideB);
    int sideLevel = std::max(fieldAt(previousPower, sideAPos.x, sideAPos.y, sideAPos.z),
                              fieldAt(previousPower, sideBPos.x, sideBPos.y, sideBPos.z));

    if (state.mode == ComparatorMode::Compare) return backLevel >= sideLevel ? backLevel : 0;
    return backLevel > sideLevel ? backLevel - sideLevel : 0;
}

void RedstoneCircuit::injectOmni(const Vec3i& pos, int level, std::vector<Vec3i>& wireQueue) {
    injectOmniExcept(pos, level, Vec3i{0, 0, 0}, wireQueue);
}

void RedstoneCircuit::injectOmniExcept(const Vec3i& pos, int level, const Vec3i& excludeOffset,
                                       std::vector<Vec3i>& wireQueue) {
    if (level <= 0) return;
    int lx, ly, lz;
    if (toLocal(pos.x, pos.y, pos.z, lx, ly, lz)) {
        int idx = index(lx, ly, lz);
        if (level > power[idx]) power[idx] = static_cast<uint8_t>(level);
    }
    for (const Vec3i& off : neighbourOffsets) {
        if (off == excludeOffset) continue;
        Vec3i target = pos + off;
        int tlx, tly, tlz;
        if (!toLocal(target.x, target.y, target.z, tlx, tly, tlz)) continue;
        int tidx = index(tlx, tly, tlz);
        if (level <= power[tidx]) continue;
        power[tidx] = static_cast<uint8_t>(level);
        if (world.blockAt(target.x, target.y, target.z) == Block::RedstoneWire)
            wireQueue.push_back(target);
    }
}

void RedstoneCircuit::injectDirectional(const Vec3i& pos, Face facing, int level,
                                        std::vector<Vec3i>& wireQueue) {
    if (level <= 0) return;
    int lx, ly, lz;
    if (toLocal(pos.x, pos.y, pos.z, lx, ly, lz)) {
        int idx = index(lx, ly, lz);
        if (level > power[idx]) power[idx] = static_cast<uint8_t>(level);
    }
    Vec3i target = pos + faceOffset(facing);
    int tlx, tly, tlz;
    if (!toLocal(target.x, target.y, target.z, tlx, tly, tlz)) return;
    int tidx = index(tlx, tly, tlz);
    if (level <= power[tidx]) return;
    power[tidx] = static_cast<uint8_t>(level);
    if (world.blockAt(target.x, target.y, target.z) == Block::RedstoneWire)
        wireQueue.push_back(target);
}

void RedstoneCircuit::propagateWire(std::vector<Vec3i>& wireQueue) {
    while (!wireQueue.empty()) {
        Vec3i cur = wireQueue.back();
        wireQueue.pop_back();
        int lx = cur.x - origin.x, ly = cur.y - origin.y, lz = cur.z - origin.z;
        int curLevel = power[index(lx, ly, lz)];
        int next = curLevel > 0 ? curLevel - 1 : 0;
        if (next <= 0) continue;

        for (const Vec3i& off : neighbourOffsets) {
            Vec3i nb = cur + off;
            int nlx, nly, nlz;
            if (!toLocal(nb.x, nb.y, nb.z, nlx, nly, nlz)) continue;
            if (world.blockAt(nb.x, nb.y, nb.z) != Block::RedstoneWire) continue;
            int nidx = index(nlx, nly, nlz);
            if (next <= power[nidx]) continue;
            power[nidx] = static_cast<uint8_t>(next);
            wireQueue.push_back(nb);
        }
    }
}

void RedstoneCircuit::resolvePowerField() {
    previousPower = power;
    std::fill(power.begin(), power.end(), 0);
    std::vector<Vec3i> wireQueue;

    for (int lx = 0; lx < dims.x; ++lx) {
        for (int ly = 0; ly < dims.y; ++ly) {
            for (int lz = 0; lz < dims.z; ++lz) {
                Vec3i pos{origin.x + lx, origin.y + ly, origin.z + lz};
                Block b = world.blockAt(pos.x, pos.y, pos.z);
                switch (b) {
                    case Block::RedstoneBlock:
                        injectOmni(pos, redstoneMaxPower, wireQueue);
                        break;
                    case Block::Lever:
                        if (leverOn(pos)) injectOmni(pos, redstoneMaxPower, wireQueue);
                        break;
                    case Block::Button:
                        if (buttonActive(pos)) injectOmni(pos, redstoneMaxPower, wireQueue);
                        break;
                    case Block::RedstoneTorch: {
                        Vec3i mount{pos.x, pos.y - 1, pos.z};
                        int mountPower = fieldAt(previousPower, mount.x, mount.y, mount.z);
                        if (mountPower <= 0)
                            injectOmniExcept(pos, redstoneMaxPower, Vec3i{0, -1, 0}, wireQueue);
                        break;
                    }
                    case Block::Repeater: {
                        auto it = repeaters.find(pos);
                        if (it != repeaters.end() && it->second.outputOn)
                            injectDirectional(pos, it->second.facing, redstoneMaxPower, wireQueue);
                        break;
                    }
                    case Block::Comparator: {
                        auto it = comparators.find(pos);
                        if (it != comparators.end()) {
                            int level = comparatorOutput(pos, it->second);
                            if (level > 0) injectDirectional(pos, it->second.facing, level, wireQueue);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    propagateWire(wireQueue);
}

void RedstoneCircuit::tick() {
    ++tickCounter;
    advanceComponents();
    resolvePowerField();
}

int RedstoneCircuit::powerAt(int gx, int gy, int gz) const { return fieldAt(power, gx, gy, gz); }

void RedstoneCircuit::applyLamps() {
    for (int lx = 0; lx < dims.x; ++lx) {
        for (int ly = 0; ly < dims.y; ++ly) {
            for (int lz = 0; lz < dims.z; ++lz) {
                int gx = origin.x + lx, gy = origin.y + ly, gz = origin.z + lz;
                Block b = world.blockAt(gx, gy, gz);
                if (b != Block::RedstoneLamp && b != Block::RedstoneLampLit) continue;

                bool lit = false;
                for (const Vec3i& off : neighbourOffsets) {
                    if (fieldAt(power, gx + off.x, gy + off.y, gz + off.z) > 0) {
                        lit = true;
                        break;
                    }
                }
                Block want = lit ? Block::RedstoneLampLit : Block::RedstoneLamp;
                if (b != want) world.setBlock(gx, gy, gz, want);
            }
        }
    }
}

}
