#pragma once

#include <cstdint>

#include "TilePalette.h"

namespace vf {

// One generated texel. Alpha is either 0 or 255: the atlas is sampled with
// nearest filtering, so a half transparent edge would only smear.
struct TilePixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// Everything the generator needs beyond the palette itself. Two samplers built
// from equal params produce byte identical output; nothing here reads a clock,
// a global, or the order tiles happen to be generated in.
struct TileTextureParams {
    int tileSize = 16;
    uint32_t noiseSeed = 0x9e3779b9u;
    // Scales every pixel's distance from its tile's base colour. 1 is the
    // palette as authored.
    float contrast = 1.0f;
};

// Turns a TileSpec into pixels. The sampler owns its palette copy so a caller
// can hold several with different params without them sharing state, and no
// allocation happens per pixel or per tile.
class TileSampler {
public:
    explicit TileSampler(const TileTextureParams& params = TileTextureParams{});
    TileSampler(const TileTextureParams& params, const TilePalette& palette);

    const TileTextureParams& params() const { return settings; }
    const TilePalette& palette() const { return table; }

    TilePixel pixelAt(int tile, int x, int y) const;

    // Same generator against a spec the palette does not hold, which is how the
    // pattern layer is tested without the palette's noise in the way.
    TilePixel pixelOf(const TileSpec& spec, int tileSalt, int x, int y) const;

    // Writes tileSize * tileSize RGBA8 texels, row major, into caller memory.
    void writeTile(int tile, uint8_t* rgba) const;

private:
    TileTextureParams settings;
    TilePalette table;
};

} // namespace vf
