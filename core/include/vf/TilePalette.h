#pragma once

#include <cstdint>
#include <vector>

#include "Block.h"

namespace vf {

// Eight bit colour, the unit every tile is built from. Alpha is deliberately
// absent: only the sampler decides where a tile is cut away, and it decides it
// per pixel from the pattern, not from a colour the palette carries around.
struct TileRgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

inline bool operator==(const TileRgb& a, const TileRgb& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

inline bool operator!=(const TileRgb& a, const TileRgb& b) { return !(a == b); }

// What the surface is made of. The material never picks a colour; it shapes the
// grain that runs over whatever the pattern laid down, and its amplitude is the
// spec's noise value. A tile with zero noise therefore has no grain at all,
// whatever its material says.
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

// The large scale shape drawn across the tile before the material grain.
enum class TilePattern : uint8_t {
    Solid,        // one colour with accent blotches drifting through it
    OreSpeckle,   // veins of accent over the host rock, rimmed dark
    Brick,        // courses with mortar, every other row offset by half a brick
    Plank,        // boards with seams and grain running along the board
    Lattice,      // rails and rungs, or a full grid, with gaps between them
    EdgeBand,     // a band of accent along one edge with a ragged fringe
    GrowthRings,  // concentric rings around the tile centre
    Dots,         // scattered round dots on a jittered cell grid
    Strata,       // layer bands across the tile, warped along their length
    Splatter,     // irregular blobs of accent, rimmed dark
    Liquid,       // travelling wave crests with ripple on top
    Cross,        // a plant sprite: stalk plus leaves, the rest cut away
    Web,          // radial spokes and rings, the rest cut away
    Pane,         // a frame with glints, the middle cut away
    Count,
};

// One row of the palette. scale and density are pattern relative, both in the
// 0..1 range:
//   scale    feature size (vein width, board width, ring spacing, dot radius)
//   density  how much of the tile the feature covers
//   vertical the pattern's main axis runs along Y instead of X
//   flipped  pattern variant: EdgeBand bands the far edge, Lattice draws a full
//            grid instead of two rails, Plank shifts its board tinting
//   cutout   the pattern's gaps become alpha 0 instead of shaded backdrop
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

// The registry: one TileSpec for every tile id Block.h hands out, indexed by
// that id. Built as data rather than as a switch so the whole table can be
// walked in a test instead of sampled at a few well chosen points.
class TilePalette {
public:
    TilePalette();

    const TileSpec& specFor(int tile) const;
    int size() const { return static_cast<int>(rows.size()); }
    const std::vector<TileSpec>& specs() const { return rows; }

private:
    std::vector<TileSpec> rows;
};

} // namespace vf
