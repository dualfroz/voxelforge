#pragma once

#include <cstdint>
#include "World.h"

namespace vf {

struct TerrainParams {
    uint32_t seed = 1337u;
    int chunksX = 8;
    int chunksZ = 8;
    int worldHeight = 64;   // in blocks
    int seaLevel = 26;
    int baseHeight = 24;
    float amplitude = 18.0f;
    float frequency = 0.015f;  // world-space sample scale
    int octaves = 5;
    bool trees = true;
};

// Fills `world` with a bounded heightmap terrain: stone core, dirt, grass,
// sand near water, water up to sea level, snow on peaks, and scattered trees.
void generateTerrain(World& world, const TerrainParams& params);

// Terrain surface height (topmost solid block y) for a world column.
int columnHeight(uint32_t seed, const TerrainParams& params, int gx, int gz);

} // namespace vf
