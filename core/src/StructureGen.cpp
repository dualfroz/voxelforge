#include "vf/StructureGen.h"

#include <algorithm>

namespace vf {

namespace {

struct SplitMix32 {
    uint32_t state;

    uint32_t next() {
        state += 0x9E3779B9u;
        uint32_t z = state;
        z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
        z = (z ^ (z >> 13)) * 0xC2B2AE35u;
        z = z ^ (z >> 16);
        return z;
    }

    int nextRange(int lo, int hiInclusive) {
        uint32_t span = static_cast<uint32_t>(hiInclusive - lo + 1);
        return lo + static_cast<int>(next() % span);
    }
};

const Vec3i cardinalDirections[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};

} // namespace

uint32_t chunkStructureHash(uint32_t seed, int chunkX, int chunkZ, uint32_t salt) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint32_t v) { h ^= v; h *= 1099511628211ull; };
    mix(seed);
    mix(static_cast<uint32_t>(chunkX));
    mix(static_cast<uint32_t>(chunkZ));
    mix(salt);
    return static_cast<uint32_t>(h ^ (h >> 32));
}

StructureGen::StructureGen(const StructureGenParams& structureParams) : params(structureParams) {}

bool StructureGen::structureForChunk(int chunkX, int chunkZ, StructureGenKind& outKind) const {
    if (chunkStructureHash(params.seed, chunkX, chunkZ, 0x11u) % static_cast<uint32_t>(params.villageChance) == 0) {
        outKind = StructureGenKind::Village;
        return true;
    }
    if (chunkStructureHash(params.seed, chunkX, chunkZ, 0x22u) % static_cast<uint32_t>(params.templeChance) == 0) {
        outKind = StructureGenKind::DesertTemple;
        return true;
    }
    if (chunkStructureHash(params.seed, chunkX, chunkZ, 0x33u) % static_cast<uint32_t>(params.dungeonChance) == 0) {
        outKind = StructureGenKind::Dungeon;
        return true;
    }
    if (chunkStructureHash(params.seed, chunkX, chunkZ, 0x44u) % static_cast<uint32_t>(params.mineshaftChance) == 0) {
        outKind = StructureGenKind::Mineshaft;
        return true;
    }
    if (chunkStructureHash(params.seed, chunkX, chunkZ, 0x55u) % static_cast<uint32_t>(params.ravineChance) == 0) {
        outKind = StructureGenKind::Ravine;
        return true;
    }
    return false;
}

BoundingBoxI StructureGen::computeBounds(const Vec3i& origin, const std::vector<StructurePiece>& pieces) const {
    Vec3i first = origin + pieces.front().offset;
    BoundingBoxI box{first, first};
    for (const StructurePiece& piece : pieces) {
        Vec3i world = origin + piece.offset;
        box.min.x = std::min(box.min.x, world.x);
        box.min.y = std::min(box.min.y, world.y);
        box.min.z = std::min(box.min.z, world.z);
        box.max.x = std::max(box.max.x, world.x);
        box.max.y = std::max(box.max.y, world.y);
        box.max.z = std::max(box.max.z, world.z);
    }
    return box;
}

