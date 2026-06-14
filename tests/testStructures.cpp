#include "testHarness.h"
#include "vf/Structures.h"
#include "vf/WorldGen.h"

using namespace vf;

namespace {

bool sameEdits(const std::vector<StructureEdit>& a, const std::vector<StructureEdit>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!(a[i].offset == b[i].offset) || a[i].block != b[i].block) return false;
    }
    return true;
}

std::size_t countBlock(const World& world, Block target) {
    std::size_t total = 0;
    for (const auto& entry : world.allChunks())
        for (Block b : entry.second->raw())
            if (b == target) ++total;
    return total;
}

bool worldsEqual(const World& a, const World& b) {
    if (a.chunkCount() != b.chunkCount()) return false;
    for (const auto& entry : a.allChunks()) {
        const Chunk* other = b.chunkAt(entry.first);
        if (!other) return false;
        if (entry.second->raw() != other->raw()) return false;
    }
    return true;
}

} // namespace

VF_TEST(structureBuildIsDeterministicPerSeed) {
    std::vector<StructureEdit> oakA = buildStructure(StructureKind::OakTree, 12345u);
    std::vector<StructureEdit> oakB = buildStructure(StructureKind::OakTree, 12345u);
    CHECK(sameEdits(oakA, oakB));   // same seed, same tree

    std::vector<StructureEdit> oakC = buildStructure(StructureKind::OakTree, 999u);
    CHECK(!sameEdits(oakA, oakC));  // different seed, different tree
    CHECK(!oakA.empty());
}

VF_TEST(structureHutHasLanternFloorAndDoor) {
    std::vector<StructureEdit> hut = buildStructure(StructureKind::Hut, 7u);
    bool lantern = false, floorCentre = false, doorOpen = true;
    for (const StructureEdit& e : hut) {
        if (e.block == Block::Glowstone) lantern = true;
        if (e.offset == Vec3i(0, 0, 0) && e.block == Block::Wood) floorCentre = true;
        if (e.offset == Vec3i(0, 1, -2)) doorOpen = false;   // doorway cell must stay empty
    }
    CHECK(lantern);
    CHECK(floorCentre);
    CHECK(doorOpen);
}

VF_TEST(structureStampWritesBlocks) {
    World world;
    std::vector<StructureEdit> oak = buildStructure(StructureKind::OakTree, 42u);
    stampStructure(world, {100, 40, 100}, oak, 128);
    CHECK(world.blockAt(100, 40, 100) == Block::Wood);   // trunk base
    CHECK(countBlock(world, Block::Leaves) > 0);         // canopy present
}

VF_TEST(structureLeavesDoNotOverwriteSolids) {
    World world;
    world.setBlock(100, 41, 100, Block::Stone);   // pre-existing solid where a leaf would go
    std::vector<StructureEdit> oak = buildStructure(StructureKind::OakTree, 42u);
    stampStructure(world, {100, 40, 100}, oak, 128);
    CHECK(world.blockAt(100, 41, 100) == Block::Wood || world.blockAt(100, 41, 100) == Block::Stone);
    // trunk (wood) may overwrite, but no leaf replaced the stone
}

VF_TEST(worldGenWithStructuresIsDeterministic) {
    WorldGenParams params;
    params.structures = true;
    WorldGen genA(params), genB(params);
    World a, b;
    for (int cx = 0; cx < 5; ++cx)
        for (int cz = 0; cz < 5; ++cz) {
            genA.generateColumn(a, cx, cz);
            genB.generateColumn(b, cx, cz);
        }
    CHECK(worldsEqual(a, b));   // same seed -> identical structures and terrain
}

VF_TEST(worldGenDifferentSeedDiffersWithStructures) {
    WorldGenParams pa;
    pa.seed = 1337u;
    WorldGenParams pb;
    pb.seed = 2024u;
    WorldGen genA(pa), genB(pb);
    World a, b;
    for (int cx = 0; cx < 5; ++cx)
        for (int cz = 0; cz < 5; ++cz) {
            genA.generateColumn(a, cx, cz);
            genB.generateColumn(b, cx, cz);
        }
    CHECK(!worldsEqual(a, b));
}

VF_TEST(worldGenStampsHutsWhenEnabled) {
    WorldGenParams on;
    on.structures = true;
    WorldGen gen(on);
    World world;
    for (int cx = 0; cx < 16; ++cx)
        for (int cz = 0; cz < 16; ++cz)
            gen.generateColumn(world, cx, cz);
    CHECK(countBlock(world, Block::Glowstone) > 0);   // lantern proves a hut was stamped

    WorldGenParams off;
    off.structures = false;
    WorldGen bare(off);
    World plain;
    for (int cx = 0; cx < 16; ++cx)
        for (int cz = 0; cz < 16; ++cz)
            bare.generateColumn(plain, cx, cz);
    CHECK_EQ(countBlock(plain, Block::Glowstone), static_cast<std::size_t>(0));
}
