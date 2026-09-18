#pragma once

#include <array>
#include <cstdint>
#include "Block.h"
#include "NoiseAdv.h"

namespace vf {

enum class BiomeKind : uint8_t {
    Ocean = 0,
    Beach = 1,
    Desert = 2,
    Plains = 3,
    Forest = 4,
    Taiga = 5,
    Tundra = 6,
    Swamp = 7,
    Jungle = 8,
    Savanna = 9,
    Mountains = 10,
    Count,
};

const char* biomeKindName(BiomeKind biome);

struct ClimateSample {
    float temperature = 0.0f;
    float humidity = 0.0f;
    float continentalness = 0.0f;
    float erosion = 0.0f;
    float weirdness = 0.0f;
};

struct BiomeGenParams {
    uint32_t seed = 1337u;
    float climateFrequency = 0.003f;
    float erosionFrequency = 0.006f;
    float weirdnessFrequency = 0.01f;
    float riverWidth = 0.045f;
    int seaLevel = 26;
};

struct SurfaceLayers {
    Block top = Block::Grass;
    Block filler = Block::Dirt;
    int fillerDepth = 3;
};

struct DecoratorDensity {
    float treeDensity = 0.0f;
    float bushDensity = 0.0f;
    float caneDensity = 0.0f;
};

struct BiomeWeights {
    std::array<float, static_cast<std::size_t>(BiomeKind::Count)> weight{};

    float weightOf(BiomeKind biome) const { return weight[static_cast<std::size_t>(biome)]; }
};

class BiomeGen {
public:
    explicit BiomeGen(const BiomeGenParams& params = {});

    ClimateSample climateAt(int gx, int gz) const;
    BiomeKind biomeFromClimate(const ClimateSample& climate) const;
    BiomeKind biomeAt(int gx, int gz) const;

    bool isRiver(int gx, int gz) const;

    SurfaceLayers surfaceFor(BiomeKind biome) const;

    Block surfaceBlockAt(int gx, int gz, int height) const;

    DecoratorDensity decoratorsFor(BiomeKind biome) const;

    BiomeWeights biomeBlend(int gx, int gz, int radius) const;

    const BiomeGenParams& settings() const { return params; }

private:
    BiomeGenParams params;
    NoiseAdv temperatureNoise;
    NoiseAdv humidityNoise;
    NoiseAdv continentalnessNoise;
    NoiseAdv erosionNoise;
    NoiseAdv weirdnessNoise;
};

}
