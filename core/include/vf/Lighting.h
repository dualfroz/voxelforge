#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

constexpr int maxLight = 15;

// Baked light for a rectangular world region. Two channels: `sun` (skylight,
// seeded from the open top of the box and travelling straight down without
// attenuation) and `block` (emitter light from Glowstone). Both are BFS
// flood-filled, losing one level per step into a transparent neighbour.
struct LightField {
    Vec3i origin;   // world-space minimum corner
    Vec3i dims;     // extent in blocks
    std::vector<uint8_t> sun;
    std::vector<uint8_t> block;

    int index(int lx, int ly, int lz) const {
        return (ly * dims.z + lz) * dims.x + lx;
    }

    bool localInBox(int lx, int ly, int lz) const {
        return lx >= 0 && lx < dims.x && ly >= 0 && ly < dims.y && lz >= 0 && lz < dims.z;
    }

    // World-space sampling. Skylight above the box top reads as open sky (full),
    // everything else outside the box reads dark.
    uint8_t sunAtWorld(int gx, int gy, int gz) const {
        int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
        if (ly >= dims.y && lx >= 0 && lx < dims.x && lz >= 0 && lz < dims.z)
            return static_cast<uint8_t>(maxLight);
        if (!localInBox(lx, ly, lz)) return 0;
        return sun[index(lx, ly, lz)];
    }

    uint8_t blockAtWorld(int gx, int gy, int gz) const {
        int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
        if (!localInBox(lx, ly, lz)) return 0;
        return block[index(lx, ly, lz)];
    }

    // Combined perceived brightness 0..1: brighter of scaled skylight and block
    // light. `dayFactor` in 0..1 dims the sun for night without touching torches.
    float combinedAtWorld(int gx, int gy, int gz, float dayFactor = 1.0f) const {
        float s = static_cast<float>(sunAtWorld(gx, gy, gz)) * dayFactor;
        float b = static_cast<float>(blockAtWorld(gx, gy, gz));
        float best = s > b ? s : b;
        return best / static_cast<float>(maxLight);
    }
};

// Flood-fills skylight and block light across a world region. The box top is
// treated as open sky. Opaque blocks stop propagation; Air, Water and Leaves
// carry light onward.
LightField computeLight(const World& world, const Vec3i& origin, const Vec3i& dims);

} // namespace vf
