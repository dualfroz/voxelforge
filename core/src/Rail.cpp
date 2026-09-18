#include "vf/Rail.h"

namespace vf {

namespace {

bool railAt(const World& world, const Vec3i& cell) {
    return isRailBlock(world.blockAt(cell.x, cell.y, cell.z));
}

bool poweredRailAt(const World& world, const Vec3i& cell) {
    return world.blockAt(cell.x, cell.y, cell.z) == Block::PoweredRail;
}

struct Neighbours {
    bool n = false, s = false, e = false, w = false;
    bool nUp = false, sUp = false, eUp = false, wUp = false;
};

void probe(const World& world, const Vec3i& pos, RailDir dir, bool& conn, bool& ascend) {
    Vec3i flat = pos + railDelta(dir);
    Vec3i up = flat + Vec3i{0, 1, 0};
    Vec3i down = flat + Vec3i{0, -1, 0};
    bool flatRail = railAt(world, flat);
    bool upRail = railAt(world, up);
    bool downRail = railAt(world, down);
    conn = flatRail || upRail || downRail;
    ascend = upRail && !flatRail;
}

Neighbours gatherNeighbours(const World& world, const Vec3i& pos) {
    Neighbours out;
    probe(world, pos, RailDir::North, out.n, out.nUp);
    probe(world, pos, RailDir::South, out.s, out.sUp);
    probe(world, pos, RailDir::East, out.e, out.eUp);
    probe(world, pos, RailDir::West, out.w, out.wUp);
    return out;
}

bool nextPoweredRail(const World& world, const Vec3i& cur, RailDir dir, Vec3i& out) {
    Vec3i candidate = cur + railDelta(dir);
    for (int dy : {0, 1, -1}) {
        Vec3i cell = candidate + Vec3i{0, dy, 0};
        if (poweredRailAt(world, cell)) {
            out = cell;
            return true;
        }
    }
    return false;
}

bool railEndNeighbour(const World& world, const Vec3i& pos, RailShape shape,
                      RailDir endDir, Vec3i& out) {
    Vec3i base = pos + railDelta(endDir);
    if (isAscending(shape) && endDir == ascendUpDir(shape)) base.y += 1;
    Vec3i lower = base + Vec3i{0, -1, 0};
    if (railAt(world, base)) {
        out = base;
        return true;
    }
    if (railAt(world, lower)) {
        out = lower;
        return true;
    }
    return false;
}

}

RailDir opposite(RailDir dir) {
    switch (dir) {
        case RailDir::North: return RailDir::South;
        case RailDir::South: return RailDir::North;
        case RailDir::East:  return RailDir::West;
        case RailDir::West:  return RailDir::East;
    }
    return RailDir::North;
}

Vec3i railDelta(RailDir dir) {
    switch (dir) {
        case RailDir::North: return {0, 0, -1};
        case RailDir::South: return {0, 0, 1};
        case RailDir::East:  return {1, 0, 0};
        case RailDir::West:  return {-1, 0, 0};
    }
    return {0, 0, 0};
}

bool isAscending(RailShape shape) {
    return shape == RailShape::AscendNorth || shape == RailShape::AscendSouth ||
           shape == RailShape::AscendEast || shape == RailShape::AscendWest;
}

RailDir ascendUpDir(RailShape shape) {
    switch (shape) {
        case RailShape::AscendNorth: return RailDir::North;
        case RailShape::AscendSouth: return RailDir::South;
        case RailShape::AscendEast:  return RailDir::East;
        case RailShape::AscendWest:  return RailDir::West;
        default: return RailDir::North;
    }
}

std::pair<RailDir, RailDir> railEnds(RailShape shape) {
    switch (shape) {
        case RailShape::NorthSouth:
        case RailShape::AscendNorth:
        case RailShape::AscendSouth:
            return {RailDir::North, RailDir::South};
        case RailShape::EastWest:
        case RailShape::AscendEast:
        case RailShape::AscendWest:
            return {RailDir::East, RailDir::West};
        case RailShape::CurveSouthEast: return {RailDir::South, RailDir::East};
        case RailShape::CurveSouthWest: return {RailDir::South, RailDir::West};
        case RailShape::CurveNorthWest: return {RailDir::North, RailDir::West};
        case RailShape::CurveNorthEast: return {RailDir::North, RailDir::East};
    }
    return {RailDir::North, RailDir::South};
}

RailShape inferRailShape(const World& world, const Vec3i& pos, bool switched) {
    Neighbours nb = gatherNeighbours(world, pos);
    bool canCurve = world.blockAt(pos.x, pos.y, pos.z) == Block::Rail;
    int count = int(nb.n) + int(nb.s) + int(nb.e) + int(nb.w);

    RailShape shape = RailShape::NorthSouth;
    if (nb.n && nb.s) shape = RailShape::NorthSouth;
    else if (nb.e && nb.w) shape = RailShape::EastWest;
    else if (canCurve && nb.s && nb.e) shape = RailShape::CurveSouthEast;
    else if (canCurve && nb.s && nb.w) shape = RailShape::CurveSouthWest;
    else if (canCurve && nb.n && nb.w) shape = RailShape::CurveNorthWest;
    else if (canCurve && nb.n && nb.e) shape = RailShape::CurveNorthEast;
    else if (nb.n || nb.s) shape = RailShape::NorthSouth;
    else if (nb.e || nb.w) shape = RailShape::EastWest;

    if (switched && canCurve && count >= 3) {
        if (nb.n && nb.s && nb.e) shape = RailShape::CurveSouthEast;
        else if (nb.n && nb.s && nb.w) shape = RailShape::CurveSouthWest;
        else if (nb.e && nb.w && nb.n) shape = RailShape::CurveNorthEast;
        else if (nb.e && nb.w && nb.s) shape = RailShape::CurveSouthEast;
    }

    if (shape == RailShape::NorthSouth) {
        if (nb.nUp) shape = RailShape::AscendNorth;
        else if (nb.sUp) shape = RailShape::AscendSouth;
    } else if (shape == RailShape::EastWest) {
        if (nb.eUp) shape = RailShape::AscendEast;
        else if (nb.wUp) shape = RailShape::AscendWest;
    }

    return shape;
}

RailStep advanceOnRail(const World& world, const Vec3i& pos, RailDir moveDir, bool switched) {
    RailStep step;
    if (!railAt(world, pos)) return step;

    RailShape shape = inferRailShape(world, pos, switched);
    std::pair<RailDir, RailDir> ends = railEnds(shape);
    RailDir cameFrom = opposite(moveDir);

    RailDir exitDir;
    if (cameFrom == ends.first) exitDir = ends.second;
    else if (cameFrom == ends.second) exitDir = ends.first;
    else return step;

    Vec3i out;
    if (!railEndNeighbour(world, pos, shape, exitDir, out)) return step;
    step.nextPos = out;
    step.exitDir = exitDir;
    step.valid = true;
    return step;
}

std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const std::function<bool(const Vec3i&)>& directlyPowered) {
    std::unordered_set<Vec3i, Vec3iHash> active;
    std::vector<Vec3i> sources;
    for (const Vec3i& rail : poweredRails) {
        if (directlyPowered(rail)) {
            active.insert(rail);
            sources.push_back(rail);
        }
    }

    for (const Vec3i& source : sources) {
        RailShape shape = inferRailShape(world, source);
        std::pair<RailDir, RailDir> ends = railEnds(shape);
        for (RailDir dir : {ends.first, ends.second}) {
            Vec3i cur = source;
            for (int step = 1; step <= poweredRailReach; ++step) {
                Vec3i nxt;
                if (!nextPoweredRail(world, cur, dir, nxt)) break;
                active.insert(nxt);
                cur = nxt;
            }
        }
    }
    return active;
}

