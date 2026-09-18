#pragma once

#include <cstdint>
#include <vector>
#include "vf/Block.h"

namespace vf {

struct Atlas {
    int tileSize = 16;
    int layers = tileCount;
    std::vector<uint8_t> pixels;
};

Atlas buildAtlas(int tileSize = 16);

}
