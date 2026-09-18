#pragma once

#include <cstdint>
#include <vector>

#include "Block.h"

namespace vf {

struct TileRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

inline bool operator==(const TileRgb& a, const TileRgb& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline bool operator!=(const TileRgb& a, const TileRgb& b) { return !(a == b); }

enum class TileMaterial : uint8_t {
    Stone,
    Soil,
    Wood,
    Foliage,
    Metal,
    Crystal,
    Fabric,
    Fluid,
    Light,
    Organic,
    Count,
};

enum class TilePattern : uint8_t {
    Solid,
    OreSpeckle,
    Brick,
    Plank,
    Lattice,
    EdgeBand,
    GrowthRings,
    Dots,
    Strata,
    Splatter,
    Liquid,
    Cross,
    Web,
    Pane,
    Count,
};

struct TileSpec {
    TileRgb base{160, 160, 160};
    TileRgb accent{160, 160, 160};
    float noise = 0.10f;
    TilePattern pattern = TilePattern::Solid;
    TileMaterial material = TileMaterial::Stone;
    float scale = 0.5f;
    float density = 0.5f;
    bool vertical = false;
    bool flipped = false;
    bool cutout = false;
    const char* name = "";
};

class TilePalette {
public:
    TilePalette();

    const TileSpec& specFor(int tile) const;
    int size() const { return static_cast<int>(rows.size()); }
    const std::vector<TileSpec>& specs() const { return rows; }

private:
    std::vector<TileSpec> rows;
};

}
