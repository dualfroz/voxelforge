#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "World.h"
#include "Block.h"

namespace vf {

constexpr int redstoneMaxPower = 15;

enum class ComparatorMode : uint8_t {
    Compare = 0,
    Subtract = 1,
};

struct RepeaterState {
    Face facing = Face::PosX;
    int delayTicks = 1;
    bool lastInput = false;
    int countdown = -1;
    bool pendingOutput = false;
    bool outputOn = false;
};

struct ComparatorState {
    Face facing = Face::PosX;
    ComparatorMode mode = ComparatorMode::Compare;
};

class RedstoneCircuit {
public:
    RedstoneCircuit(World& world, const Vec3i& origin, const Vec3i& dims);

    void setLever(const Vec3i& pos, bool on);
    bool leverOn(const Vec3i& pos) const;

    void pressButton(const Vec3i& pos, int durationTicks = 10);
    bool buttonActive(const Vec3i& pos) const;

    void setRepeater(const Vec3i& pos, Face facing, int delayTicks);
    void setComparator(const Vec3i& pos, Face facing, ComparatorMode mode);
    bool repeaterOutputOn(const Vec3i& pos) const;

    void tick();

    int powerAt(int gx, int gy, int gz) const;
    bool poweredAt(int gx, int gy, int gz) const { return powerAt(gx, gy, gz) > 0; }

    void applyLamps();

    const Vec3i& regionOrigin() const { return origin; }
    const Vec3i& regionDims() const { return dims; }
    int currentTick() const { return tickCounter; }

private:
    int index(int lx, int ly, int lz) const { return (ly * dims.z + lz) * dims.x + lx; }
    bool inBox(int lx, int ly, int lz) const;
    bool toLocal(int gx, int gy, int gz, int& lx, int& ly, int& lz) const;
    int fieldAt(const std::vector<uint8_t>& field, int gx, int gy, int gz) const;

    void advanceComponents();
    void resolvePowerField();
    void injectOmni(const Vec3i& pos, int level, std::vector<Vec3i>& wireQueue);
    void injectOmniExcept(const Vec3i& pos, int level, const Vec3i& excludeOffset,
                          std::vector<Vec3i>& wireQueue);
    void injectDirectional(const Vec3i& pos, Face facing, int level, std::vector<Vec3i>& wireQueue);
    void propagateWire(std::vector<Vec3i>& wireQueue);
    int comparatorOutput(const Vec3i& pos, const ComparatorState& state) const;

    World& world;
    Vec3i origin;
    Vec3i dims;
    int tickCounter = 0;
    std::vector<uint8_t> power;
    std::vector<uint8_t> previousPower;
    std::unordered_map<Vec3i, bool, Vec3iHash> leverStates;
    std::unordered_map<Vec3i, int, Vec3iHash> buttonTimers;
    std::unordered_map<Vec3i, RepeaterState, Vec3iHash> repeaters;
    std::unordered_map<Vec3i, ComparatorState, Vec3iHash> comparators;
};

}
