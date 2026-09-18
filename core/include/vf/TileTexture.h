#pragma once

#include <cstdint>

#include "TilePalette.h"

namespace vf {

struct TilePixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

struct TileTextureParams {
    int tileSize = 16;
    uint32_t noiseSeed = 0x9e3779b9u;
    float contrast = 1.0f;
};

class TileSampler {
public:
    explicit TileSampler(const TileTextureParams& params = TileTextureParams{});
    TileSampler(const TileTextureParams& params, const TilePalette& palette);

    const TileTextureParams& params() const { return settings; }
    const TilePalette& palette() const { return table; }

    TilePixel pixelAt(int tile, int x, int y) const;

    TilePixel pixelOf(const TileSpec& spec, int tileSalt, int x, int y) const;

    void writeTile(int tile, uint8_t* rgba) const;

private:
    TileTextureParams settings;
    TilePalette table;
};

}
