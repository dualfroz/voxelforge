#pragma once

#include <cstdint>
#include <vector>

#include "Noise.h"
#include "NoiseAdv.h"
#include "World.h"

namespace vf {

enum class EndBiome : uint8_t {
    Void = 0,
    CentralIsland = 1,
    OuterIsland = 2,
    Count,
};

// A lens of end stone floating in the void. `thickness` is the half-depth at
// the island centre; it tapers to nothing at the rim.
struct EndIsland {
    Vec3i center;
    float radius = 0.0f;
    float thickness = 0.0f;
    bool central = false;

    bool valid() const { return radius > 0.0f; }
};

struct ObsidianPillar {
    Vec3i base;
    int radius = 1;
    int height = 0;
    bool crowned = false;
};

// Deterministic End generator: one central end stone island ringed by void,
// an outer island belt beyond a distance threshold, obsidian pillars on a
// fixed ring around the origin, a bedrock exit portal at the centre and
// recursive chorus growth on the outer islands.
class EndGenerator {
public:
    struct Params {
        uint32_t seed = 1337u;
        int floorY = 0;
        int ceilingY = 128;
        int islandY = 52;
        float centralRadius = 88.0f;
        float centralThickness = 24.0f;
        float edgeFrequency = 0.017f;
        float edgeAmplitude = 15.0f;
        int outerDistance = 640;
        int outerCellSize = 160;
        int outerChance = 3;
        int pillarCount = 10;
        int pillarRing = 43;
        bool chorus = true;
        bool exitPortal = true;
    };

    EndGenerator();
    explicit EndGenerator(const Params& endParams);

    const Params& settings() const { return params; }
    int stackChunkCount() const { return (params.ceilingY - params.floorY) / chunkSize; }

    EndBiome biomeAt(int gx, int gz) const;
    EndIsland islandAt(int gx, int gz) const;

    bool isVoid(int gx, int gz) const;
    // Top end stone block of the column, or floorY - 1 when the column is void.
    int surfaceHeight(int gx, int gz) const;
    bool isStone(int gx, int gy, int gz) const;

    std::vector<ObsidianPillar> pillars() const;

    void generateChunk(Chunk& chunk, const Vec3i& chunkCoord) const;
    void generateColumn(World& world, int columnX, int columnZ) const;
    void decorateColumn(World& world, int columnX, int columnZ) const;

    // Recursive chorus stem. Each level grows one to three vertical segments,
    // sprouts up to two side branches that recurse on their own, and caps
    // every dead end with a flower. Returns the number of blocks written.
    int growChorus(World& world, const Vec3i& base, uint32_t roll, int maxDepth) const;

    void placePillars(World& world) const;
    void placeExitPortal(World& world) const;

private:
    EndIsland centralIsland() const;
    EndIsland outerIslandFor(int cellX, int cellZ) const;
    bool columnExtent(int gx, int gz, int& topY, int& bottomY) const;

    int growBranch(World& world, Vec3i pos, int depth, int maxDepth, uint32_t& rngState) const;
    void stampPillars(World& world, int minX, int minZ, int maxX, int maxZ) const;
    void stampExitPortal(World& world, int minX, int minZ, int maxX, int maxZ) const;
    void raisePurpurSpire(World& world, int gx, int gz, int top, uint32_t roll) const;

    Params params;
    PerlinNoise edgeNoise;
    PerlinNoise surfaceNoise;
    NoiseAdv outerNoise;
};

} // namespace vf
