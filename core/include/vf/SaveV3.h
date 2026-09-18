#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Entity.h"

namespace vf {

struct GridSnapshot {
    Vec3i origin;
    Vec3i dims;
    std::vector<uint8_t> channelA;
    std::vector<uint8_t> channelB;
};

struct SaveV3 {
    World world;
    std::vector<Mob> mobs;
    bool hasFluid = false;
    GridSnapshot fluid;
    bool hasLight = false;
    GridSnapshot light;
};

std::vector<uint8_t> serializeSaveV3(const SaveV3& save);

bool deserializeSaveV3(const uint8_t* data, std::size_t size, SaveV3& out);

}
