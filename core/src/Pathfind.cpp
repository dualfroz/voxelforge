#include "vf/Pathfind.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <unordered_map>

namespace vf {

bool isStandable(const World& world, const Vec3i& cell, const PathConfig& config) {
    if (world.solidAt(cell.x, cell.y - 1, cell.z)) {
        for (int h = 0; h < config.mobHeight; ++h)
            if (world.solidAt(cell.x, cell.y + h, cell.z)) return false;
        return true;
    }
    return false;
}

namespace {

const int horiz[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

void successors(const World& world, const Vec3i& cell, const PathConfig& config,
                std::vector<Vec3i>& out) {
    for (auto& dir : horiz) {
        int nx = cell.x + dir[0], nz = cell.z + dir[1];
        for (int dy = config.maxStepUp; dy >= -config.maxDrop; --dy) {
            Vec3i cand{nx, cell.y + dy, nz};
            if (!isStandable(world, cand, config)) continue;
            if (dy > 0 && world.solidAt(cell.x, cell.y + config.mobHeight, cell.z))
                continue;   // ceiling blocks the step up
            out.push_back(cand);
        }
    }
}

int heuristic(const Vec3i& a, const Vec3i& b) {
    return std::abs(a.x - b.x) + std::abs(a.z - b.z);
}

struct Frontier {
    int f;
    int order;
    Vec3i cell;
};

struct FrontierGreater {
    bool operator()(const Frontier& a, const Frontier& b) const {
        if (a.f != b.f) return a.f > b.f;
        return a.order > b.order;   // stable tie-break by insertion order
    }
};

} // namespace

std::vector<Vec3i> findPath(const World& world, const Vec3i& start, const Vec3i& goal,
                            const PathConfig& config) {
    if (!isStandable(world, start, config) || !isStandable(world, goal, config)) return {};
    if (start == goal) return {start};

    std::priority_queue<Frontier, std::vector<Frontier>, FrontierGreater> open;
    std::unordered_map<Vec3i, int, Vec3iHash> gScore;
    std::unordered_map<Vec3i, Vec3i, Vec3iHash> cameFrom;

    gScore[start] = 0;
    int order = 0;
    open.push({heuristic(start, goal), order++, start});

    int expansions = 0;
    std::vector<Vec3i> neighbours;

    while (!open.empty()) {
        Frontier current = open.top();
        open.pop();

        auto known = gScore.find(current.cell);
        if (known == gScore.end() || current.f - heuristic(current.cell, goal) > known->second)
            continue;   // stale entry

        if (current.cell == goal) {
            std::vector<Vec3i> path;
            Vec3i step = goal;
            path.push_back(step);
            while (step != start) {
                step = cameFrom[step];
                path.push_back(step);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (++expansions > config.maxExpansions) return {};

        neighbours.clear();
        successors(world, current.cell, config, neighbours);
        int nextG = known->second + 1;
        for (const Vec3i& next : neighbours) {
            auto it = gScore.find(next);
            if (it != gScore.end() && it->second <= nextG) continue;
            gScore[next] = nextG;
            cameFrom[next] = current.cell;
            open.push({nextG + heuristic(next, goal), order++, next});
        }
    }
    return {};
}

} // namespace vf
