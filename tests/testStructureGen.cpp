#include "testHarness.h"
#include "vf/StructureGen.h"

#include <array>
#include <unordered_map>

using namespace vf;

namespace {

bool piecesEqual(const std::vector<StructurePiece>& a, const std::vector<StructurePiece>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].offset != b[i].offset) return false;
        if (a[i].block != b[i].block) return false;
    }
    return true;
}

int countBlock(const StructurePlan& plan, Block block) {
    int count = 0;
    for (const StructurePiece& piece : plan.pieces)
        if (piece.block == block) ++count;
    return count;
}

}

VF_TEST(structureGenBoundingBoxContainsAndIntersects) {
    BoundingBoxI box{{0, 0, 0}, {10, 10, 10}};
    CHECK(box.contains({5, 5, 5}));
    CHECK(box.contains({0, 0, 0}));
    CHECK(box.contains({10, 10, 10}));
    CHECK(!box.contains({11, 5, 5}));
    CHECK(!box.contains({5, -1, 5}));

    BoundingBoxI overlapping{{5, 5, 5}, {15, 15, 15}};
    BoundingBoxI disjoint{{20, 20, 20}, {30, 30, 30}};
    CHECK(box.intersects(overlapping));
    CHECK(!box.intersects(disjoint));
}

VF_TEST(structureGenChunkStructureHashIsDeterministicAndVaries) {
    uint32_t first = chunkStructureHash(1337u, 3, -5, 0x11u);
    uint32_t second = chunkStructureHash(1337u, 3, -5, 0x11u);
    CHECK_EQ(first, second);

    uint32_t differentSalt = chunkStructureHash(1337u, 3, -5, 0x22u);
    uint32_t differentCoord = chunkStructureHash(1337u, 4, -5, 0x11u);
    uint32_t differentSeed = chunkStructureHash(7u, 3, -5, 0x11u);
    CHECK(first != differentSalt);
    CHECK(first != differentCoord);
    CHECK(first != differentSeed);
}

VF_TEST(structureGenDistributionCoversEveryKind) {
    StructureGen gen;
    std::array<int, 5> counts{};
    for (int cx = -60; cx <= 60; ++cx) {
        for (int cz = -60; cz <= 60; ++cz) {
            StructureGenKind kind;
            if (gen.structureForChunk(cx, cz, kind)) counts[static_cast<std::size_t>(kind)] += 1;
        }
    }
    for (int count : counts) CHECK(count > 0);
}

VF_TEST(structureGenChunkDecisionIsDeterministic) {
    StructureGen gen;
    for (int cx = -30; cx <= 30; cx += 3) {
        for (int cz = -30; cz <= 30; cz += 3) {
            StructureGenKind a, b;
            bool hitA = gen.structureForChunk(cx, cz, a);
            bool hitB = gen.structureForChunk(cx, cz, b);
            CHECK_EQ(hitA, hitB);
            if (hitA) CHECK(a == b);
        }
    }
}

VF_TEST(structureGenBuildIsDeterministicForSameInputs) {
    StructureGen gen;
    StructurePlan a = gen.buildStructure(StructureGenKind::Village, 3, 4, 40);
    StructurePlan b = gen.buildStructure(StructureGenKind::Village, 3, 4, 40);
    CHECK(piecesEqual(a.pieces, b.pieces));
    CHECK(a.origin == b.origin);
    CHECK(a.bounds.min == b.bounds.min);
    CHECK(a.bounds.max == b.bounds.max);
}

VF_TEST(structureGenBuildDiffersAcrossChunksAndSeeds) {
    StructureGen genA(StructureGenParams{11u, 16, 42, 37, 53, 61, 71});
    StructureGen genB(StructureGenParams{99u, 16, 42, 37, 53, 61, 71});

    StructurePlan village1 = genA.buildStructure(StructureGenKind::Village, 1, 1, 40);
    StructurePlan village2 = genA.buildStructure(StructureGenKind::Village, 2, 5, 40);
    StructurePlan village3 = genB.buildStructure(StructureGenKind::Village, 1, 1, 40);

    CHECK(!piecesEqual(village1.pieces, village2.pieces));
    CHECK(!piecesEqual(village1.pieces, village3.pieces));
}

VF_TEST(structureGenAllPiecesFallInsideBounds) {
    StructureGen gen;
    StructureGenKind kinds[5] = {
        StructureGenKind::Village, StructureGenKind::Dungeon, StructureGenKind::Mineshaft,
        StructureGenKind::Ravine, StructureGenKind::DesertTemple,
    };
    for (StructureGenKind kind : kinds) {
        StructurePlan plan = gen.buildStructure(kind, 5, -7, 50);
        CHECK(!plan.pieces.empty());
        for (const StructurePiece& piece : plan.pieces) {
            Vec3i world = plan.origin + piece.offset;
            CHECK(plan.bounds.contains(world));
        }
    }
}

