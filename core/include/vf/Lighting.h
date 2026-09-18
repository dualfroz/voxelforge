#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

constexpr int maxLight = 15;

struct LightField {
    Vec3i origin;
    Vec3i dims;
    std::vector<uint8_t> sun;
    std::vector<uint8_t> block;

    int index(int lx, int ly, int lz) const {
        return (ly * dims.z + lz) * dims.x + lx;
    }

    bool localInBox(int lx, int ly, int lz) const {
        return lx >= 0 && lx < dims.x && ly >= 0 && ly < dims.y && lz >= 0 && lz < dims.z;
    }

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

    float combinedAtWorld(int gx, int gy, int gz, float dayFactor = 1.0f) const {
        float s = static_cast<float>(sunAtWorld(gx, gy, gz)) * dayFactor;
        float b = static_cast<float>(blockAtWorld(gx, gy, gz));
        float best = s > b ? s : b;
        return best / static_cast<float>(maxLight);
    }
};

LightField computeLight(const World& world, const Vec3i& origin, const Vec3i& dims);

}
