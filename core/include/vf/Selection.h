#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "Block.h"
#include "Math.h"
#include "World.h"

namespace vf {

enum class SelectionShape : uint8_t {
    Box = 0,
    Sphere = 1,
    Ellipsoid = 2,
    Cylinder = 3,
    HollowBox = 4,
    Wand = 5,
    Mask = 6,
};

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

    SelectionMask dilated(int radius) const;
    SelectionMask eroded(int radius) const;

    SelectionMask outline() const;

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

class Selection {
public:
    Selection() = default;

    static Selection box(const Vec3i& cornerA, const Vec3i& cornerB);
    static Selection sphere(const Vec3i& center, int radius);
    static Selection ellipsoid(const Vec3i& center, const Vec3i& radii);
    static Selection cylinder(const Vec3i& center, int radius, int height);
    static Selection hollowBox(const Vec3i& cornerA, const Vec3i& cornerB, int thickness);
    static Selection fromMask(const SelectionMask& mask);

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

}
