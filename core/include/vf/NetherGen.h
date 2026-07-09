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

// Vertical profile of a single Nether column. The cavern is the gap between
// the top of the lower mass (floorHeight) and the underside of the roof mass
// (ceilingHeight); everything at or below lavaLevel inside that gap is lava.
struct NetherColumn {
    NetherBiome biome = NetherBiome::NetherWastes;
    int floorHeight = 0;
    int ceilingHeight = 0;
    int lavaLevel = 0;
    bool drowned = false;

    int cavernHeight() const { return ceilingHeight - floorHeight - 1; }
};

// Deterministic Nether generator: a solid rock volume between a bedrock floor
// and a bedrock roof, hollowed out by a cavern profile plus 3D carving noise,
// flooded with a lava sea at a fixed level, and surfaced per Nether biome.
// The same seed and column always yield the same blocks.
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

    // Tunnel network cut through the solid mass, sampled independently of the
    // column profile so it links the floor rock to the roof rock.
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

} // namespace vf
