#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "World.h"
#include "Math.h"
#include "BiomeGen.h"
#include "Weather.h"

namespace vf {

struct ClimateChange {
    Vec3i pos;
    Block from;
    Block to;
};

struct ClimateConfig {
    int seaLevel = 26;
    // Temperature drop per block of elevation above sea level.
    float heightLapse = 0.006f;
    // Temperature drop per block of distance from the z=0 "equator".
    float latitudeLapse = 0.0009f;
    // Cold enough to lay snow and freeze exposed water.
    float freezeTemperature = 0.15f;
    // Warm enough to thaw ice and melt snow (hysteresis above freezing).
    float meltTemperature = 0.5f;
    // Milder threshold at which water freezes if it already touches ice, so a
    // frozen patch creeps outward across a surface.
    float frostSpreadTemperature = 0.30f;
    int maxSnowDepth = 8;
    // Ticks a block of ice must survive in the cold before it compacts to
    // packed ice.
    int packThreshold = 4;
};

// Deterministic per-region climate simulation layered on a BiomeGen. Column
// temperature is the biome sample adjusted for elevation and latitude. During
// snowfall it accumulates SnowLayer depth on exposed surfaces; exposed water
// freezes to Ice when cold; Ice and snow thaw when warm or beside a heat source
// (lava, torches, glowstone). tick scans the region in a fixed order and emits
// the resulting block changes without mutating the world; apply commits them.
class Climate {
public:
    Climate(const BiomeGen& biomes, const ClimateConfig& config = {});

    float temperatureAt(int gx, int gy, int gz) const;
    float humidityAt(int gx, int gz) const;
    bool freezingAt(int gx, int gy, int gz) const;
    bool thawingAt(int gx, int gy, int gz) const;

    std::vector<ClimateChange> tick(World& world, const Vec3i& origin, const Vec3i& dims,
                                  Precipitation precip);

    static void apply(World& world, const std::vector<ClimateChange>& changes);

    int snowDepthAt(const Vec3i& pos) const;
    int iceAgeAt(const Vec3i& pos) const;
    std::size_t trackedSnowColumns() const { return snowDepth.size(); }

    const ClimateConfig& settings() const { return config; }

private:
    bool heatSourceNear(const World& world, const Vec3i& pos) const;
    bool iceNear(const World& world, const Vec3i& pos) const;

    const BiomeGen& biomes;
    ClimateConfig config;
    std::unordered_map<Vec3i, int, Vec3iHash> snowDepth;
    std::unordered_map<Vec3i, int, Vec3iHash> iceAge;
};

} // namespace vf
