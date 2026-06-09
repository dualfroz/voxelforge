#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Entity.h"

namespace vf {

// A rectangular byte grid: two parallel channels over an origin/dims box. Used to
// carry a fluid snapshot (levels + sources) or a light snapshot (sun + block).
struct GridSnapshot {
    Vec3i origin;
    Vec3i dims;
    std::vector<uint8_t> channelA;
    std::vector<uint8_t> channelB;
};

// The full v3 save payload: world blocks plus the simulation state that v1/v2 did
// not persist (mobs, fluid, light).
struct SaveV3 {
    World world;
    std::vector<Mob> mobs;
    bool hasFluid = false;
    GridSnapshot fluid;
    bool hasLight = false;
    GridSnapshot light;
};

// Binary format "VXF3": magic, version 3, the v1 RLE block stream, then tagged,
// length-prefixed sections (mobs, fluid, light). Unknown sections are skipped.
std::vector<uint8_t> serializeSaveV3(const SaveV3& save);

// Loads a save into `out`. Accepts a v3 buffer (full state) or a legacy v1/v2
// buffer (magic "VXF1"), in which case only the world is populated and the
// mobs/fluid/light stay empty. Returns false on bad magic or a truncated stream.
bool deserializeSaveV3(const uint8_t* data, std::size_t size, SaveV3& out);

} // namespace vf
