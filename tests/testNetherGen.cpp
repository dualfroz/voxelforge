#include "testHarness.h"
#include "vf/NetherGen.h"

#include <map>
#include <set>

using namespace vf;

namespace {

std::map<Block, int> censusColumn(const World& world, int columnX, int columnZ, int ceilingY) {
    std::map<Block, int> tally;
    int ox = columnX * chunkSize;
    int oz = columnZ * chunkSize;
    for (int gx = ox; gx < ox + chunkSize; ++gx)
        for (int gz = oz; gz < oz + chunkSize; ++gz)
            for (int gy = 0; gy < ceilingY; ++gy)
                ++tally[world.blockAt(gx, gy, gz)];
    return tally;
}

int countOf(const std::map<Block, int>& tally, Block block) {
    auto it = tally.find(block);
    return it == tally.end() ? 0 : it->second;
}

int findColumnWithBiome(const NetherGenerator& generator, NetherBiome biome, int& outColumnX,
                        int& outColumnZ) {
    for (int columnX = -40; columnX <= 40; ++columnX) {
        for (int columnZ = -40; columnZ <= 40; ++columnZ) {
            int probeX = columnX * chunkSize + 8;
            int probeZ = columnZ * chunkSize + 8;
            if (generator.biomeAt(probeX, probeZ) != biome) continue;
            outColumnX = columnX;
            outColumnZ = columnZ;
            return 1;
        }
    }
    return 0;
}

}

VF_TEST(netherChunkGenerationRepeatsExactlyOnTheSameSeed) {
    NetherGenerator generator;
    Chunk first, second;
    generator.generateChunk(first, {2, 3, -4});
    generator.generateChunk(second, {2, 3, -4});
    CHECK(first.raw() == second.raw());

    Chunk elsewhere;
    generator.generateChunk(elsewhere, {2, 3, 5});
    CHECK(first.raw() != elsewhere.raw());

    NetherGenerator::Params other;
    other.seed = 9001u;
    NetherGenerator reseeded(other);

    World original, alternative;
    generator.generateColumn(original, 2, -4);
    reseeded.generateColumn(alternative, 2, -4);

    int differingChunks = 0;
    for (const auto& entry : original.allChunks()) {
        const Chunk* twin = alternative.chunkAt(entry.first);
        if (!twin || twin->raw() != entry.second->raw()) ++differingChunks;
    }
    CHECK(differingChunks > 0);
}

VF_TEST(netherColumnIsByteIdenticalAcrossTwoRuns) {
    NetherGenerator generator;
    World first, second;
    generator.generateColumn(first, -3, 6);
    generator.generateColumn(second, -3, 6);

    CHECK_EQ(first.chunkCount(), second.chunkCount());
    CHECK_EQ(first.chunkCount(), static_cast<std::size_t>(generator.stackChunkCount()));

    int mismatches = 0;
    for (const auto& entry : first.allChunks()) {
        const Chunk* twin = second.chunkAt(entry.first);
        if (!twin || twin->raw() != entry.second->raw()) ++mismatches;
    }
    CHECK_EQ(mismatches, 0);
}

VF_TEST(netherGenerationStaysInsideTheColumnItWasAskedFor) {
    NetherGenerator generator;
    World world;
    generator.generateColumn(world, 5, 7);

    int strays = 0;
    for (const auto& entry : world.allChunks())
        if (entry.first.x != 5 || entry.first.z != 7) ++strays;
    CHECK_EQ(strays, 0);
}

VF_TEST(netherIsSealedByBedrockAtTheFloorAndTheRoof) {
    NetherGenerator generator;
    const NetherGenerator::Params& params = generator.settings();
    World world;
    generator.generateColumn(world, 0, 0);

    int floorGaps = 0;
    int roofGaps = 0;
    for (int gx = 0; gx < chunkSize; ++gx) {
        for (int gz = 0; gz < chunkSize; ++gz) {
            if (world.blockAt(gx, params.floorY, gz) != Block::Bedrock) ++floorGaps;
            if (world.blockAt(gx, params.ceilingY - 1, gz) != Block::Bedrock) ++roofGaps;
        }
    }
    CHECK_EQ(floorGaps, 0);
    CHECK_EQ(roofGaps, 0);

    int raggedFloor = 0;
    for (int gx = 0; gx < chunkSize; ++gx)
        for (int gz = 0; gz < chunkSize; ++gz)
            if (generator.isBedrock(gx, params.floorY + 2, gz)) ++raggedFloor;
    CHECK(raggedFloor > 0);
    CHECK(raggedFloor < chunkSize * chunkSize);
    CHECK(!generator.isBedrock(0, 60, 0));
}

