#include "vf/Fluid.h"

namespace vf {

FluidSim::FluidSim(World& world, const Vec3i& origin, const Vec3i& dims)
    : world(world), origin(origin), dims(dims) {
    std::size_t cells = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
    level_.assign(cells, 0);
    source_.assign(cells, 0);
}

bool FluidSim::inBox(int lx, int ly, int lz) const {
    return lx >= 0 && lx < dims.x && ly >= 0 && ly < dims.y && lz >= 0 && lz < dims.z;
}

bool FluidSim::solidWorld(int gx, int gy, int gz) const { return world.solidAt(gx, gy, gz); }

uint8_t FluidSim::levelLocal(int lx, int ly, int lz) const {
    if (!inBox(lx, ly, lz)) return 0;
    return level_[index(lx, ly, lz)];
}

bool FluidSim::sourceLocal(int lx, int ly, int lz) const {
    if (!inBox(lx, ly, lz)) return false;
    return source_[index(lx, ly, lz)] != 0;
}

bool FluidSim::spreader(int lx, int ly, int lz) const {
    if (sourceLocal(lx, ly, lz)) return true;
    if (levelLocal(lx, ly, lz) == 0) return false;
    if (solidWorld(origin.x + lx, origin.y + ly - 1, origin.z + lz)) return true;
    if (levelLocal(lx, ly - 1, lz) > 0 && levelLocal(lx, ly + 1, lz) == 0) return true;
    return false;
}

uint8_t FluidSim::computeNext(int lx, int ly, int lz) const {
    if (solidWorld(origin.x + lx, origin.y + ly, origin.z + lz)) return 0;
    if (sourceLocal(lx, ly, lz)) return fluidMax;
    if (levelLocal(lx, ly + 1, lz) > 0) return fluidMax;

    uint8_t best = 0;
    const int horiz[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& h : horiz) {
        int nx = lx + h[0], nz = lz + h[1];
        uint8_t lh = levelLocal(nx, ly, nz);
        if (lh <= 1 || !spreader(nx, ly, nz)) continue;
        uint8_t give = static_cast<uint8_t>(lh - 1);
        if (give > best) best = give;
    }
    return best;
}

bool FluidSim::step() {
    std::vector<uint8_t> next(level_.size(), 0);
    for (int lx = 0; lx < dims.x; ++lx)
        for (int ly = 0; ly < dims.y; ++ly)
            for (int lz = 0; lz < dims.z; ++lz)
                next[index(lx, ly, lz)] = computeNext(lx, ly, lz);
    bool changed = next != level_;
    level_.swap(next);
    return changed;
}

int FluidSim::settle(int maxSteps) {
    int steps = 0;
    while (steps < maxSteps && step()) ++steps;
    return steps;
}

void FluidSim::addSource(int gx, int gy, int gz) {
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (!inBox(lx, ly, lz)) return;
    source_[index(lx, ly, lz)] = 1;
    level_[index(lx, ly, lz)] = fluidMax;
}

void FluidSim::removeSource(int gx, int gy, int gz) {
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (!inBox(lx, ly, lz)) return;
    source_[index(lx, ly, lz)] = 0;
}

void FluidSim::setLevel(int gx, int gy, int gz, uint8_t value) {
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (!inBox(lx, ly, lz)) return;
    level_[index(lx, ly, lz)] = value > fluidMax ? fluidMax : value;
}

uint8_t FluidSim::level(int gx, int gy, int gz) const {
    return levelLocal(gx - origin.x, gy - origin.y, gz - origin.z);
}

bool FluidSim::isSource(int gx, int gy, int gz) const {
    return sourceLocal(gx - origin.x, gy - origin.y, gz - origin.z);
}

void FluidSim::applyToWorld() const {
    for (int lx = 0; lx < dims.x; ++lx)
        for (int ly = 0; ly < dims.y; ++ly)
            for (int lz = 0; lz < dims.z; ++lz) {
                if (level_[index(lx, ly, lz)] == 0) continue;
                int gx = origin.x + lx, gy = origin.y + ly, gz = origin.z + lz;
                if (world.blockAt(gx, gy, gz) == Block::Air)
                    world.setBlock(gx, gy, gz, Block::Water);
            }
}

void FluidSim::loadRaw(const std::vector<uint8_t>& levels, const std::vector<uint8_t>& sources) {
    if (levels.size() == level_.size()) level_ = levels;
    if (sources.size() == source_.size()) source_ = sources;
}

}
