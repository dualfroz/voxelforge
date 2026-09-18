#pragma once

#include <memory>
#include <vector>

#include "Terrain.h"
#include "vf/Fluid.h"

namespace vf {

class WaterFlow {
public:
    WaterFlow(World& world, TerrainView& terrain) : world(world), terrain(terrain) {}

    void pour(const Vec3i& cell);
    void tick(const Vec3& playerPosition);

    int activeSites() const { return static_cast<int>(sites.size()); }
    int flowingCells() const;

private:
    struct Site {
        std::unique_ptr<FluidSim> sim;
        std::vector<uint8_t> previous;
        int quietSteps = 0;
    };

    Site* siteContaining(const Vec3i& cell);
    void publishChanges(Site& site);

    World& world;
    TerrainView& terrain;
    std::vector<Site> sites;
};

}