VF_TEST(netherCavernSitsBetweenTheFloorAndTheRoofMass) {
    NetherGenerator generator;
    const NetherGenerator::Params& params = generator.settings();

    int drownedColumns = 0;
    int dryColumns = 0;
    for (int gx = -400; gx <= 400; gx += 37) {
        for (int gz = -400; gz <= 400; gz += 37) {
            NetherColumn column = generator.columnAt(gx, gz);
            CHECK(column.floorHeight > params.floorY);
            CHECK(column.ceilingHeight < params.ceilingY - 1);
            CHECK(column.cavernHeight() >= 9);
            CHECK_EQ(column.lavaLevel, params.lavaLevel);
            if (column.drowned) ++drownedColumns;
            else ++dryColumns;
        }
    }
    CHECK(drownedColumns > 0);
    CHECK(dryColumns > 0);
}

VF_TEST(netherLavaSeaLeavesNoAirBelowItsSurface) {
    NetherGenerator generator;
    const NetherGenerator::Params& params = generator.settings();

    int airBelowLava = 0;
    int lavaSamples = 0;
    for (int gx = -120; gx <= 120; gx += 11) {
        for (int gz = -120; gz <= 120; gz += 11) {
            NetherColumn column = generator.columnAt(gx, gz);
            for (int gy = params.floorY + 1; gy <= params.lavaLevel; ++gy) {
                Block block = generator.blockAt(column, gx, gy, gz);
                if (block == Block::Air) ++airBelowLava;
                if (block == Block::Lava) ++lavaSamples;
            }
        }
    }
    CHECK_EQ(airBelowLava, 0);
    CHECK(lavaSamples > 0);
}

VF_TEST(netherCarriesAllFourBiomes) {
    NetherGenerator generator;
    std::set<int> seen;
    for (int gx = -900; gx <= 900; gx += 17)
        for (int gz = -900; gz <= 900; gz += 17)
            seen.insert(static_cast<int>(generator.biomeAt(gx, gz)));
    CHECK_EQ(seen.size(), static_cast<std::size_t>(NetherBiome::Count));
}

VF_TEST(netherBiomesLayDownTheirOwnSurfaceBlocks) {
    NetherGenerator generator;
    int soulSandTops = 0;
    int basaltTops = 0;
    int mossTops = 0;
    int foreignSoulTops = 0;

    for (int gx = -600; gx <= 600; gx += 13) {
        for (int gz = -600; gz <= 600; gz += 13) {
            NetherColumn column = generator.columnAt(gx, gz);
            Block top = generator.blockAt(column, gx, column.floorHeight, gz);
            switch (column.biome) {
                case NetherBiome::SoulSandValley:
                    if (top == Block::SoulSand) ++soulSandTops;
                    if (top == Block::Basalt || top == Block::Moss) ++foreignSoulTops;
                    break;
                case NetherBiome::BasaltDeltas:
                    if (top == Block::Basalt) ++basaltTops;
                    break;
                case NetherBiome::CrimsonForest:
                    if (top == Block::Moss) ++mossTops;
                    break;
                default:
                    break;
            }
        }
    }
    CHECK(soulSandTops > 0);
    CHECK(basaltTops > 0);
    CHECK(mossTops > 0);
    CHECK_EQ(foreignSoulTops, 0);
}

