#include "vf/Brush.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

const Vec3i faceOffsets[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

uint32_t brushHash(uint32_t seed, const Vec3i& cell) {
    uint32_t h = seed * 2654435761u;
    h ^= static_cast<uint32_t>(cell.x) * 73856093u;
    h ^= static_cast<uint32_t>(cell.y) * 19349663u;
    h ^= static_cast<uint32_t>(cell.z) * 83492791u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

bool listed(const std::vector<Block>& blocks, Block block) {
    return std::find(blocks.begin(), blocks.end(), block) != blocks.end();
}

Selection innerVolume(const BrushParams& params) {
    int innerRadius = params.radius - params.shellThickness;
    if (innerRadius < 0) return Selection{};
    if (params.kind == BrushKind::Cylinder) {
        int innerHeight = params.height - 2 * params.shellThickness;
        if (innerHeight < 1) return Selection{};
        Vec3i base{params.center.x, params.center.y + params.shellThickness, params.center.z};
        return Selection::cylinder(base, innerRadius, innerHeight);
    }
    return Selection::sphere(params.center, innerRadius);
}

}

BrushStroke::BrushStroke(World& world, EditHistory& history) : world(world), history(history) {}

const char* BrushStroke::kindName(BrushKind kind) {
    switch (kind) {
        case BrushKind::Sphere: return "sphere";
        case BrushKind::Cylinder: return "cylinder";
        case BrushKind::Replace: return "replace";
        case BrushKind::Smooth: return "smooth";
        case BrushKind::Erode: return "erode";
        case BrushKind::Overlay: return "overlay";
        case BrushKind::Naturalize: return "naturalize";
        case BrushKind::Gradient: return "gradient";
    }
    return "brush";
}

Selection BrushStroke::regionOf(const BrushParams& params) {
    if (params.kind == BrushKind::Cylinder)
        return Selection::cylinder(params.center, params.radius, params.height);
    return Selection::sphere(params.center, params.radius);
}

BrushResult BrushStroke::apply(const BrushParams& params, uint64_t tick) {
    return applyWithin(params, regionOf(params), tick);
}

BrushResult BrushStroke::applyWithin(const BrushParams& params, const Selection& limit,
                                     uint64_t tick) {
    BrushResult outcome;
    std::string label = params.label.empty() ? std::string(kindName(params.kind)) : params.label;
    history.beginBatch(label, tick);

    switch (params.kind) {
        case BrushKind::Sphere:
        case BrushKind::Cylinder:
            paintVolume(params, limit, outcome);
            break;
        case BrushKind::Replace:
            paintReplace(params, limit, outcome);
            break;
        case BrushKind::Smooth:
            paintSmooth(params, limit, outcome);
            break;
        case BrushKind::Erode:
            paintNeighbourRule(params, limit, true, outcome);
            break;
        case BrushKind::Overlay:
            paintNeighbourRule(params, limit, false, outcome);
            break;
        case BrushKind::Naturalize:
            paintNaturalize(params, limit, outcome);
            break;
        case BrushKind::Gradient:
            paintGradient(params, limit, outcome);
            break;
    }

    outcome.recorded = history.endBatch();
    previous = outcome;
    return outcome;
}

void BrushStroke::write(const BrushParams& params, const Vec3i& cell, Block block,
                        BrushResult& outcome) {
    ++outcome.visited;
    if (cell.y < params.worldMinY || cell.y >= params.worldMaxY) {
        outcome.clippedAtHeightLimit = true;
        return;
    }
    Vec3i chunkCoord{floorDiv(cell.x, chunkSize), floorDiv(cell.y, chunkSize),
                     floorDiv(cell.z, chunkSize)};
    if (world.chunkAt(chunkCoord) == nullptr) outcome.outsideLoadedWorld = true;
    if (history.recordSet(world, cell, block)) ++outcome.changed;
}

void BrushStroke::paintVolume(const BrushParams& params, const Selection& limit,
                              BrushResult& outcome) {
    Selection shape = regionOf(params);
    if (params.mode == BrushParams::VolumeMode::Shell)
        shape = shape.differenceWith(innerVolume(params));
    Selection painted = shape.intersectWith(limit);
    Block value = params.mode == BrushParams::VolumeMode::Hollow ? Block::Air : params.block;
    painted.forEach([&](const Vec3i& cell) { write(params, cell, value, outcome); });
}

void BrushStroke::paintReplace(const BrushParams& params, const Selection& limit,
                               BrushResult& outcome) {
    limit.forEach([&](const Vec3i& cell) {
        Block existing = world.blockAt(cell.x, cell.y, cell.z);
        if (params.onlyReplace.empty()) {
            if (isAir(existing)) return;
        } else if (!listed(params.onlyReplace, existing)) {
            return;
        }
        write(params, cell, params.block, outcome);
    });
}

void BrushStroke::paintSmooth(const BrushParams& params, const Selection& limit,
                              BrushResult& outcome) {
    if (limit.empty()) return;
    Vec3i lo = limit.min();
    Vec3i hi = limit.max();
    std::size_t spanX = static_cast<std::size_t>(hi.x - lo.x + 1);
    std::size_t spanZ = static_cast<std::size_t>(hi.z - lo.z + 1);
    std::size_t columns = spanX * spanZ;

    std::vector<uint8_t> present(columns, 0u);
    std::vector<int> columnLow(columns, 0);
    std::vector<int> columnHigh(columns, 0);
    limit.forEach([&](const Vec3i& cell) {
        std::size_t column = static_cast<std::size_t>(cell.z - lo.z) * spanX +
                             static_cast<std::size_t>(cell.x - lo.x);
        if (present[column] == 0u) {
            present[column] = 1u;
            columnLow[column] = cell.y;
            columnHigh[column] = cell.y;
            return;
        }
        columnLow[column] = std::min(columnLow[column], cell.y);
        columnHigh[column] = std::max(columnHigh[column], cell.y);
    });

    std::vector<int> surface(columns, 0);
    for (std::size_t column = 0; column < columns; ++column) {
        if (present[column] == 0u) continue;
        int x = lo.x + static_cast<int>(column % spanX);
        int z = lo.z + static_cast<int>(column / spanX);
        int top = columnLow[column] - 1;
        for (int y = columnHigh[column]; y >= columnLow[column]; --y) {
            if (!isAir(world.blockAt(x, y, z))) {
                top = y;
                break;
            }
        }
        surface[column] = top;
    }

    std::vector<int> target(columns, 0);
    int reach = std::max(params.radius, 1);
    for (std::size_t column = 0; column < columns; ++column) {
        if (present[column] == 0u) continue;
        int cx = static_cast<int>(column % spanX);
        int cz = static_cast<int>(column / spanX);
        long long sum = 0;
        int samples = 0;
        for (int dz = -reach; dz <= reach; ++dz)
            for (int dx = -reach; dx <= reach; ++dx) {
                if (dx * dx + dz * dz > reach * reach) continue;
                int nx = cx + dx;
                int nz = cz + dz;
                if (nx < 0 || nz < 0 || nx >= static_cast<int>(spanX) ||
                    nz >= static_cast<int>(spanZ))
                    continue;
                std::size_t neighbour =
                    static_cast<std::size_t>(nz) * spanX + static_cast<std::size_t>(nx);
                if (present[neighbour] == 0u) continue;
                sum += surface[neighbour];
                ++samples;
            }
        target[column] = samples == 0
                             ? surface[column]
                             : static_cast<int>(std::lround(static_cast<double>(sum) / samples));
    }

    for (std::size_t column = 0; column < columns; ++column) {
        if (present[column] == 0u) continue;
        int x = lo.x + static_cast<int>(column % spanX);
        int z = lo.z + static_cast<int>(column / spanX);
        int current = surface[column];
        int goal = std::min(std::max(target[column], columnLow[column] - 1), columnHigh[column]);
        if (goal == current) continue;

        if (goal < current) {
            for (int y = current; y > goal; --y) {
                Vec3i cell{x, y, z};
                if (limit.contains(cell)) write(params, cell, Block::Air, outcome);
            }
            continue;
        }
        Block fill = current >= columnLow[column] ? world.blockAt(x, current, z) : params.block;
        for (int y = current + 1; y <= goal; ++y) {
            Vec3i cell{x, y, z};
            if (limit.contains(cell)) write(params, cell, fill, outcome);
        }
    }
}

void BrushStroke::paintNeighbourRule(const BrushParams& params, const Selection& limit, bool erode,
                                     BrushResult& outcome) {
    std::vector<Vec3i> targets;
    limit.forEach([&](const Vec3i& cell) {
        Block existing = world.blockAt(cell.x, cell.y, cell.z);
        if (erode == isAir(existing)) return;
        int matching = 0;
        for (const Vec3i& offset : faceOffsets) {
            Vec3i neighbour = cell + offset;
            bool neighbourIsAir = isAir(world.blockAt(neighbour.x, neighbour.y, neighbour.z));
            if (neighbourIsAir == erode) ++matching;
        }
        if (matching >= params.freeNeighbourThreshold) targets.push_back(cell);
    });

    Block value = erode ? Block::Air : params.block;
    for (const Vec3i& cell : targets) write(params, cell, value, outcome);
}

void BrushStroke::paintNaturalize(const BrushParams& params, const Selection& limit,
                                  BrushResult& outcome) {
    if (limit.empty()) return;
    Vec3i lo = limit.min();
    Vec3i hi = limit.max();
    for (int z = lo.z; z <= hi.z; ++z)
        for (int x = lo.x; x <= hi.x; ++x) {
            int depth = -1;
            for (int y = hi.y; y >= lo.y; --y) {
                Block existing = world.blockAt(x, y, z);
                if (isAir(existing)) {
                    depth = -1;
                    continue;
                }
                ++depth;
                Vec3i cell{x, y, z};
                if (!limit.contains(cell)) continue;
                Block want = params.deepBlock;
                if (depth == 0) {
                    want = params.surfaceBlock;
                } else if (depth <= params.soilDepth) {
                    want = params.soilBlock;
                }
                write(params, cell, want, outcome);
            }
        }
}

void BrushStroke::paintGradient(const BrushParams& params, const Selection& limit,
                                BrushResult& outcome) {
    double reach = params.radius + 0.5;
    limit.forEach([&](const Vec3i& cell) {
        double dx = cell.x - params.center.x;
        double dy = cell.y - params.center.y;
        double dz = cell.z - params.center.z;
        double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        double mix = std::min(distance / reach, 1.0);
        double sample = brushHash(params.seed, cell) / 4294967296.0;
        write(params, cell, sample < mix ? params.secondaryBlock : params.block, outcome);
    });
}

}