StructurePlan StructureGen::buildVillage(int chunkX, int chunkZ, int groundY, uint32_t rngState) const {
    SplitMix32 rng{rngState};
    StructurePlan plan;
    plan.kind = StructureGenKind::Village;
    plan.origin = {chunkX * params.chunkSize, groundY, chunkZ * params.chunkSize};

    int houseCount = rng.nextRange(2, 4);
    int spacing = 7;

    for (int house = 0; house < houseCount; ++house) {
        int hx = (house % 2) * spacing;
        int hz = (house / 2) * spacing;
        int width = 5;
        int depth = 5;
        int height = 4;

        for (int x = 0; x < width; ++x)
            for (int z = 0; z < depth; ++z)
                plan.pieces.push_back({{hx + x, 0, hz + z}, Block::Planks});

        for (int y = 1; y <= height; ++y) {
            for (int x = 0; x < width; ++x) {
                plan.pieces.push_back({{hx + x, y, hz}, Block::Cobblestone});
                plan.pieces.push_back({{hx + x, y, hz + depth - 1}, Block::Cobblestone});
            }
            for (int z = 0; z < depth; ++z) {
                plan.pieces.push_back({{hx, y, hz + z}, Block::Cobblestone});
                plan.pieces.push_back({{hx + width - 1, y, hz + z}, Block::Cobblestone});
            }
        }

        plan.pieces.push_back({{hx + width / 2, 1, hz}, Block::Air});
        plan.pieces.push_back({{hx + width / 2, 2, hz}, Block::Air});

        for (int x = 0; x < width; ++x)
            for (int z = 0; z < depth; ++z)
                plan.pieces.push_back({{hx + x, height + 1, hz + z}, Block::Wood});

        plan.pieces.push_back({{hx + width / 2, height, hz + depth / 2}, Block::Glowstone});
    }

    int pathLength = spacing * ((houseCount + 1) / 2) + spacing;
    for (int x = 0; x < pathLength; ++x)
        plan.pieces.push_back({{x, 0, -1}, Block::Gravel});

    plan.bounds = computeBounds(plan.origin, plan.pieces);
    return plan;
}

StructurePlan StructureGen::buildDungeon(int chunkX, int chunkZ, int groundY, uint32_t rngState) const {
    SplitMix32 rng{rngState};
    StructurePlan plan;
    plan.kind = StructureGenKind::Dungeon;

    int width = rng.nextRange(5, 8);
    int depth = rng.nextRange(5, 8);
    int height = 4;
    int originY = groundY - height - 3;
    plan.origin = {chunkX * params.chunkSize, originY, chunkZ * params.chunkSize};

    for (int y = 0; y <= height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int z = 0; z < depth; ++z) {
                bool shell = (x == 0 || x == width - 1 || z == 0 || z == depth - 1 || y == 0 || y == height);
                Block block = shell ? Block::Cobblestone : Block::Air;
                plan.pieces.push_back({{x, y, z}, block});
            }
        }
    }

    plan.pieces.push_back({{width / 2, 1, depth / 2}, Block::Glowstone});   // mob spawner marker
    plan.pieces.push_back({{1, 1, 1}, Block::Chest});
    plan.pieces.push_back({{width - 2, 1, depth - 2}, Block::Chest});

    plan.bounds = computeBounds(plan.origin, plan.pieces);
    return plan;
}

StructurePlan StructureGen::buildMineshaft(int chunkX, int chunkZ, int groundY, uint32_t rngState) const {
    SplitMix32 rng{rngState};
    StructurePlan plan;
    plan.kind = StructureGenKind::Mineshaft;
    plan.origin = {chunkX * params.chunkSize, groundY - 20, chunkZ * params.chunkSize};

    int segmentCount = rng.nextRange(4, 7);
    int x = 0;
    int z = 0;

    for (int segment = 0; segment < segmentCount; ++segment) {
        Vec3i dir = cardinalDirections[static_cast<std::size_t>(rng.nextRange(0, 3))];
        int length = rng.nextRange(5, 10);

        for (int step = 0; step < length; ++step) {
            for (int y = 0; y < 3; ++y) {
                for (int w = -1; w <= 1; ++w) {
                    int px = x + dir.z * w;
                    int pz = z + dir.x * w;
                    Block block = (y == 0) ? Block::Planks : Block::Air;
                    plan.pieces.push_back({{px, y, pz}, block});
                }
            }
            if (step % 4 == 0) {
                plan.pieces.push_back({{x, 0, z}, Block::Wood});
                plan.pieces.push_back({{x, 1, z}, Block::Wood});
                plan.pieces.push_back({{x, 2, z}, Block::Wood});
            }
            x += dir.x;
            z += dir.z;
        }
    }

    plan.pieces.push_back({{0, 1, 0}, Block::Chest});
    plan.bounds = computeBounds(plan.origin, plan.pieces);
    return plan;
}

