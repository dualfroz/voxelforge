#include "testHarness.h"
#include "vf/EndGen.h"

#include <cmath>
#include <set>

using namespace vf;

namespace {

float distanceFromOrigin(int gx, int gz) {
    float x = static_cast<float>(gx);
    float z = static_cast<float>(gz);
    return std::sqrt(x * x + z * z);
}

bool locateOuterIsland(const EndGenerator& generator, int& outX, int& outZ) {
    const EndGenerator::Params& params = generator.settings();
    for (int gx = params.outerDistance; gx < params.outerDistance + 2000; gx += 4) {
        for (int gz = -600; gz <= 600; gz += 4) {
            if (generator.biomeAt(gx, gz) != EndBiome::OuterIsland) continue;
            outX = gx;
            outZ = gz;
            return true;
        }
    }
    return false;
}

int tallyBlocks(const World& world, Block block, int minX, int minZ, int span, int ceilingY) {
    int total = 0;
    for (int gx = minX; gx < minX + span; ++gx)
        for (int gz = minZ; gz < minZ + span; ++gz)
            for (int gy = 0; gy < ceilingY; ++gy)
                if (world.blockAt(gx, gy, gz) == block) ++total;
    return total;
}

}

VF_TEST(endChunkGenerationRepeatsExactlyOnTheSameSeed) {
    EndGenerator generator;
    Chunk first, second;
    generator.generateChunk(first, {1, 3, -1});
    generator.generateChunk(second, {1, 3, -1});
    CHECK(first.raw() == second.raw());

    EndGenerator::Params other;
    other.seed = 24601u;
    EndGenerator reseeded(other);
    Chunk different;
    reseeded.generateChunk(different, {1, 3, -1});
    CHECK(first.raw() != different.raw());
}

VF_TEST(endColumnIsByteIdenticalAcrossTwoRuns) {
    EndGenerator generator;
    World first, second;
    generator.generateColumn(first, 2, -2);
    generator.generateColumn(second, 2, -2);

    CHECK_EQ(first.chunkCount(), second.chunkCount());
    int mismatches = 0;
    for (const auto& entry : first.allChunks()) {
        const Chunk* twin = second.chunkAt(entry.first);
        if (!twin || twin->raw() != entry.second->raw()) ++mismatches;
    }
    CHECK_EQ(mismatches, 0);
}

VF_TEST(endCentralIslandIsSurroundedByVoid) {
    EndGenerator generator;
    const EndGenerator::Params& params = generator.settings();

    CHECK(!generator.isVoid(0, 0));
    CHECK_EQ(generator.biomeAt(0, 0), EndBiome::CentralIsland);

    int solidNearCentre = 0;
    for (int gx = -40; gx <= 40; gx += 5)
        for (int gz = -40; gz <= 40; gz += 5)
            if (!generator.isVoid(gx, gz)) ++solidNearCentre;
    CHECK(solidNearCentre > 200);

    int intruders = 0;
    int checkedRing = 0;
    for (int gx = -600; gx <= 600; gx += 9) {
        for (int gz = -600; gz <= 600; gz += 9) {
            float distance = distanceFromOrigin(gx, gz);
            if (distance <= params.centralRadius + params.edgeAmplitude) continue;
            if (distance >= static_cast<float>(params.outerDistance)) continue;
            ++checkedRing;
            if (!generator.isVoid(gx, gz)) ++intruders;
            if (generator.biomeAt(gx, gz) != EndBiome::Void) ++intruders;
            if (generator.islandAt(gx, gz).valid()) ++intruders;
        }
    }
    CHECK(checkedRing > 1000);
    CHECK_EQ(intruders, 0);
}

VF_TEST(endIslandEdgeIsRaggedNotACleanCircle) {
    EndGenerator generator;
    const EndGenerator::Params& params = generator.settings();

    int inside = 0;
    int outside = 0;
    for (int step = 0; step < 360; step += 3) {
        float angle = 3.14159265f * static_cast<float>(step) / 180.0f;
        int gx = static_cast<int>(std::lround(std::cos(angle) * params.centralRadius));
        int gz = static_cast<int>(std::lround(std::sin(angle) * params.centralRadius));
        if (generator.isVoid(gx, gz)) ++outside;
        else ++inside;
    }
    CHECK(inside > 0);
    CHECK(outside > 0);
}

VF_TEST(endTerrainIsEndStoneUnderAnEmptySky) {
    EndGenerator generator;
    World world;
    generator.generateColumn(world, 0, 0);

    int surface = generator.surfaceHeight(8, 8);
    CHECK(surface > generator.settings().floorY);
    CHECK_EQ(world.blockAt(8, surface, 8), Block::EndStone);
    CHECK(generator.isStone(8, surface, 8));
    CHECK(!generator.isStone(8, surface + 1, 8));

    int deepBottom = surface;
    while (generator.isStone(8, deepBottom - 1, 8)) --deepBottom;
    CHECK(deepBottom > generator.settings().floorY);
    CHECK_EQ(world.blockAt(8, deepBottom - 1, 8), Block::Air);
    CHECK_EQ(world.blockAt(8, 120, 8), Block::Air);
}