VF_TEST(netherSoulSandValleyFillsItselfWithSoulSoil) {
    NetherGenerator generator;
    int columnX = 0, columnZ = 0;
    CHECK(findColumnWithBiome(generator, NetherBiome::SoulSandValley, columnX, columnZ) == 1);

    World world;
    generator.generateColumn(world, columnX, columnZ);
    std::map<Block, int> tally = censusColumn(world, columnX, columnZ, generator.settings().ceilingY);

    CHECK(countOf(tally, Block::SoulSand) > 0);
    CHECK(countOf(tally, Block::SoulSoil) > 0);
    CHECK_EQ(countOf(tally, Block::Basalt), 0);
}

VF_TEST(netherStocksQuartzGlowstoneAndLava) {
    NetherGenerator generator;
    World world;
    for (int columnX = -2; columnX <= 2; ++columnX)
        for (int columnZ = -2; columnZ <= 2; ++columnZ)
            generator.generateColumn(world, columnX, columnZ);

    int quartz = 0, glowstone = 0, lava = 0, netherrack = 0, magma = 0;
    for (int gx = -32; gx < 48; ++gx) {
        for (int gz = -32; gz < 48; ++gz) {
            for (int gy = 0; gy < generator.settings().ceilingY; ++gy) {
                switch (world.blockAt(gx, gy, gz)) {
                    case Block::NetherQuartzOre: ++quartz; break;
                    case Block::Glowstone: ++glowstone; break;
                    case Block::Lava: ++lava; break;
                    case Block::Netherrack: ++netherrack; break;
                    case Block::Magma: ++magma; break;
                    default: break;
                }
            }
        }
    }
    CHECK(quartz > 0);
    CHECK(glowstone > 0);
    CHECK(magma > 0);
    CHECK(lava > 1000);
    CHECK(netherrack > quartz);
}

VF_TEST(netherGlowstoneClingsToTheRoofNotTheFloor) {
    NetherGenerator generator;
    World world;
    for (int columnX = -1; columnX <= 1; ++columnX)
        for (int columnZ = -1; columnZ <= 1; ++columnZ)
            generator.generateColumn(world, columnX, columnZ);

    int lowGlowstone = 0;
    int highGlowstone = 0;
    for (int gx = -16; gx < 32; ++gx) {
        for (int gz = -16; gz < 32; ++gz) {
            NetherColumn column = generator.columnAt(gx, gz);
            for (int gy = 0; gy < generator.settings().ceilingY; ++gy) {
                if (world.blockAt(gx, gy, gz) != Block::Glowstone) continue;
                if (gy >= column.ceilingHeight - 4) ++highGlowstone;
                else ++lowGlowstone;
            }
        }
    }
    CHECK(highGlowstone > 0);
    CHECK_EQ(lowGlowstone, 0);
}

VF_TEST(netherCavesCarveRockWithoutEmptyingIt) {
    NetherGenerator generator;
    int carved = 0;
    int total = 0;
    for (int gx = -200; gx <= 200; gx += 7) {
        for (int gz = -200; gz <= 200; gz += 7) {
            for (int gy = 8; gy < 120; gy += 9) {
                if (generator.isCarved(gx, gy, gz)) ++carved;
                ++total;
            }
        }
    }
    CHECK(carved > 0);
    CHECK(carved < total / 4);
}

VF_TEST(netherDecorationCanBeSwitchedOffWithoutChangingTheRock) {
    NetherGenerator::Params bare;
    bare.decorate = false;
    NetherGenerator plain(bare);
    NetherGenerator decorated;

    World plainWorld, decoratedWorld;
    plain.generateColumn(plainWorld, 1, 1);
    decorated.generateColumn(decoratedWorld, 1, 1);

    int differences = 0;
    int netherrackKept = 0;
    for (int gx = 16; gx < 32; ++gx) {
        for (int gz = 16; gz < 32; ++gz) {
            for (int gy = 0; gy < 128; ++gy) {
                Block bareBlock = plainWorld.blockAt(gx, gy, gz);
                if (bareBlock != decoratedWorld.blockAt(gx, gy, gz)) ++differences;
                else if (bareBlock == Block::Netherrack) ++netherrackKept;
            }
        }
    }
    CHECK(differences > 0);
    CHECK(netherrackKept > 1000);
}
