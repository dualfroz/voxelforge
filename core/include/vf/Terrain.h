#pragma once

#include <cstdint>
#include "World.h"

namespace vf {

struct TerrainParams {
    uint32_t seed = 1337u;
    int chunksX = 8;
    int chunksZ = 8;
    int worldHeight = 64;
    int seaLevel = 26;
    int baseHeight = 24;
    float amplitude = 18.0f;
    float frequency = 0.015f;
    int octaves = 5;
    bool trees = true;
};

void generateTerrain(World& world, const TerrainParams& params);

int columnHeight(uint32_t seed, const TerrainParams& params, int gx, int gz);

}
