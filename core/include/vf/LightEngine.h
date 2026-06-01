#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Block.h"

namespace vf {

constexpr int maxLightLevel = 15;

// Incremental sky + block light over a fixed world box. Sky light is seeded from
// the open top of the box at full strength and travels straight down without loss
// while unobstructed, attenuating one level per step in every other direction.
// Block light radiates from emitters, attenuating one level per step. Editing a
// block re-lights only the affected neighbourhood (flood-fill add plus removal
// darkening) and yields the same field a full rebuild would produce. The box is
// chunk-agnostic: it indexes by world coordinate, so light crosses chunk borders
// seamlessly.
class LightEngine {
public:
    LightEngine(World& world, const Vec3i& origin, const Vec3i& dims);

    void rebuild();

    // Sets a world block and incrementally updates both light channels.
    void setBlock(int gx, int gy, int gz, Block block);

    uint8_t sunAt(int gx, int gy, int gz) const;
    uint8_t blockAt(int gx, int gy, int gz) const;
    float combinedAt(int gx, int gy, int gz, float dayFactor = 1.0f) const;

    const Vec3i& regionOrigin() const { return origin; }
    const Vec3i& regionDims() const { return dims; }
    const std::vector<uint8_t>& sunField() const { return sun; }
    const std::vector<uint8_t>& blockField() const { return block; }
    void loadRaw(const std::vector<uint8_t>& sunData, const std::vector<uint8_t>& blockData) {
        if (sunData.size() == sun.size()) sun = sunData;
        if (blockData.size() == block.size()) block = blockData;
    }

private:
    struct Node {
        int lx, ly, lz;
        uint8_t level;
    };

    int index(int lx, int ly, int lz) const { return (ly * dims.z + lz) * dims.x + lx; }
    bool inBox(int lx, int ly, int lz) const;
    bool occludesLocal(int lx, int ly, int lz) const;

    // isSun selects the straight-down full-strength rule for the channel.
    void floodAdd(std::vector<uint8_t>& field, std::vector<Node>& queue, bool isSun);
    void floodRemove(std::vector<uint8_t>& field, std::vector<Node>& removals,
                     std::vector<Node>& reAdd, bool isSun);
    void updateSun(int lx, int ly, int lz);
    void updateBlock(int lx, int ly, int lz, Block newBlock);

    World& world;
    Vec3i origin;
    Vec3i dims;
    std::vector<uint8_t> sun;
    std::vector<uint8_t> block;
};

} // namespace vf