StructurePlan StructureGen::buildRavine(int chunkX, int chunkZ, int groundY, uint32_t rngState) const {
    (void)groundY;
    SplitMix32 rng{rngState};
    StructurePlan plan;
    plan.kind = StructureGenKind::Ravine;
    plan.origin = {chunkX * params.chunkSize, 5, chunkZ * params.chunkSize};

    int length = rng.nextRange(20, 39);
    float x = 0.0f;
    float z = static_cast<float>(params.chunkSize) * 0.5f;
    float dx = 1.0f;
    float dz = 0.0f;

    for (int step = 0; step < length; ++step) {
        int width = rng.nextRange(2, 4);
        int depth = rng.nextRange(15, 34);
        int cx = static_cast<int>(x);
        int cz = static_cast<int>(z);

        for (int y = 0; y < depth; ++y)
            for (int w = -width; w <= width; ++w)
                plan.pieces.push_back({{cx + w, y, cz}, Block::Air});

        float turn = (static_cast<float>(rng.nextRange(-100, 100))) / 800.0f;
        dz += turn;
        x += dx;
        z += dz;
    }

    plan.bounds = computeBounds(plan.origin, plan.pieces);
    return plan;
}

StructurePlan StructureGen::buildTemple(int chunkX, int chunkZ, int groundY, uint32_t rngState) const {
    (void)rngState;
    StructurePlan plan;
    plan.kind = StructureGenKind::DesertTemple;
    plan.origin = {chunkX * params.chunkSize, groundY, chunkZ * params.chunkSize};

    int base = 11;
    int height = 6;
    for (int y = 0; y < height; ++y) {
        int inset = y;
        int size = base - inset * 2;
        if (size < 1) break;
        for (int x = 0; x < size; ++x) {
            for (int z = 0; z < size; ++z) {
                bool shell = (x == 0 || x == size - 1 || z == 0 || z == size - 1);
                if (shell || y == 0)
                    plan.pieces.push_back({{inset + x, y, inset + z}, Block::Sandstone});
            }
        }
    }

    int chamberY = -4;
    for (int x = 3; x < 8; ++x)
        for (int z = 3; z < 8; ++z)
            plan.pieces.push_back({{x, chamberY, z}, Block::Air});
    plan.pieces.push_back({{5, chamberY, 5}, Block::Chest});

    plan.bounds = computeBounds(plan.origin, plan.pieces);
    return plan;
}

StructurePlan StructureGen::buildStructure(StructureGenKind kind, int chunkX, int chunkZ, int groundY) const {
    uint32_t rngState = chunkStructureHash(params.seed, chunkX, chunkZ, 0x9E3779B9u ^ static_cast<uint32_t>(kind));

    switch (kind) {
        case StructureGenKind::Village:      return buildVillage(chunkX, chunkZ, groundY, rngState);
        case StructureGenKind::Dungeon:      return buildDungeon(chunkX, chunkZ, groundY, rngState);
        case StructureGenKind::Mineshaft:    return buildMineshaft(chunkX, chunkZ, groundY, rngState);
        case StructureGenKind::Ravine:       return buildRavine(chunkX, chunkZ, groundY, rngState);
        case StructureGenKind::DesertTemple: return buildTemple(chunkX, chunkZ, groundY, rngState);
        case StructureGenKind::Count: break;
    }
    return buildVillage(chunkX, chunkZ, groundY, rngState);
}

void StructureGen::stamp(World& world, const StructurePlan& plan, int maxY) const {
    for (const StructurePiece& piece : plan.pieces) {
        Vec3i worldPos = plan.origin + piece.offset;
        if (worldPos.y < 0 || worldPos.y >= maxY) continue;
        world.setBlock(worldPos.x, worldPos.y, worldPos.z, piece.block);
    }
}

} // namespace vf
