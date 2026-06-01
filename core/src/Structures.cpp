#include "vf/Structures.h"

#include <cstdlib>

namespace vf {

namespace {

struct Rng {
    uint32_t state;
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1)); }
    bool chance(int percent) { return static_cast<int>(next() % 100u) < percent; }
};

void addTrunk(std::vector<StructureEdit>& edits, int height) {
    for (int y = 0; y < height; ++y) edits.push_back({{0, y, 0}, Block::Wood});
}

std::vector<StructureEdit> buildOak(Rng& rng) {
    std::vector<StructureEdit> edits;
    int trunk = rng.range(4, 6);
    addTrunk(edits, trunk);

    int canopyBottom = trunk - 2;
    for (int dy = 0; dy <= 2; ++dy) {
        int radius = dy == 2 ? 1 : 2;
        int y = canopyBottom + dy;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0 && y < trunk) continue;   // trunk occupies the core
                int manhattan = std::abs(dx) + std::abs(dz);
                if (manhattan > radius + (dy == 0 ? 1 : 0)) continue;
                if (manhattan == radius + 1 && rng.chance(50)) continue;   // ragged edge
                edits.push_back({{dx, y, dz}, Block::Leaves});
            }
        }
    }
    edits.push_back({{0, trunk, 0}, Block::Leaves});   // crown
    return edits;
}

std::vector<StructureEdit> buildPine(Rng& rng) {
    std::vector<StructureEdit> edits;
    int trunk = rng.range(6, 9);
    addTrunk(edits, trunk);

    for (int y = 2; y <= trunk; ++y) {
        int fromTop = trunk - y;
        int radius = fromTop >= 3 ? 2 : (fromTop >= 1 ? 1 : 0);
        for (int dx = -radius; dx <= radius; ++dx)
            for (int dz = -radius; dz <= radius; ++dz) {
                if (std::abs(dx) + std::abs(dz) > radius) continue;
                if (dx == 0 && dz == 0 && y < trunk) continue;
                edits.push_back({{dx, y, dz}, Block::Leaves});
            }
    }
    edits.push_back({{0, trunk + 1, 0}, Block::Leaves});   // tip
    return edits;
}

std::vector<StructureEdit> buildHut(Rng& rng) {
    std::vector<StructureEdit> edits;
    int height = rng.range(3, 4);
    const int half = 2;

    for (int dx = -half; dx <= half; ++dx)
        for (int dz = -half; dz <= half; ++dz)
            edits.push_back({{dx, 0, dz}, Block::Wood});   // floor

    for (int y = 1; y <= height; ++y)
        for (int dx = -half; dx <= half; ++dx)
            for (int dz = -half; dz <= half; ++dz) {
                bool perimeter = std::abs(dx) == half || std::abs(dz) == half;
                if (!perimeter) continue;
                bool door = dx == 0 && dz == -half && y <= 2;   // doorway on the -z wall
                if (door) continue;
                edits.push_back({{dx, y, dz}, Block::Wood});
            }

    for (int dx = -half; dx <= half; ++dx)
        for (int dz = -half; dz <= half; ++dz)
            edits.push_back({{dx, height + 1, dz}, Block::Leaves});   // thatched roof

    edits.push_back({{0, height, 0}, Block::Glowstone});   // lantern (distinct marker)
    (void)rng;
    return edits;
}

} // namespace

uint32_t structureHash(uint32_t seed, int x, int y, int z) {
    uint32_t h = seed * 2654435761u;
    h ^= static_cast<uint32_t>(x) * 0x9e3779b9u;
    h = (h << 13) | (h >> 19);
    h ^= static_cast<uint32_t>(y) * 0x85ebca6bu;
    h = (h << 7) | (h >> 25);
    h ^= static_cast<uint32_t>(z) * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return h;
}

std::vector<StructureEdit> buildStructure(StructureKind kind, uint32_t seed) {
    Rng rng{seed ? seed : 0x9e3779b9u};
    switch (kind) {
        case StructureKind::OakTree: return buildOak(rng);
        case StructureKind::PineTree: return buildPine(rng);
        case StructureKind::Hut: return buildHut(rng);
    }
    return {};
}

void stampStructure(World& world, const Vec3i& base, const std::vector<StructureEdit>& edits,
                    int maxY) {
    for (const StructureEdit& edit : edits) {
        int gx = base.x + edit.offset.x;
        int gy = base.y + edit.offset.y;
        int gz = base.z + edit.offset.z;
        if (gy < 0 || gy >= maxY) continue;
        if (edit.block == Block::Leaves && world.blockAt(gx, gy, gz) != Block::Air) continue;
        world.setBlock(gx, gy, gz, edit.block);
    }
}

} // namespace vf
