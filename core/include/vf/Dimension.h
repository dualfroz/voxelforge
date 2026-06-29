#pragma once

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

#include "EndGen.h"
#include "NetherGen.h"
#include "World.h"
#include "WorldGen.h"

namespace vf {

enum class DimensionId : uint8_t {
    Overworld = 0,
    Nether = 1,
    End = 2,
    Count,
};

// Per-dimension rules. coordinateScale counts overworld blocks per one block
// of this dimension, so the Nether's 8.0 is the familiar 1:8 ratio.
struct DimensionConfig {
    DimensionId id = DimensionId::Overworld;
    int floorY = 0;
    int ceilingY = 128;
    bool hasSky = true;
    bool hasCeiling = false;
    bool hasDayCycle = true;
    bool waterEvaporates = false;
    bool bedsExplode = false;
    float coordinateScale = 1.0f;
    float ambientLight = 0.0f;
    uint32_t seedSalt = 0u;

    static DimensionConfig forDimension(DimensionId id);
    static const char* nameOf(DimensionId id);

    int height() const { return ceilingY - floorY; }
    int lowestPlaceableY() const { return floorY + 1; }
    int highestPlaceableY() const { return hasCeiling ? ceilingY - 3 : ceilingY - 2; }
    bool containsY(int y) const { return y >= floorY && y < ceilingY; }
    int clampY(int y) const;
};

// One world plus the generator that fills it. Exactly one generator is live
// per dimension; the others stay null.
class Dimension {
public:
    Dimension(const DimensionConfig& dimensionConfig, uint32_t worldSeed);

    const DimensionConfig& config() const { return cfg; }
    DimensionId id() const { return cfg.id; }
    uint32_t seed() const { return derivedSeed; }

    World& world() { return blocks; }
    const World& world() const { return blocks; }

    bool columnGenerated(int columnX, int columnZ) const;
    void generateColumn(int columnX, int columnZ);
    int generateArea(int columnX, int columnZ, int columnRadius);
    std::size_t generatedColumnCount() const { return columns.size(); }

    void tick(int steps) { elapsed += steps; }
    long long ticks() const { return elapsed; }
    // Local time of day in [0, 1). Dimensions without a day cycle are frozen
    // at their ambient level and always report the same fraction.
    float dayFraction() const;

    int highestSolidY(int gx, int gz) const;
    // Lowest feet position at or above fromY that rests on something solid and
    // has `clearance` free blocks above it.
    bool standingSpot(int gx, int gz, int fromY, int clearance, int& outFeetY) const;

    const WorldGen* overworldGenerator() const { return overworld.get(); }
    const NetherGenerator* netherGenerator() const { return nether.get(); }
    const EndGenerator* endGenerator() const { return end.get(); }

private:
    DimensionConfig cfg;
    uint32_t derivedSeed;
    World blocks;
    long long elapsed = 0;
    std::unordered_set<Vec3i, Vec3iHash> columns;
    std::unique_ptr<WorldGen> overworld;
    std::unique_ptr<NetherGenerator> nether;
    std::unique_ptr<EndGenerator> end;
};

// Owns one Dimension per DimensionId; each ticks and generates independently.
class DimensionSet {
public:
    static constexpr int dimensionCount = static_cast<int>(DimensionId::Count);

    explicit DimensionSet(uint32_t worldSeed = 1337u);

    uint32_t seed() const { return baseSeed; }

    Dimension& dimension(DimensionId id) { return *worlds[static_cast<std::size_t>(id)]; }
    const Dimension& dimension(DimensionId id) const { return *worlds[static_cast<std::size_t>(id)]; }

    Dimension& overworld() { return dimension(DimensionId::Overworld); }
    Dimension& nether() { return dimension(DimensionId::Nether); }
    Dimension& end() { return dimension(DimensionId::End); }

    void tickAll(int steps);
    void tickOnly(DimensionId id, int steps) { dimension(id).tick(steps); }
    std::size_t totalChunks() const;

private:
    uint32_t baseSeed;
    std::vector<std::unique_ptr<Dimension>> worlds;
};

struct TravelResult {
    DimensionId from = DimensionId::Overworld;
    DimensionId to = DimensionId::Nether;
    Vec3i source;
    Vec3i scaledTarget;              // pure coordinate mapping
    Vec3i target;                    // scaledTarget clamped into the destination
    bool clampedToFloor = false;
    bool clampedToCeiling = false;
    bool needsPortal = true;
    int searchRadius = 0;
    int nearestIndex = -1;           // index into the candidate list passed to plan()
    int nearestDistanceSquared = 0;
};

// Moves a position between dimensions: scales the horizontal coordinates by
// the ratio of the two coordinate scales, clamps the result into the target's
// vertical bounds, and reports whether an existing portal was close enough to
// reuse or a new one has to be raised.
class DimensionTravel {
public:
    explicit DimensionTravel(DimensionSet& set) : worlds(set) {}

    DimensionSet& dimensions() const { return worlds; }

    static Vec3i scale(const Vec3i& pos, const DimensionConfig& from, const DimensionConfig& to);
    static int searchRadiusFor(DimensionId to);
    static int buildRadiusFor(DimensionId to);

    TravelResult plan(DimensionId from, DimensionId to, const Vec3i& pos) const;
    TravelResult plan(DimensionId from, DimensionId to, const Vec3i& pos,
                      const std::vector<Vec3i>& knownPortals) const;

    bool findSafeSpot(DimensionId to, const Vec3i& near, int radius, int clearance,
                      Vec3i& outSpot) const;
    void carvePlatform(DimensionId to, const Vec3i& spot, int halfWidth, int clearance);

private:
    DimensionSet& worlds;
};

} // namespace vf
