#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Block.h"
#include "EditHistory.h"
#include "Math.h"
#include "Selection.h"
#include "World.h"

namespace vf {

enum class BrushKind : uint8_t {
    Sphere = 0,
    Cylinder = 1,
    Replace = 2,
    Smooth = 3,
    Erode = 4,
    Overlay = 5,
    Naturalize = 6,
    Gradient = 7,
};

// Everything a stroke needs. One struct for all brushes: a kind reads the
// fields that apply to it and ignores the rest, which keeps a brush preset a
// plain value that can be stored, copied and replayed.
struct BrushParams {
    enum class VolumeMode : uint8_t {
        Fill = 0,
        Hollow = 1,
        Shell = 2,
    };

    BrushKind kind = BrushKind::Sphere;
    VolumeMode mode = VolumeMode::Fill;

    Vec3i center{0, 0, 0};
    int radius = 3;
    int height = 4;
    int shellThickness = 1;

    Block block = Block::Stone;
    Block secondaryBlock = Block::Dirt;

    // Empty means "every non-air block"; otherwise only the listed types are
    // touched, which is what makes a replace brush safe to drag over terrain.
    std::vector<Block> onlyReplace;

    int freeNeighbourThreshold = 3;
    int soilDepth = 3;
    Block surfaceBlock = Block::Grass;
    Block soilBlock = Block::Dirt;
    Block deepBlock = Block::Stone;

    uint32_t seed = 0u;

    int worldMinY = 0;
    int worldMaxY = 256;

    std::string label;
};

struct BrushResult {
    std::size_t changed = 0;
    std::size_t visited = 0;
    bool outsideLoadedWorld = false;
    bool clippedAtHeightLimit = false;
    bool recorded = false;
};

// Applies brushes to a world through an EditHistory. One call to apply() is one
// batch, so every stroke - including the ones that touch thousands of blocks -
// undoes in a single step.
class BrushStroke {
public:
    BrushStroke(World& world, EditHistory& history);

    BrushResult apply(const BrushParams& params, uint64_t tick);
    BrushResult applyWithin(const BrushParams& params, const Selection& limit, uint64_t tick);

    // The volume a brush covers when no selection restricts it.
    static Selection regionOf(const BrushParams& params);
    static const char* kindName(BrushKind kind);

    const BrushResult& lastResult() const { return previous; }

private:
    void write(const BrushParams& params, const Vec3i& cell, Block block, BrushResult& outcome);
    void paintVolume(const BrushParams& params, const Selection& limit, BrushResult& outcome);
    void paintReplace(const BrushParams& params, const Selection& limit, BrushResult& outcome);
    void paintSmooth(const BrushParams& params, const Selection& limit, BrushResult& outcome);
    void paintNeighbourRule(const BrushParams& params, const Selection& limit, bool erode,
                            BrushResult& outcome);
    void paintNaturalize(const BrushParams& params, const Selection& limit, BrushResult& outcome);
    void paintGradient(const BrushParams& params, const Selection& limit, BrushResult& outcome);

    World& world;
    EditHistory& history;
    BrushResult previous;
};

} // namespace vf
