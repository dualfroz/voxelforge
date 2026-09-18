#include "vf/Lighting.h"

#include <deque>

namespace vf {

namespace {

struct Node {
    int lx, ly, lz;
    uint8_t level;
};

const int neighbourOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                   {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

}

LightField computeLight(const World& world, const Vec3i& origin, const Vec3i& dims) {
    LightField field;
    field.origin = origin;
    field.dims = dims;
    std::size_t cells = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
    field.sun.assign(cells, 0);
    field.block.assign(cells, 0);

    auto occludes = [&](int lx, int ly, int lz) {
        return occludesLight(world.blockAt(origin.x + lx, origin.y + ly, origin.z + lz));
    };

    std::deque<Node> queue;

    for (int lx = 0; lx < dims.x; ++lx) {
        for (int lz = 0; lz < dims.z; ++lz) {
            for (int ly = dims.y - 1; ly >= 0; --ly) {
                if (occludes(lx, ly, lz)) break;
                int i = field.index(lx, ly, lz);
                field.sun[i] = static_cast<uint8_t>(maxLight);
                queue.push_back({lx, ly, lz, static_cast<uint8_t>(maxLight)});
            }
        }
    }

    while (!queue.empty()) {
        Node node = queue.front();
        queue.pop_front();
        uint8_t current = field.sun[field.index(node.lx, node.ly, node.lz)];
        if (current != node.level) continue;

        for (auto& off : neighbourOffset) {
            int nx = node.lx + off[0], ny = node.ly + off[1], nz = node.lz + off[2];
            if (!field.localInBox(nx, ny, nz) || occludes(nx, ny, nz)) continue;
            bool straightDown = off[1] == -1 && current == maxLight;
            uint8_t target = straightDown ? static_cast<uint8_t>(maxLight)
                                          : static_cast<uint8_t>(current - 1);
            if (target == 0) continue;
            int ni = field.index(nx, ny, nz);
            if (field.sun[ni] >= target) continue;
            field.sun[ni] = target;
            queue.push_back({nx, ny, nz, target});
        }
    }

    queue.clear();
    for (int lx = 0; lx < dims.x; ++lx) {
        for (int ly = 0; ly < dims.y; ++ly) {
            for (int lz = 0; lz < dims.z; ++lz) {
                int emission = blockLightEmission(
                    world.blockAt(origin.x + lx, origin.y + ly, origin.z + lz));
                if (emission <= 0) continue;
                int i = field.index(lx, ly, lz);
                field.block[i] = static_cast<uint8_t>(emission);
                queue.push_back({lx, ly, lz, static_cast<uint8_t>(emission)});
            }
        }
    }

    while (!queue.empty()) {
        Node node = queue.front();
        queue.pop_front();
        uint8_t current = field.block[field.index(node.lx, node.ly, node.lz)];
        if (current != node.level || current <= 1) continue;

        for (auto& off : neighbourOffset) {
            int nx = node.lx + off[0], ny = node.ly + off[1], nz = node.lz + off[2];
            if (!field.localInBox(nx, ny, nz) || occludes(nx, ny, nz)) continue;
            uint8_t target = static_cast<uint8_t>(current - 1);
            int ni = field.index(nx, ny, nz);
            if (field.block[ni] >= target) continue;
            field.block[ni] = target;
            queue.push_back({nx, ny, nz, target});
        }
    }

    return field;
}

}