VF_TEST(structureGenVillageHasHousesAndLanterns) {
    StructureGen gen;
    StructurePlan plan = gen.buildStructure(StructureGenKind::Village, 0, 0, 40);
    CHECK(countBlock(plan, Block::Cobblestone) > 0);
    CHECK(countBlock(plan, Block::Planks) > 0);
    CHECK(countBlock(plan, Block::Wood) > 0);
    CHECK(countBlock(plan, Block::Glowstone) > 0);
    CHECK(countBlock(plan, Block::Air) > 0);
}

VF_TEST(structureGenDungeonHasHollowRoomSpawnerAndTwoChests) {
    StructureGen gen;
    StructurePlan plan = gen.buildStructure(StructureGenKind::Dungeon, 2, 2, 60);
    CHECK_EQ(countBlock(plan, Block::Chest), 2);
    CHECK(countBlock(plan, Block::Glowstone) >= 1);
    CHECK(countBlock(plan, Block::Cobblestone) > 0);
    CHECK(countBlock(plan, Block::Air) > 0);
    CHECK(plan.origin.y < 60);
}

VF_TEST(structureGenMineshaftHasCorridorsSupportsAndChest) {
    StructureGen gen;
    StructurePlan plan = gen.buildStructure(StructureGenKind::Mineshaft, -3, 8, 70);
    CHECK(countBlock(plan, Block::Planks) > 0);
    CHECK(countBlock(plan, Block::Wood) > 0);
    CHECK(countBlock(plan, Block::Air) > 0);
    CHECK_EQ(countBlock(plan, Block::Chest), 1);
}

VF_TEST(structureGenRavineIsAPureCarve) {
    StructureGen gen;
    StructurePlan plan = gen.buildStructure(StructureGenKind::Ravine, 9, -2, 40);
    CHECK(!plan.pieces.empty());
    for (const StructurePiece& piece : plan.pieces) CHECK(piece.block == Block::Air);
    CHECK(plan.bounds.max.y - plan.bounds.min.y > 5);
}

VF_TEST(structureGenDesertTempleHasSandstoneShellAndHiddenChest) {
    StructureGen gen;
    StructurePlan plan = gen.buildStructure(StructureGenKind::DesertTemple, 4, 4, 40);
    CHECK(countBlock(plan, Block::Sandstone) > 20);
    CHECK_EQ(countBlock(plan, Block::Chest), 1);
    CHECK(countBlock(plan, Block::Air) > 0);
    bool hasBelowGround = false;
    for (const StructurePiece& piece : plan.pieces)
        if (piece.offset.y < 0) hasBelowGround = true;
    CHECK(hasBelowGround);
}

VF_TEST(structureGenStampWritesEveryPieceWithinRange) {
    StructureGen gen;
    World world;
    StructurePlan plan = gen.buildStructure(StructureGenKind::Dungeon, 1, 1, 30);
    gen.stamp(world, plan, 128);

    std::unordered_map<Vec3i, Block, Vec3iHash> oracle;
    for (const StructurePiece& piece : plan.pieces)
        oracle[plan.origin + piece.offset] = piece.block;

    CHECK(!oracle.empty());
    for (const auto& entry : oracle)
        CHECK(world.blockAt(entry.first.x, entry.first.y, entry.first.z) == entry.second);
}

VF_TEST(structureGenStampClipsPiecesOutsideMaxY) {
    StructureGen gen;
    World world;
    StructurePlan plan = gen.buildStructure(StructureGenKind::DesertTemple, 0, 0, 40);

    int splitY = (plan.bounds.min.y + plan.bounds.max.y) / 2;
    gen.stamp(world, plan, splitY);

    std::unordered_map<Vec3i, Block, Vec3iHash> oracle;
    std::vector<Vec3i> outOfRange;
    for (const StructurePiece& piece : plan.pieces) {
        Vec3i worldPos = plan.origin + piece.offset;
        if (worldPos.y < 0 || worldPos.y >= splitY) {
            outOfRange.push_back(worldPos);
        } else {
            oracle[worldPos] = piece.block;
        }
    }

    CHECK(!oracle.empty());
    CHECK(!outOfRange.empty());

    for (const auto& entry : oracle)
        CHECK(world.blockAt(entry.first.x, entry.first.y, entry.first.z) == entry.second);

    for (const Vec3i& pos : outOfRange)
        if (oracle.find(pos) == oracle.end())
            CHECK(world.blockAt(pos.x, pos.y, pos.z) == Block::Air);
}
