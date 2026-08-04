#include "vf/Selection.h"

#include <algorithm>
#include <deque>
#include <unordered_set>

namespace vf {

namespace {

constexpr int wandUnboundedReach = 1 << 20;

const Vec3i faceOffsets[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

int popcount64(uint64_t value) {
    int bits = 0;
    while (value != 0u) {
        value &= value - 1u;
        ++bits;
    }
    return bits;
}

Vec3i minCorner(const Vec3i& a, const Vec3i& b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3i maxCorner(const Vec3i& a, const Vec3i& b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

Vec3i extentOf(const Vec3i& lo, const Vec3i& hi) {
    return {hi.x - lo.x + 1, hi.y - lo.y + 1, hi.z - lo.z + 1};
}

} // namespace

SelectionMask::SelectionMask(const Vec3i& minCornerIn, const Vec3i& dims) {
    if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) return;
    corner = minCornerIn;
    size = dims;
    words.assign((cellCount() + 63u) / 64u, 0u);
}

Vec3i SelectionMask::max() const {
    return {corner.x + size.x - 1, corner.y + size.y - 1, corner.z + size.z - 1};
}

std::size_t SelectionMask::cellCount() const {
    return static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
           static_cast<std::size_t>(size.z);
}

bool SelectionMask::inBounds(const Vec3i& position) const {
    return position.x >= corner.x && position.x < corner.x + size.x &&
           position.y >= corner.y && position.y < corner.y + size.y &&
           position.z >= corner.z && position.z < corner.z + size.z;
}

std::size_t SelectionMask::indexOf(const Vec3i& position) const {
    std::size_t localX = static_cast<std::size_t>(position.x - corner.x);
    std::size_t localY = static_cast<std::size_t>(position.y - corner.y);
    std::size_t localZ = static_cast<std::size_t>(position.z - corner.z);
    return (localY * static_cast<std::size_t>(size.z) + localZ) * static_cast<std::size_t>(size.x) +
           localX;
}

bool SelectionMask::contains(const Vec3i& position) const {
    if (!inBounds(position)) return false;
    std::size_t index = indexOf(position);
    return (words[index >> 6] >> (index & 63u)) & 1u;
}

void SelectionMask::add(const Vec3i& position) { setCell(position, true); }

void SelectionMask::remove(const Vec3i& position) { setCell(position, false); }

void SelectionMask::setCell(const Vec3i& position, bool on) {
    if (!inBounds(position)) return;
    std::size_t index = indexOf(position);
    uint64_t bit = uint64_t(1) << (index & 63u);
    bool was = (words[index >> 6] & bit) != 0u;
    if (was == on) return;
    if (on) {
        words[index >> 6] |= bit;
        ++setCells;
    } else {
        words[index >> 6] &= ~bit;
        --setCells;
    }
}

void SelectionMask::fillAll() {
    std::size_t total = cellCount();
    if (total == 0u) return;
    std::fill(words.begin(), words.end(), ~uint64_t(0));
    std::size_t tail = total & 63u;
    if (tail != 0u) words.back() = (uint64_t(1) << tail) - 1u;
    setCells = total;
}

void SelectionMask::clearAll() {
    std::fill(words.begin(), words.end(), uint64_t(0));
    setCells = 0;
}

void SelectionMask::recount() {
    std::size_t total = 0;
    for (uint64_t word : words) total += static_cast<std::size_t>(popcount64(word));
    setCells = total;
}

SelectionMask SelectionMask::unionWith(const SelectionMask& other) const {
    if (cellCount() == 0u) return other;
    if (other.cellCount() == 0u) return *this;
    Vec3i lo = minCorner(corner, other.corner);
    Vec3i hi = maxCorner(max(), other.max());
    SelectionMask merged(lo, extentOf(lo, hi));
    forEach([&](const Vec3i& cell) { merged.add(cell); });
    other.forEach([&](const Vec3i& cell) { merged.add(cell); });
    return merged;
}

SelectionMask SelectionMask::intersectWith(const SelectionMask& other) const {
    Vec3i lo = maxCorner(corner, other.corner);
    Vec3i hi = minCorner(max(), other.max());
    if (cellCount() == 0u || other.cellCount() == 0u) return SelectionMask{};
    if (hi.x < lo.x || hi.y < lo.y || hi.z < lo.z) return SelectionMask{};
    SelectionMask shared(lo, extentOf(lo, hi));
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                Vec3i cell{x, y, z};
                if (contains(cell) && other.contains(cell)) shared.add(cell);
            }
    return shared;
}

SelectionMask SelectionMask::differenceWith(const SelectionMask& other) const {
    SelectionMask remainder = *this;
    other.forEach([&](const Vec3i& cell) { remainder.remove(cell); });
    return remainder;
}

SelectionMask SelectionMask::inverted() const {
    SelectionMask flipped(corner, size);
    if (flipped.cellCount() == 0u) return flipped;
    flipped.fillAll();
    for (std::size_t word = 0; word < words.size(); ++word) flipped.words[word] &= ~words[word];
    flipped.recount();
    return flipped;
}

SelectionMask SelectionMask::dilated(int radius) const {
    if (radius <= 0 || cellCount() == 0u) return *this;
    Vec3i lo{corner.x - radius, corner.y - radius, corner.z - radius};
    Vec3i hi{max().x + radius, max().y + radius, max().z + radius};
    SelectionMask grown(lo, extentOf(lo, hi));
    forEach([&](const Vec3i& cell) { grown.add(cell); });

    for (int step = 0; step < radius; ++step) {
        SelectionMask next = grown;
        grown.forEach([&](const Vec3i& cell) {
            for (const Vec3i& offset : faceOffsets) next.add(cell + offset);
        });
        grown = next;
    }
    return grown;
}

SelectionMask SelectionMask::eroded(int radius) const {
    if (radius <= 0 || cellCount() == 0u) return *this;
    SelectionMask shrunk = *this;
    for (int step = 0; step < radius; ++step) {
        SelectionMask next = shrunk;
        shrunk.forEach([&](const Vec3i& cell) {
            for (const Vec3i& offset : faceOffsets) {
                if (!shrunk.contains(cell + offset)) {
                    next.remove(cell);
                    break;
                }
            }
        });
        shrunk = next;
        if (shrunk.empty()) break;
    }
    return shrunk;
}

SelectionMask SelectionMask::outline() const {
    SelectionMask shell(corner, size);
    forEach([&](const Vec3i& cell) {
        for (const Vec3i& offset : faceOffsets) {
            if (!contains(cell + offset)) {
                shell.add(cell);
                break;
            }
        }
    });
    return shell;
}

SelectionMask SelectionMask::trimmed() const {
    if (empty()) return SelectionMask{};
    Vec3i lo = max();
    Vec3i hi = corner;
    forEach([&](const Vec3i& cell) {
        lo = minCorner(lo, cell);
        hi = maxCorner(hi, cell);
    });
    SelectionMask tight(lo, extentOf(lo, hi));
    forEach([&](const Vec3i& cell) { tight.add(cell); });
    return tight;
}

SelectionMask SelectionMask::shifted(const Vec3i& delta) const {
    SelectionMask moved = *this;
    moved.corner = corner + delta;
    return moved;
}

bool SelectionMask::sameCells(const SelectionMask& other) const {
    if (setCells != other.setCells) return false;
    if (setCells == 0u) return true;
    SelectionMask left = trimmed();
    SelectionMask right = other.trimmed();
    if (!(left.corner == right.corner) || !(left.size == right.size)) return false;
    return left.words == right.words;
}

void SelectionMask::forEach(const std::function<void(const Vec3i&)>& visit) const {
    if (setCells == 0u) return;
    for (int y = 0; y < size.y; ++y)
        for (int z = 0; z < size.z; ++z)
            for (int x = 0; x < size.x; ++x) {
                Vec3i cell{corner.x + x, corner.y + y, corner.z + z};
                if (contains(cell)) visit(cell);
            }
}

std::vector<Vec3i> SelectionMask::cells() const {
    std::vector<Vec3i> out;
    out.reserve(setCells);
    forEach([&](const Vec3i& cell) { out.push_back(cell); });
    return out;
}

uint32_t SelectionStats::countOf(Block block) const {
    std::size_t index = static_cast<std::size_t>(block);
    if (index >= histogram.size()) return 0u;
    return histogram[index];
}

Block SelectionStats::dominantBlock() const {
    Block best = Block::Air;
    uint32_t bestCount = 0;
    for (std::size_t index = 1; index < histogram.size(); ++index) {
        if (histogram[index] > bestCount) {
            bestCount = histogram[index];
            best = static_cast<Block>(index);
        }
    }
    return best;
}

std::size_t SelectionStats::distinctBlocks() const {
    std::size_t distinct = 0;
    for (uint32_t entry : histogram)
        if (entry != 0u) ++distinct;
    return distinct;
}

Selection Selection::derived(SelectionMask mask) {
    Selection out;
    out.kind = SelectionShape::Mask;
    out.cells = std::move(mask);
    return out;
}

Selection Selection::box(const Vec3i& cornerA, const Vec3i& cornerB) {
    Vec3i lo = minCorner(cornerA, cornerB);
    Vec3i hi = maxCorner(cornerA, cornerB);
    Selection out;
    out.kind = SelectionShape::Box;
    out.cells = SelectionMask(lo, extentOf(lo, hi));
    out.cells.fillAll();
    return out;
}

Selection Selection::sphere(const Vec3i& center, int radius) {
    Selection out = ellipsoid(center, {radius, radius, radius});
    out.kind = SelectionShape::Sphere;
    return out;
}

Selection Selection::ellipsoid(const Vec3i& center, const Vec3i& radii) {
    int rx = std::max(radii.x, 0);
    int ry = std::max(radii.y, 0);
    int rz = std::max(radii.z, 0);
    Vec3i lo{center.x - rx, center.y - ry, center.z - rz};
    Vec3i hi{center.x + rx, center.y + ry, center.z + rz};

    Selection out;
    out.kind = SelectionShape::Ellipsoid;
    out.cells = SelectionMask(lo, extentOf(lo, hi));

    double halfX = rx + 0.5;
    double halfY = ry + 0.5;
    double halfZ = rz + 0.5;
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                double nx = (x - center.x) / halfX;
                double ny = (y - center.y) / halfY;
                double nz = (z - center.z) / halfZ;
                if (nx * nx + ny * ny + nz * nz <= 1.0) out.cells.add({x, y, z});
            }
    return out;
}

Selection Selection::cylinder(const Vec3i& center, int radius, int height) {
    int r = std::max(radius, 0);
    int h = std::max(height, 1);
    Vec3i lo{center.x - r, center.y, center.z - r};
    Vec3i hi{center.x + r, center.y + h - 1, center.z + r};

    Selection out;
    out.kind = SelectionShape::Cylinder;
    out.cells = SelectionMask(lo, extentOf(lo, hi));

    double limit = (r + 0.5) * (r + 0.5);
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                double dx = x - center.x;
                double dz = z - center.z;
                if (dx * dx + dz * dz <= limit) out.cells.add({x, y, z});
            }
    return out;
}

