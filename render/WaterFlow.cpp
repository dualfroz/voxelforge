#include "WaterFlow.h"

namespace vf {

namespace {

const Vec3i siteDims{33, 14, 33};
const Vec3i siteOffset{16, 10, 16};
constexpr int maxSites = 6;
constexpr int retireAfterQuietSteps = 12;
constexpr float retireDistance = 64.0f;

}

WaterFlow::Site* WaterFlow::siteContaining(const Vec3i& cell) {
    for (Site& site : sites) {
        const Vec3i& origin = site.sim->regionOrigin();
        const Vec3i& dims = site.sim->regionDims();
        if (cell.x >= origin.x + 1 && cell.x < origin.x + dims.x - 1 && cell.y >= origin.y &&
            cell.y < origin.y + dims.y && cell.z >= origin.z + 1 && cell.z < origin.z + dims.z - 1)
            return &site;
    }
    return nullptr;
}

void WaterFlow::pour(const Vec3i& cell) {
    Site* site = siteContaining(cell);
    if (!site) {
        if (static_cast<int>(sites.size()) >= maxSites) sites.erase(sites.begin());
        Site fresh;
        Vec3i origin{cell.x - siteOffset.x, cell.y - siteOffset.y, cell.z - siteOffset.z};
        if (origin.y < 0) origin.y = 0;
        fresh.sim = std::make_unique<FluidSim>(world, origin, siteDims);
        fresh.previous = fresh.sim->levels();
        sites.push_back(std::move(fresh));
        site = &sites.back();
    }
    site->sim->addSource(cell.x, cell.y, cell.z);
    site->quietSteps = 0;
    publishChanges(*site);
}

void WaterFlow::publishChanges(Site& site) {
    const std::vector<uint8_t>& levels = site.sim->levels();
    const Vec3i& origin = site.sim->regionOrigin();
    const Vec3i& dims = site.sim->regionDims();
    std::vector<Vec3i> wetted;
    for (int ly = 0; ly < dims.y; ++ly) {
        for (int lz = 0; lz < dims.z; ++lz) {
            for (int lx = 0; lx < dims.x; ++lx) {
                std::size_t index = static_cast<std::size_t>((ly * dims.z + lz) * dims.x + lx);
                if (levels[index] == site.previous[index] || levels[index] == 0) continue;
                Vec3i cell{origin.x + lx, origin.y + ly, origin.z + lz};
                if (world.blockAt(cell.x, cell.y, cell.z) == Block::Air) wetted.push_back(cell);
            }
        }
    }
    site.previous = levels;
    site.sim->applyToWorld();
    for (const Vec3i& cell : wetted) terrain.markCellChanged(cell.x, cell.y, cell.z);
}

void WaterFlow::tick(const Vec3& playerPosition) {
    for (Site& site : sites) {
        if (site.sim->step()) {
            site.quietSteps = 0;
            publishChanges(site);
        } else {
            ++site.quietSteps;
        }
    }
    for (std::size_t i = 0; i < sites.size();) {
        const Vec3i& origin = sites[i].sim->regionOrigin();
        float dx = static_cast<float>(origin.x + siteOffset.x) - playerPosition.x;
        float dz = static_cast<float>(origin.z + siteOffset.z) - playerPosition.z;
        bool tooFar = dx * dx + dz * dz > retireDistance * retireDistance;
        if (tooFar || sites[i].quietSteps >= retireAfterQuietSteps) sites.erase(sites.begin() + i);
        else ++i;
    }
}

int WaterFlow::flowingCells() const {
    int wet = 0;
    for (const Site& site : sites)
        for (uint8_t level : site.sim->levels())
            if (level > 0) ++wet;
    return wet;
}

}
