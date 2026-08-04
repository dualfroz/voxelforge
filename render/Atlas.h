#pragma once

#include <cstdint>
#include <vector>
#include "vf/Block.h"

namespace vf {

// A procedurally generated texture-array atlas: one RGBA tile per layer,
// indexed by the tile ids that Block.h assigns to each face. Built in code so
// the demo has no binary asset dependency. The colours and the patterns come
// from vf::TilePalette and are drawn by vf::TileSampler, both in core/ and both
// under test; this header only describes the buffer they are written into.
struct Atlas {
    int tileSize = 16;
    int layers = tileCount;
    std::vector<uint8_t> pixels;   // layers * tileSize * tileSize * 4, RGBA8
};

Atlas buildAtlas(int tileSize = 16);

} // namespace vf
