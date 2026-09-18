#pragma once

#include <string>

#include "vf/Inventory.h"
#include "vf/Math.h"

namespace vf {

struct HudFrame {
    int fps = 0;
    Vec3 position;
    std::string biome;
    std::string phase;
    long long day = 0;
    int hour = 0;
    int minute = 0;
    int moonPhase = 0;
    int skyLight = 15;
    std::string weather;
    int timeScale = 1;
    float health = 0.0f;
    float maxHealth = 20.0f;
    float hunger = 0.0f;
    float maxHunger = 20.0f;
    float air = 0.0f;
    float maxAir = 15.0f;
    int food = 0;
    bool flying = false;
    bool swimming = false;
    bool underwater = false;
    int passive = 0;
    int hostile = 0;
    int villagers = 0;
    int employed = 0;
    int beds = 0;
    int workstations = 0;
    int waterSites = 0;
    int flowingCells = 0;
    int columns = 0;
    int meshes = 0;
    int pending = 0;
    std::string target;
    float mining = 0.0f;
    std::string message;
};

void pushHotbar(const Hotbar& hotbar);
void pushHud(const HudFrame& frame);

}
