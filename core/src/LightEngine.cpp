#include "vf/LightEngine.h"

#include <algorithm>

namespace vf {

namespace {

const int offsets[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                           {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
constexpr int downDir = 3;

}

LightEngine::LightEngine(World& world, const Vec3i& origin, const Vec3i& dims)
    : world(world), origin(origin), dims(dims) {
    std::size_t cells = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
    sun.assign(cells, 0);
    block.assign(cells, 0);
}

bool LightEngine::inBox(int lx, int ly, int lz) const {
    return lx >= 0 && lx < dims.x && ly >= 0 && ly < dims.y && lz >= 0 && lz < dims.z;
}

bool LightEngine::occludesLocal(int lx, int ly, int lz) const {
    return occludesLight(world.blockAt(origin.x + lx, origin.y + ly, origin.z + lz));
}

void LightEngine::floodAdd(std::vector<uint8_t>& field, std::vector<Node>& queue, bool isSun) {
    while (!queue.empty()) {
        Node node = queue.back();
        queue.pop_back();
        uint8_t cur = field[index(node.lx, node.ly, node.lz)];
        if (cur != node.level) continue;

        for (int d = 0; d < 6; ++d) {
            int nx = node.lx + offsets[d][0];
            int ny = node.ly + offsets[d][1];
            int nz = node.lz + offsets[d][2];
            if (!inBox(nx, ny, nz) || occludesLocal(nx, ny, nz)) continue;

            uint8_t target;
            if (isSun && d == downDir && cur == maxLightLevel)
                target = static_cast<uint8_t>(maxLightLevel);
            else
                target = cur > 0 ? static_cast<uint8_t>(cur - 1) : 0;
            if (target == 0) continue;

            int ni = index(nx, ny, nz);
            if (field[ni] >= target) continue;
            field[ni] = target;
            queue.push_back({nx, ny, nz, target});
        }
    }
}

void LightEngine::floodRemove(std::vector<uint8_t>& field, std::vector<Node>& removals,
                              std::vector<Node>& reAdd, bool isSun) {
    while (!removals.empty()) {
        Node r = removals.back();
        removals.pop_back();

        for (int d = 0; d < 6; ++d) {
            int nx = r.lx + offsets[d][0];
            int ny = r.ly + offsets[d][1];
            int nz = r.lz + offsets[d][2];
            if (!inBox(nx, ny, nz)) continue;

            int ni = index(nx, ny, nz);
            uint8_t neighbour = field[ni];
            if (neighbour == 0) continue;

            bool dependent;
            if (isSun && d == downDir && r.level == maxLightLevel && neighbour == maxLightLevel)
                dependent = true;
            else
                dependent = neighbour < r.level;

            if (dependent) {
                field[ni] = 0;
                removals.push_back({nx, ny, nz, neighbour});
            } else {
                reAdd.push_back({nx, ny, nz, neighbour});
            }
        }
    }
}

void LightEngine::updateSun(int lx, int ly, int lz) {
    int i = index(lx, ly, lz);
    std::vector<Node> removals{{lx, ly, lz, sun[i]}};
    std::vector<Node> reAdd;
    sun[i] = 0;
    floodRemove(sun, removals, reAdd, true);

    if (!occludesLocal(lx, ly, lz)) {
        if (ly == dims.y - 1) {
            sun[i] = static_cast<uint8_t>(maxLightLevel);
            reAdd.push_back({lx, ly, lz, sun[i]});
        }
        for (int d = 0; d < 6; ++d) {
            int nx = lx + offsets[d][0], ny = ly + offsets[d][1], nz = lz + offsets[d][2];
            if (!inBox(nx, ny, nz) || occludesLocal(nx, ny, nz)) continue;
            uint8_t neighbour = sun[index(nx, ny, nz)];
            if (neighbour > 0) reAdd.push_back({nx, ny, nz, neighbour});
        }
    }
    floodAdd(sun, reAdd, true);
}

void LightEngine::updateBlock(int lx, int ly, int lz, Block newBlock) {
    int i = index(lx, ly, lz);
    std::vector<Node> removals{{lx, ly, lz, block[i]}};
    std::vector<Node> reAdd;
    block[i] = 0;
    floodRemove(block, removals, reAdd, false);

    int emission = blockLightEmission(newBlock);
    if (emission > 0) {
        block[i] = static_cast<uint8_t>(emission);
        reAdd.push_back({lx, ly, lz, block[i]});
    }
    if (!occludesLocal(lx, ly, lz)) {
        for (int d = 0; d < 6; ++d) {
            int nx = lx + offsets[d][0], ny = ly + offsets[d][1], nz = lz + offsets[d][2];
            if (!inBox(nx, ny, nz) || occludesLocal(nx, ny, nz)) continue;
            uint8_t neighbour = block[index(nx, ny, nz)];
            if (neighbour > 0) reAdd.push_back({nx, ny, nz, neighbour});
        }
    }
    floodAdd(block, reAdd, false);
}

void LightEngine::rebuild() {
    std::fill(sun.begin(), sun.end(), 0);
    std::fill(block.begin(), block.end(), 0);

    std::vector<Node> queue;
    for (int lx = 0; lx < dims.x; ++lx) {
        for (int lz = 0; lz < dims.z; ++lz) {
            for (int ly = dims.y - 1; ly >= 0; --ly) {
                if (occludesLocal(lx, ly, lz)) break;
                int i = index(lx, ly, lz);
                sun[i] = static_cast<uint8_t>(maxLightLevel);
                queue.push_back({lx, ly, lz, static_cast<uint8_t>(maxLightLevel)});
            }
        }
    }
    floodAdd(sun, queue, true);

    queue.clear();
    for (int lx = 0; lx < dims.x; ++lx) {
        for (int ly = 0; ly < dims.y; ++ly) {
            for (int lz = 0; lz < dims.z; ++lz) {
                int emission = blockLightEmission(
                    world.blockAt(origin.x + lx, origin.y + ly, origin.z + lz));
                if (emission <= 0) continue;
                int i = index(lx, ly, lz);
                block[i] = static_cast<uint8_t>(emission);
                queue.push_back({lx, ly, lz, static_cast<uint8_t>(emission)});
            }
        }
    }
    floodAdd(block, queue, false);
}

void LightEngine::setBlock(int gx, int gy, int gz, Block newBlock) {
    world.setBlock(gx, gy, gz, newBlock);
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (!inBox(lx, ly, lz)) return;
    updateSun(lx, ly, lz);
    updateBlock(lx, ly, lz, newBlock);
}

uint8_t LightEngine::sunAt(int gx, int gy, int gz) const {
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (ly >= dims.y && lx >= 0 && lx < dims.x && lz >= 0 && lz < dims.z)
        return static_cast<uint8_t>(maxLightLevel);
    if (!inBox(lx, ly, lz)) return 0;
    return sun[index(lx, ly, lz)];
}

uint8_t LightEngine::blockAt(int gx, int gy, int gz) const {
    int lx = gx - origin.x, ly = gy - origin.y, lz = gz - origin.z;
    if (!inBox(lx, ly, lz)) return 0;
    return block[index(lx, ly, lz)];
}

float LightEngine::combinedAt(int gx, int gy, int gz, float dayFactor) const {
    float s = static_cast<float>(sunAt(gx, gy, gz)) * dayFactor;
    float b = static_cast<float>(blockAt(gx, gy, gz));
    float best = s > b ? s : b;
    return best / static_cast<float>(maxLightLevel);
}

}
