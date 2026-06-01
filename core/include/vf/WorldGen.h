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
    int maxY = 128;            // world vertical extent in blocks (multiple of chunkSize)
    float biomeFrequency = 0.0055f;
    float heightFrequency = 0.012f;
    int heightOctaves = 5;
    float caveFrequency = 0.028f;
    float caveThreshold = 0.86f;
    float oreFrequency = 0.14f;
    bool trees = true;
    bool structures = true;   // deterministic trees and huts stamped per column
};

// Deterministic infinite terrain: biomes from low-frequency climate noise,
// heightmap scaled per biome, 3D-noise spaghetti caves, and depth-banded ore
// veins. Generating the same chunk twice yields identical blocks.
class WorldGen {
public:
    explicit WorldGen(const WorldGenParams& params = {});

    Biome biomeAt(int gx, int gz) const;
    int surfaceHeight(int gx, int gz) const;
    bool isCave(int gx, int gy, int gz) const;

    int stackChunkCount() const { return params.maxY / chunkSize; }

    // Fills exactly one chunk from its coordinate (no cross-chunk writes).
    void generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const;

    // Fills the whole vertical stack for one column and plants its trees.
    void generateColumn(World& world, int columnX, int columnZ) const;

    // Stamps at most one deterministic structure (tree or hut) into a column,
    // chosen from the seed and column coordinate. Confined to the column footprint
    // so the result is independent of generation order.
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

} // namespace vf
