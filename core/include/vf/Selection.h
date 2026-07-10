#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "Block.h"
#include "Math.h"
#include "World.h"

namespace vf {

// How a selection was built. Every shape collapses into a SelectionMask, so the
// tag is informational: it survives shifting but any set operation produces a
// free-form Mask, because the result no longer has a closed form.
enum class SelectionShape : uint8_t {
    Box = 0,
    Sphere = 1,
    Ellipsoid = 2,
    Cylinder = 3,
    HollowBox = 4,
    Wand = 5,
    Mask = 6,
};

// One bit per cell of an axis-aligned bounding box, indexed in the same
// (y, z, x) order the chunk storage uses. Storing the box explicitly is what
// makes union/difference/intersection and in-box negation exact rather than
// approximations over a list of positions.
class SelectionMask {
public:
    SelectionMask() = default;
    SelectionMask(const Vec3i& minCorner, const Vec3i& dims);

    const Vec3i& min() const { return corner; }
    Vec3i max() const;
    const Vec3i& dims() const { return size; }

    std::size_t cellCount() const;
    std::size_t count() const { return setCells; }
    bool empty() const { return setCells == 0u; }

    bool inBounds(const Vec3i& position) const;
    bool contains(const Vec3i& position) const;

    void add(const Vec3i& position);
    void remove(const Vec3i& position);
    void setCell(const Vec3i& position, bool on);
    void fillAll();
    void clearAll();

    SelectionMask unionWith(const SelectionMask& other) const;
    SelectionMask intersectWith(const SelectionMask& other) const;
    SelectionMask differenceWith(const SelectionMask& other) const;
    SelectionMask inverted() const;

    // Dilation and erosion over the 6-connected neighbourhood, applied `radius`
    // times. Dilation grows the bounding box; erosion keeps it and treats
    // everything outside as unset, so eroding a solid box peels its shell.
    SelectionMask dilated(int radius) const;
    SelectionMask eroded(int radius) const;

    // Cells that have at least one 6-neighbour outside the selection.
    SelectionMask outline() const;

    // Same cells, bounding box shrunk to fit them.
    SelectionMask trimmed() const;
    SelectionMask shifted(const Vec3i& delta) const;

    bool sameCells(const SelectionMask& other) const;

    void forEach(const std::function<void(const Vec3i&)>& visit) const;
    std::vector<Vec3i> cells() const;

private:
    std::size_t indexOf(const Vec3i& position) const;
    void recount();

    Vec3i corner{0, 0, 0};
    Vec3i size{0, 0, 0};
    std::vector<uint64_t> words;
    std::size_t setCells = 0;
};

// Aggregate view of what a selection covers in a given world.
struct SelectionStats {
    std::size_t blockCount = 0;
    std::size_t solidCount = 0;
    std::size_t airCount = 0;
    Vec3i dims{0, 0, 0};
    long long boundingVolume = 0;
    std::vector<uint32_t> histogram;

    uint32_t countOf(Block block) const;
    Block dominantBlock() const;
    std::size_t distinctBlocks() const;
};

// A world-space selection: a shape tag plus the mask it rasterized into.
// Every operation returns a new selection; nothing mutates in place, so a
// selection handed to a brush cannot change under it.
class Selection {
public:
    Selection() = default;

    static Selection box(const Vec3i& cornerA, const Vec3i& cornerB);
    static Selection sphere(const Vec3i& center, int radius);
    static Selection ellipsoid(const Vec3i& center, const Vec3i& radii);
    static Selection cylinder(const Vec3i& center, int radius, int height);
    static Selection hollowBox(const Vec3i& cornerA, const Vec3i& cornerB, int thickness);
    static Selection fromMask(const SelectionMask& mask);

    // Flood fill over blocks equal to the one at `seed`, 6-connected, stopping
    // at `maxCells`. The limit is what keeps a wand on an ocean of air finite,
    // so hitting it is a normal outcome reported by truncated().
    static Selection magicWand(const World& world, const Vec3i& seed, std::size_t maxCells);
    static Selection magicWandWithin(const World& world, const Vec3i& seed, std::size_t maxCells,
                                     const Vec3i& limitMin, const Vec3i& limitMax);

    SelectionShape shape() const { return kind; }
    const SelectionMask& mask() const { return cells; }
    bool truncated() const { return hitLimit; }

    bool contains(const Vec3i& position) const { return cells.contains(position); }
    std::size_t count() const { return cells.count(); }
    bool empty() const { return cells.empty(); }
    Vec3i min() const { return cells.min(); }
    Vec3i max() const { return cells.max(); }
    Vec3i dims() const { return cells.dims(); }

    Selection unionWith(const Selection& other) const;
    Selection intersectWith(const Selection& other) const;
    Selection differenceWith(const Selection& other) const;
    Selection inverted() const;
    Selection expanded(int radius) const;
    Selection contracted(int radius) const;
    Selection outline() const;
    Selection shifted(const Vec3i& delta) const;

    SelectionStats stats(const World& world) const;
    std::vector<Vec3i> blocks() const;
    void forEach(const std::function<void(const Vec3i&)>& visit) const;

private:
    static Selection derived(SelectionMask mask);

    SelectionShape kind = SelectionShape::Mask;
    SelectionMask cells;
    bool hitLimit = false;
};

} // namespace vf
