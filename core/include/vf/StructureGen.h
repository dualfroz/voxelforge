#pragma once

#include <cstdint>
#include <vector>
#include "World.h"

namespace vf {

enum class StructureGenKind : uint8_t {
    Village = 0,
    Dungeon = 1,
    Mineshaft = 2,
    Ravine = 3,
    DesertTemple = 4,
    Count,
};

struct BoundingBoxI {
    Vec3i min;
    Vec3i max;

    bool contains(const Vec3i& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }

    bool intersects(const BoundingBoxI& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

struct StructurePiece {
    Vec3i offset;
    Block block;
};

struct StructurePlan {
    StructureGenKind kind = StructureGenKind::Village;
    Vec3i origin;
    BoundingBoxI bounds;
    std::vector<StructurePiece> pieces;
};

struct StructureGenParams {
    uint32_t seed = 1337u;
    int chunkSize = 16;
    int villageChance = 42;
    int dungeonChance = 37;
    int mineshaftChance = 53;
    int ravineChance = 61;
    int templeChance = 71;
};

uint32_t chunkStructureHash(uint32_t seed, int chunkX, int chunkZ, uint32_t salt);

class StructureGen {
public:
    explicit StructureGen(const StructureGenParams& params = {});

    bool structureForChunk(int chunkX, int chunkZ, StructureGenKind& outKind) const;

    StructurePlan buildStructure(StructureGenKind kind, int chunkX, int chunkZ, int groundY) const;

    void stamp(World& world, const StructurePlan& plan, int maxY) const;

    const StructureGenParams& settings() const { return params; }

private:
    StructurePlan buildVillage(int chunkX, int chunkZ, int groundY, uint32_t rngState) const;
    StructurePlan buildDungeon(int chunkX, int chunkZ, int groundY, uint32_t rngState) const;
    StructurePlan buildMineshaft(int chunkX, int chunkZ, int groundY, uint32_t rngState) const;
    StructurePlan buildRavine(int chunkX, int chunkZ, int groundY, uint32_t rngState) const;
    StructurePlan buildTemple(int chunkX, int chunkZ, int groundY, uint32_t rngState) const;

    BoundingBoxI computeBounds(const Vec3i& origin, const std::vector<StructurePiece>& pieces) const;

    StructureGenParams params;
};

}
