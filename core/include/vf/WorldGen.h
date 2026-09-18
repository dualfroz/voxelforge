#pragma once

#include <cstdint>
#include "World.h"
#include "Noise.h"

namespace vf {

enum class Biome : uint8_t {
    Ocean = 0,
    Beach = 1,
    Plains = 2,
    Forest = 3,
    Desert = 4,
    Mountains = 5,
    Snow = 6,
    Count
};

struct WorldGenParams {
    uint32_t seed = 1337u;
    int seaLevel = 26;
    int maxY = 128;
    float biomeFrequency = 0.0055f;
    float heightFrequency = 0.012f;
    int heightOctaves = 5;
    float caveFrequency = 0.028f;
    float caveThreshold = 0.86f;
    float oreFrequency = 0.14f;
    bool trees = true;
    bool structures = true;
};

class WorldGen {
public:
    explicit WorldGen(const WorldGenParams& params = {});

    Biome biomeAt(int gx, int gz) const;
    int surfaceHeight(int gx, int gz) const;
    bool isCave(int gx, int gy, int gz) const;

    int stackChunkCount() const { return params.maxY / chunkSize; }

    void generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const;

    void generateColumn(World& world, int columnX, int columnZ) const;

    void placeStructures(World& world, int columnX, int columnZ) const;

    const WorldGenParams& settings() const { return params; }

private:
    Biome climateBiome(int gx, int gz) const;
    Block topBlock(Biome biome, int height) const;

    WorldGenParams params;
    PerlinNoise heightNoise;
    PerlinNoise tempNoise;
    PerlinNoise humidNoise;
    PerlinNoise mountainNoise;
    PerlinNoise caveNoise;
    PerlinNoise oreNoise;
    PerlinNoise treeNoise;
};

}