std::unordered_set<Vec3i, Vec3iHash> activePoweredRails(
    const World& world,
    const std::vector<Vec3i>& poweredRails,
    const RedstoneCircuit& circuit) {
    return activePoweredRails(world, poweredRails, [&](const Vec3i& p) {
        return circuit.poweredAt(p.x, p.y, p.z) || circuit.poweredAt(p.x, p.y - 1, p.z);
    });
}

void RailGraph::build(const World& world, const std::vector<Vec3i>& railCells, bool switched) {
    adjacency.clear();
    for (const Vec3i& cell : railCells) {
        if (!railAt(world, cell)) continue;
        RailShape shape = inferRailShape(world, cell, switched);
        std::pair<RailDir, RailDir> ends = railEnds(shape);
        std::vector<Vec3i> links;
        for (RailDir end : {ends.first, ends.second}) {
            Vec3i out;
            if (railEndNeighbour(world, cell, shape, end, out)) links.push_back(out);
        }
        adjacency.emplace(cell, std::move(links));
    }
}

std::vector<Vec3i> RailGraph::neighbours(const Vec3i& pos) const {
    auto it = adjacency.find(pos);
    if (it == adjacency.end()) return {};
    return it->second;
}

std::vector<Vec3i> RailGraph::trace(const World& world, const Vec3i& start,
                                    RailDir dir, int maxSteps) const {
    std::vector<Vec3i> path;
    std::unordered_set<Vec3i, Vec3iHash> seen;
    Vec3i cur = start;
    RailDir heading = dir;
    for (int step = 0; step < maxSteps; ++step) {
        if (seen.count(cur)) break;
        path.push_back(cur);
        seen.insert(cur);
        RailStep next = advanceOnRail(world, cur, heading);
        if (!next.valid) break;
        cur = next.nextPos;
        heading = next.exitDir;
    }
    return path;
}

int RailGraph::distance(const Vec3i& a, const Vec3i& b) const {
    if (!contains(a) || !contains(b)) return -1;
    if (a == b) return 0;

    std::unordered_map<Vec3i, int, Vec3iHash> depth;
    std::vector<Vec3i> frontier{a};
    depth[a] = 0;
    while (!frontier.empty()) {
        std::vector<Vec3i> nextFrontier;
        for (const Vec3i& node : frontier) {
            int nodeDepth = depth[node];
            auto it = adjacency.find(node);
            if (it == adjacency.end()) continue;
            for (const Vec3i& neighbour : it->second) {
                if (depth.find(neighbour) != depth.end()) continue;
                depth[neighbour] = nodeDepth + 1;
                if (neighbour == b) return nodeDepth + 1;
                nextFrontier.push_back(neighbour);
            }
        }
        frontier.swap(nextFrontier);
    }
    return -1;
}

}
