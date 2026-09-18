#include "Atlas.h"

#include <cstddef>

#include "vf/TileTexture.h"

namespace vf {

Atlas buildAtlas(int tileSize) {
    Atlas atlas;
    atlas.tileSize = tileSize;
    atlas.layers = tileCount;
    atlas.pixels.resize(static_cast<std::size_t>(tileCount) * tileSize * tileSize * 4);

    TileTextureParams params;
    params.tileSize = tileSize;
    TileSampler sampler(params);

    const std::size_t tileBytes = static_cast<std::size_t>(tileSize) * tileSize * 4;
    for (int tile = 0; tile < tileCount; ++tile) {
        sampler.writeTile(tile, atlas.pixels.data() + tileBytes * static_cast<std::size_t>(tile));
    }
    return atlas;
}

}