Selection Selection::hollowBox(const Vec3i& cornerA, const Vec3i& cornerB, int thickness) {
    int wall = std::max(thickness, 1);
    Vec3i lo = minCorner(cornerA, cornerB);
    Vec3i hi = maxCorner(cornerA, cornerB);

    Selection out;
    out.kind = SelectionShape::HollowBox;
    out.cells = SelectionMask(lo, extentOf(lo, hi));

    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                bool onWall = (x - lo.x) < wall || (hi.x - x) < wall || (y - lo.y) < wall ||
                              (hi.y - y) < wall || (z - lo.z) < wall || (hi.z - z) < wall;
                if (onWall) out.cells.add({x, y, z});
            }
    return out;
}

Selection Selection::fromMask(const SelectionMask& mask) { return derived(mask); }

Selection Selection::magicWand(const World& world, const Vec3i& seed, std::size_t maxCells) {
    Vec3i lo{seed.x - wandUnboundedReach, seed.y - wandUnboundedReach, seed.z - wandUnboundedReach};
    Vec3i hi{seed.x + wandUnboundedReach, seed.y + wandUnboundedReach, seed.z + wandUnboundedReach};
    return magicWandWithin(world, seed, maxCells, lo, hi);
}

Selection Selection::magicWandWithin(const World& world, const Vec3i& seed, std::size_t maxCells,
                                     const Vec3i& limitMin, const Vec3i& limitMax) {
    Selection out;
    out.kind = SelectionShape::Wand;
    if (maxCells == 0u) return out;

    Vec3i lo = minCorner(limitMin, limitMax);
    Vec3i hi = maxCorner(limitMin, limitMax);
    auto insideLimit = [&](const Vec3i& cell) {
        return cell.x >= lo.x && cell.x <= hi.x && cell.y >= lo.y && cell.y <= hi.y &&
               cell.z >= lo.z && cell.z <= hi.z;
    };
    if (!insideLimit(seed)) return out;

    Block wanted = world.blockAt(seed.x, seed.y, seed.z);
    std::unordered_set<Vec3i, Vec3iHash> seen;
    std::deque<Vec3i> frontier;
    std::vector<Vec3i> found;
    seen.insert(seed);
    frontier.push_back(seed);

    while (!frontier.empty()) {
        if (found.size() >= maxCells) {
            out.hitLimit = true;
            break;
        }
        Vec3i cell = frontier.front();
        frontier.pop_front();
        found.push_back(cell);
        for (const Vec3i& offset : faceOffsets) {
            Vec3i next = cell + offset;
            if (!insideLimit(next)) continue;
            if (seen.count(next) != 0u) continue;
            if (world.blockAt(next.x, next.y, next.z) != wanted) continue;
            seen.insert(next);
            frontier.push_back(next);
        }
    }

    if (found.empty()) return out;
    Vec3i foundLo = found.front();
    Vec3i foundHi = found.front();
    for (const Vec3i& cell : found) {
        foundLo = minCorner(foundLo, cell);
        foundHi = maxCorner(foundHi, cell);
    }
    out.cells = SelectionMask(foundLo, extentOf(foundLo, foundHi));
    for (const Vec3i& cell : found) out.cells.add(cell);
    return out;
}