VF_TEST(voidColumnsReportNoSurfaceAtAll) {
    EndGenerator generator;
    CHECK(generator.isVoid(300, 300));
    CHECK_EQ(generator.surfaceHeight(300, 300), generator.settings().floorY - 1);
    CHECK(!generator.isStone(300, 52, 300));
    CHECK_EQ(generator.biomeAt(300, 300), EndBiome::Void);

    World world;
    generator.generateColumn(world, 20, 20);
    CHECK_EQ(tallyBlocks(world, Block::EndStone, 320, 320, 16, 128), 0);
}

VF_TEST(obsidianPillarsRingTheCentreDeterministically) {
    EndGenerator generator;
    const EndGenerator::Params& params = generator.settings();
    std::vector<ObsidianPillar> ring = generator.pillars();
    std::vector<ObsidianPillar> again = generator.pillars();

    CHECK_EQ(ring.size(), static_cast<std::size_t>(params.pillarCount));
    CHECK_EQ(again.size(), ring.size());

    std::set<int> heights;
    int crowned = 0;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        const ObsidianPillar& pillar = ring[index];
        CHECK_EQ(pillar.base.x, again[index].base.x);
        CHECK_EQ(pillar.base.z, again[index].base.z);
        CHECK_EQ(pillar.height, again[index].height);

        float distance = distanceFromOrigin(pillar.base.x, pillar.base.z);
        CHECK(distance > static_cast<float>(params.pillarRing) - 6.0f);
        CHECK(distance < static_cast<float>(params.pillarRing) + 6.0f);
        CHECK(pillar.radius >= 1 && pillar.radius <= 3);
        CHECK(pillar.base.y + pillar.height < params.ceilingY - 1);
        heights.insert(pillar.height);
        if (pillar.crowned) ++crowned;
    }
    CHECK(heights.size() > 1);
    CHECK(crowned > 0);
}

VF_TEST(pillarsAreStampedIntoTheGeneratedWorld) {
    EndGenerator generator;
    World world;
    for (int columnX = -4; columnX <= 4; ++columnX)
        for (int columnZ = -4; columnZ <= 4; ++columnZ)
            generator.generateColumn(world, columnX, columnZ);

    ObsidianPillar pillar = generator.pillars().front();
    CHECK_EQ(world.blockAt(pillar.base.x, pillar.base.y, pillar.base.z), Block::Obsidian);
    CHECK_EQ(world.blockAt(pillar.base.x, pillar.base.y + pillar.height, pillar.base.z),
             Block::Obsidian);
    CHECK_EQ(world.blockAt(pillar.base.x, pillar.base.y + pillar.height + 2, pillar.base.z),
             Block::Air);
}

VF_TEST(exitPortalSitsAtTheOriginOfTheCentralIsland) {
    EndGenerator generator;
    World world;
    for (int columnX = -1; columnX <= 0; ++columnX)
        for (int columnZ = -1; columnZ <= 0; ++columnZ)
            generator.generateColumn(world, columnX, columnZ);

    int surface = generator.surfaceHeight(0, 0);
    CHECK_EQ(world.blockAt(0, surface, 0), Block::EndPortal);
    CHECK_EQ(world.blockAt(1, surface, 1), Block::EndPortal);
    CHECK_EQ(world.blockAt(2, surface, 0), Block::Bedrock);
    CHECK_EQ(world.blockAt(0, surface - 1, 0), Block::Bedrock);
    CHECK_EQ(world.blockAt(-2, surface + 1, -2), Block::Bedrock);
    CHECK_EQ(world.blockAt(0, surface + 1, 0), Block::Air);

    int portalCells = 0;
    for (int dx = -2; dx <= 2; ++dx)
        for (int dz = -2; dz <= 2; ++dz)
            if (world.blockAt(dx, surface, dz) == Block::EndPortal) ++portalCells;
    CHECK_EQ(portalCells, 9);
}

VF_TEST(exitPortalCanBeSwitchedOff) {
    EndGenerator::Params bare;
    bare.exitPortal = false;
    EndGenerator generator(bare);
    World world;
    generator.generateColumn(world, 0, 0);

    CHECK_EQ(world.blockAt(0, generator.surfaceHeight(0, 0), 0), Block::EndStone);
}

VF_TEST(chorusGrowsAStemWithBranchesAndFlowers) {
    EndGenerator generator;
    World world;
    world.setBlock(0, 50, 0, Block::EndStone);

    int written = generator.growChorus(world, {0, 51, 0}, 4711u, 3);
    CHECK(written > 3);

    int stems = 0;
    int flowers = 0;
    int highest = 50;
    int widest = 0;
    for (int gx = -8; gx <= 8; ++gx) {
        for (int gz = -8; gz <= 8; ++gz) {
            for (int gy = 50; gy < 90; ++gy) {
                Block block = world.blockAt(gx, gy, gz);
                if (block != Block::ChorusPlant && block != Block::ChorusFlower) continue;
                if (block == Block::ChorusPlant) ++stems;
                else ++flowers;
                if (gy > highest) highest = gy;
                int reach = std::abs(gx) > std::abs(gz) ? std::abs(gx) : std::abs(gz);
                if (reach > widest) widest = reach;
            }
        }
    }
    CHECK_EQ(stems + flowers, written);
    CHECK(stems > 0);
    CHECK(flowers > 0);
    CHECK(highest > 51);
    CHECK(widest > 0);
    CHECK(widest <= 3);
}

