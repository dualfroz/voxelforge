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
    Vec3i offset;   // relative to StructurePlan::origin
    Block block;
};

struct StructurePlan {
    StructureGenKind kind = StructureGenKind::Village;
    Vec3i origin;                          // world-space anchor
    BoundingBoxI bounds;                   // world-space, covers every piece
    std::vector<StructurePiece> pieces;
};

struct StructureGenParams {
    uint32_t seed = 1337u;
    int chunkSize = 16;
    int villageChance = 42;    // 1 in N chunks host a village
    int dungeonChance = 37;
    int mineshaftChance = 53;
    int ravineChance = 61;
    int templeChance = 71;
};

// Deterministic 32-bit mix of seed, chunk coordinate and a salt distinguishing
// use sites. Named apart from vf::structureHash in Structures.h to avoid a
// duplicate symbol when both translation units link together.
uint32_t chunkStructureHash(uint32_t seed, int chunkX, int chunkZ, uint32_t salt);

// Deterministic per-chunk structure placement and piece-list generation:
// village (house grid + paths), dungeon (room + spawner + chests), mineshaft
// (branching corridors + supports), ravine (carved canyon) and desert temple
// (stepped pyramid + hidden chamber). Every method is a pure function of its
// inputs, so identical seed and chunk coordinates always produce an
// identical plan.
class StructureGen {
public:
    explicit StructureGen(const StructureGenParams& params = {});

    // Decides whether chunk (chunkX, chunkZ) hosts a structure and which
    // kind, in priority order village > temple > dungeon > mineshaft > ravine.
    bool structureForChunk(int chunkX, int chunkZ, StructureGenKind& outKind) const;

    // Builds the full piece list and bounding box for `kind` anchored at
    // groundY within chunk (chunkX, chunkZ).
    StructurePlan buildStructure(StructureGenKind kind, int chunkX, int chunkZ, int groundY) const;

    // Writes every piece into the world, skipping pieces that fall outside
    // [0, maxY).
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

} // namespace vf