Selection Selection::unionWith(const Selection& other) const {
    return derived(cells.unionWith(other.cells));
}

Selection Selection::intersectWith(const Selection& other) const {
    return derived(cells.intersectWith(other.cells));
}

Selection Selection::differenceWith(const Selection& other) const {
    return derived(cells.differenceWith(other.cells));
}

Selection Selection::inverted() const { return derived(cells.inverted()); }

Selection Selection::expanded(int radius) const { return derived(cells.dilated(radius)); }

Selection Selection::contracted(int radius) const { return derived(cells.eroded(radius)); }

Selection Selection::outline() const { return derived(cells.outline()); }

Selection Selection::shifted(const Vec3i& delta) const {
    Selection out = *this;
    out.cells = cells.shifted(delta);
    return out;
}

SelectionStats Selection::stats(const World& world) const {
    SelectionStats summary;
    summary.histogram.assign(static_cast<std::size_t>(Block::Count), 0u);
    summary.dims = cells.dims();
    summary.boundingVolume = static_cast<long long>(cells.cellCount());
    summary.blockCount = cells.count();
    cells.forEach([&](const Vec3i& cell) {
        Block block = world.blockAt(cell.x, cell.y, cell.z);
        ++summary.histogram[static_cast<std::size_t>(block)];
        if (isAir(block)) {
            ++summary.airCount;
        } else {
            ++summary.solidCount;
        }
    });
    return summary;
}

std::vector<Vec3i> Selection::blocks() const { return cells.cells(); }

void Selection::forEach(const std::function<void(const Vec3i&)>& visit) const {
    cells.forEach(visit);
}

} // namespace vf
