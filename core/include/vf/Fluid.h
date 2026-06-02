#pragma once

#include <cstdint>
#include <vector>
#include "World.h"

namespace vf {

constexpr uint8_t fluidMax = 8;   // a full source cell; flow decays to 1 at the edge

// Cellular water automaton over a fixed world box. Levels run 0 (dry) to
// fluidMax. Source cells stay full every tick; flowing water spreads to lower
// horizontal neighbours losing one level per step, and pours straight down a
// gap filling the cell below fully. Updates are double buffered, so a tick is a
// pure function of the previous state (deterministic). Solid world voxels block
// and support the flow.
class FluidSim {
public:
    FluidSim(World& world, const Vec3i& origin, const Vec3i& dims);

    void addSource(int gx, int gy, int gz);
    void removeSource(int gx, int gy, int gz);
    void setLevel(int gx, int gy, int gz, uint8_t level);

    uint8_t level(int gx, int gy, int gz) const;
    bool isSource(int gx, int gy, int gz) const;
    bool isWater(int gx, int gy, int gz) const { return level(gx, gy, gz) > 0; }

    // Advances one tick. Returns true if any cell changed.
    bool step();
    // Steps until stable or maxSteps reached. Returns the number of steps taken.
    int settle(int maxSteps);

    // Writes Block::Water into the world for every wet, currently-air cell.
    void applyToWorld() const;

    const Vec3i& regionOrigin() const { return origin; }
    const Vec3i& regionDims() const { return dims; }
    const std::vector<uint8_t>& levels() const { return level_; }
    const std::vector<uint8_t>& sourceFlags() const { return source_; }
    void loadRaw(const std::vector<uint8_t>& levels, const std::vector<uint8_t>& sources);

private:
    int index(int lx, int ly, int lz) const { return (ly * dims.z + lz) * dims.x + lx; }
    bool inBox(int lx, int ly, int lz) const;
    bool solidWorld(int gx, int gy, int gz) const;
    uint8_t levelLocal(int lx, int ly, int lz) const;
    bool sourceLocal(int lx, int ly, int lz) const;
    bool spreader(int lx, int ly, int lz) const;
    uint8_t computeNext(int lx, int ly, int lz) const;

    World& world;
    Vec3i origin;
    Vec3i dims;
    std::vector<uint8_t> level_;
    std::vector<uint8_t> source_;
};

} // namespace vf
