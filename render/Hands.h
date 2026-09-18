#pragma once

#include <string>

#include "Explorer.h"
#include "Village.h"
#include "WaterFlow.h"
#include "Wildlife.h"
#include "vf/Inventory.h"
#include "vf/Raycast.h"

namespace vf {

class Hands {
public:
    Hands(World& world, TerrainView& terrain, WaterFlow& water, Wildlife& wildlife,
          Village& village, Explorer& explorer, Hotbar& hotbar);

    void aim();
    void press();
    void release();
    std::string work(float dt);
    void place();

    std::string describeTarget() const;
    float miningFraction() const;

private:
    std::string punch();

    World& world;
    TerrainView& terrain;
    WaterFlow& water;
    Wildlife& wildlife;
    Village& village;
    Explorer& explorer;
    Hotbar& hotbar;

    RaycastHit block;
    int creature = -1;
    int villager = -1;
    bool held = false;
    Vec3i miningCell;
    float miningProgress = 0.0f;
    float punchTimer = 0.0f;
};

}
