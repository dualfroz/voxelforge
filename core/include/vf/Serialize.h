#pragma once

#include <cstdint>
#include <vector>
#include "World.h"

namespace vf {

std::vector<uint8_t> serializeWorld(const World& world);

bool deserializeWorld(const uint8_t* data, std::size_t size, World& out);

}
