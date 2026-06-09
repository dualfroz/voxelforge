#include "Atlas.h"

#include <cmath>
#include <cstdint>

namespace vf {

namespace {

struct Rgb {
    uint8_t r, g, b;
};

// Deterministic per-tile value noise so tiles read as textured, not flat.
float tileNoise(int tile, int x, int y) {
    uint32_t h = static_cast<uint32_t>(tile) * 374761393u + static_cast<uint32_t>(x) * 668265263u +
                 static_cast<uint32_t>(y) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xffff) / 65535.0f;
}

Rgb baseColor(int tile) {
    switch (tile) {
        case 0: return {86, 154, 66};    // grass top
        case 1: return {112, 92, 58};    // grass side (dirt with green lip handled below)
        case 2: return {119, 88, 58};    // dirt
        case 3: return {129, 129, 132};  // stone
        case 4: return {214, 199, 143};  // sand
        case 5: return {104, 78, 46};    // wood bark
        case 6: return {160, 128, 78};   // wood rings
        case 7: return {58, 122, 52};    // leaves
        case 8: return {54, 96, 168};    // water
        case 9: return {236, 240, 245};  // snow
        case 10: return {129, 129, 132};  // coal ore (stone base)
        case 11: return {129, 129, 132};  // iron ore (stone base)
        case 12: return {129, 129, 132};  // gold ore (stone base)
        case 13: return {122, 120, 120};  // gravel
        case 14: return {242, 220, 132};  // glowstone
        default: return {160, 160, 160};
    }
}

// Ore tiles carry coloured speckles over the stone base. Returns true and the
// speckle colour when this pixel belongs to a vein.
bool oreSpeckle(int tile, int x, int y, Rgb& out) {
    if (tile < 10 || tile > 12) return false;
    uint32_t h = static_cast<uint32_t>(tile) * 2654435761u + static_cast<uint32_t>(x) * 40503u +
                 static_cast<uint32_t>(y) * 12289u;
    h ^= h >> 15;
    if ((h & 7u) >= 2u) return false;
    switch (tile) {
        case 10: out = {38, 38, 42}; break;     // coal
        case 11: out = {196, 156, 120}; break;   // iron
        default: out = {238, 214, 96}; break;    // gold
    }
    return true;
}

uint8_t clampByte(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

} // namespace

Atlas buildAtlas(int tileSize) {
    Atlas atlas;
    atlas.tileSize = tileSize;
    atlas.layers = tileCount;
    atlas.pixels.resize(static_cast<std::size_t>(tileCount) * tileSize * tileSize * 4);

    for (int tile = 0; tile < tileCount; ++tile) {
        Rgb base = baseColor(tile);
        for (int y = 0; y < tileSize; ++y) {
            for (int x = 0; x < tileSize; ++x) {
                float n = tileNoise(tile, x, y) * 0.22f - 0.11f;
                float r = base.r * (1.0f + n);
                float g = base.g * (1.0f + n);
                float b = base.b * (1.0f + n);

                // Grass side: green band across the top few rows.
                if (tile == 1 && y < tileSize / 4) {
                    r = 86 * (1.0f + n);
                    g = 154 * (1.0f + n);
                    b = 66 * (1.0f + n);
                }
                Rgb speck;
                if (oreSpeckle(tile, x, y, speck)) {
                    r = speck.r * (1.0f + n);
                    g = speck.g * (1.0f + n);
                    b = speck.b * (1.0f + n);
                }

                // Subtle grid so block edges stay legible.
                if (x == 0 || y == 0) {
                    r *= 0.86f;
                    g *= 0.86f;
                    b *= 0.86f;
                }

                std::size_t idx =
                    (static_cast<std::size_t>(tile) * tileSize * tileSize + y * tileSize + x) * 4;
                atlas.pixels[idx + 0] = clampByte(r);
                atlas.pixels[idx + 1] = clampByte(g);
                atlas.pixels[idx + 2] = clampByte(b);
                atlas.pixels[idx + 3] = 255;
            }
        }
    }
    return atlas;
}

} // namespace vf
