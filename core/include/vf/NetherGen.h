#pragma once

#include <cstdint>

#include "Noise.h"
#include "NoiseAdv.h"
#include "World.h"

namespace vf {

enum class NetherBiome : uint8_t {
    NetherWastes = 0,
    SoulSandValley = 1,
    BasaltDeltas = 2,
    CrimsonForest = 3,
    Count,
};

struct NetherColumn {
    NetherBiome biome = NetherBiome::NetherWastes;
    int floorHeight = 0;
    int ceilingHeight = 0;
    int lavaLevel = 0;
    bool drowned = false;

    int cavernHeight() const { return ceilingHeight - floorHeight - 1; }
};

class NetherGenerator {
public:
    struct Params {
        uint32_t seed = 1337u;
        int floorY = 0;
        int ceilingY = 128;
        int lavaLevel = 31;
        int bedrockThickness = 4;
        int roofThickness = 12;
        float biomeFrequency = 0.0046f;
        float floorFrequency = 0.017f;
        float ceilingFrequency = 0.013f;
        float caveFrequency = 0.031f;
        float caveThreshold = 0.87f;
        float blobThreshold = 0.46f;
        float quartzFrequency = 0.125f;
        float quartzThreshold = 0.60f;
        bool decorate = true;
    };

    NetherGenerator();
    explicit NetherGenerator(const Params& netherParams);

    const Params& settings() const { return params; }
    int stackChunkCount() const { return (params.ceilingY - params.floorY) / chunkSize; }

    NetherBiome biomeAt(int gx, int gz) const;
    NetherColumn columnAt(int gx, int gz) const;

    bool isCarved(int gx, int gy, int gz) const;
    bool isBedrock(int gx, int gy, int gz) const;

    Block blockAt(const NetherColumn& column, int gx, int gy, int gz) const;

    void generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const;
    void generateColumn(World& world, int columnX, int columnZ) const;
    void decorateColumn(World& world, int columnX, int columnZ) const;

private:
    bool isBlob(int gx, int gy, int gz) const;
    Block topFor(NetherBiome biome) const;
    Block fillerFor(NetherBiome biome) const;

    void raiseBasaltPillar(World& world, int gx, int gz, const NetherColumn& column, uint32_t roll) const;
    void pourLavafall(World& world, int gx, int gz, const NetherColumn& column) const;
    void hangGlowstone(World& world, int gx, int gz, const NetherColumn& column, uint32_t roll) const;
    void plantFungus(World& world, int gx, int gz, const NetherColumn& column, uint32_t roll) const;

    Params params;
    PerlinNoise floorNoise;
    PerlinNoise ceilingNoise;
    PerlinNoise caveNoise;
    PerlinNoise blobNoise;
    PerlinNoise oreNoise;
    NoiseAdv biomeNoise;
};

}