VF_TEST(chorusGrowthIsRepeatableAndBoundedByItsDepth) {
    EndGenerator generator;

    World first, second;
    first.setBlock(0, 50, 0, Block::EndStone);
    second.setBlock(0, 50, 0, Block::EndStone);
    int written = generator.growChorus(first, {0, 51, 0}, 88u, 3);
    CHECK_EQ(generator.growChorus(second, {0, 51, 0}, 88u, 3), written);

    int differences = 0;
    for (int gx = -8; gx <= 8; ++gx)
        for (int gz = -8; gz <= 8; ++gz)
            for (int gy = 50; gy < 90; ++gy)
                if (first.blockAt(gx, gy, gz) != second.blockAt(gx, gy, gz)) ++differences;
    CHECK_EQ(differences, 0);

    World shallow;
    shallow.setBlock(0, 50, 0, Block::EndStone);
    int small = generator.growChorus(shallow, {0, 51, 0}, 88u, 1);
    CHECK(small > 0);
    CHECK(small < written);

    int shallowTop = 50;
    for (int gy = 50; gy < 90; ++gy)
        if (shallow.blockAt(0, gy, 0) != Block::Air) shallowTop = gy;
    CHECK(shallowTop <= 54);
}

VF_TEST(chorusRefusesAnythingButEndStoneUnderneath) {
    EndGenerator generator;
    World world;

    CHECK_EQ(generator.growChorus(world, {0, 51, 0}, 1u, 3), 0);

    world.setBlock(0, 50, 0, Block::Obsidian);
    CHECK_EQ(generator.growChorus(world, {0, 51, 0}, 1u, 3), 0);

    world.setBlock(0, 50, 0, Block::EndStone);
    world.setBlock(0, 51, 0, Block::Purpur);
    CHECK_EQ(generator.growChorus(world, {0, 51, 0}, 1u, 3), 0);
}

VF_TEST(outerIslandsOnlyAppearBeyondTheDistanceThreshold) {
    EndGenerator generator;
    int islandX = 0;
    int islandZ = 0;
    CHECK(locateOuterIsland(generator, islandX, islandZ));
    CHECK(distanceFromOrigin(islandX, islandZ) >=
          static_cast<float>(generator.settings().outerDistance));
    CHECK_EQ(generator.biomeAt(islandX, islandZ), EndBiome::OuterIsland);
    CHECK(!generator.islandAt(islandX, islandZ).central);
    CHECK(generator.islandAt(islandX, islandZ).radius > 0.0f);
}

VF_TEST(outerIslandsCarryChorusAndPurpur) {
    EndGenerator generator;
    int islandX = 0;
    int islandZ = 0;
    CHECK(locateOuterIsland(generator, islandX, islandZ));

    World world;
    int columnX = floorDiv(islandX, chunkSize);
    int columnZ = floorDiv(islandZ, chunkSize);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
            generator.generateColumn(world, columnX + dx, columnZ + dz);

    int minX = (columnX - 1) * chunkSize;
    int minZ = (columnZ - 1) * chunkSize;
    int endStone = tallyBlocks(world, Block::EndStone, minX, minZ, chunkSize * 3, 128);
    int chorusStems = tallyBlocks(world, Block::ChorusPlant, minX, minZ, chunkSize * 3, 128);
    int chorusFlowers = tallyBlocks(world, Block::ChorusFlower, minX, minZ, chunkSize * 3, 128);

    CHECK(endStone > 1000);
    CHECK(chorusStems > 0);
    CHECK(chorusFlowers > 0);

    CHECK_EQ(tallyBlocks(world, Block::EndPortal, minX, minZ, chunkSize * 3, 128), 0);
    CHECK_EQ(tallyBlocks(world, Block::Obsidian, minX, minZ, chunkSize * 3, 128), 0);
}

VF_TEST(endGenerationStaysInsideTheColumnItWasAskedFor) {
    EndGenerator generator;
    int islandX = 0;
    int islandZ = 0;
    CHECK(locateOuterIsland(generator, islandX, islandZ));

    World world;
    int columnX = floorDiv(islandX, chunkSize);
    int columnZ = floorDiv(islandZ, chunkSize);
    generator.generateColumn(world, columnX, columnZ);

    int strays = 0;
    for (const auto& entry : world.allChunks())
        if (entry.first.x != columnX || entry.first.z != columnZ) ++strays;
    CHECK_EQ(strays, 0);
}
