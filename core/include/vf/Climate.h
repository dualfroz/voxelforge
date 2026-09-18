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
    float heightLapse = 0.006f;
    float latitudeLapse = 0.0009f;
    float freezeTemperature = 0.15f;
    float meltTemperature = 0.5f;
    float frostSpreadTemperature = 0.30f;
    int maxSnowDepth = 8;
    int packThreshold = 4;
};

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

}
