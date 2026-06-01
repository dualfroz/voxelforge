#pragma once

#include <array>
#include <cstdint>
#include "Block.h"
#include "NoiseAdv.h"

namespace vf {

// Standalone climate-driven biome catalogue. Deliberately named BiomeKind,
// distinct from vf::Biome in WorldGen.h, so this header can be included
// alongside WorldGen.h without redefinition.
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

// Five-axis climate sample at a world column, each roughly in [-1, 1].
struct ClimateSample {
    float temperature = 0.0f;
    float humidity = 0.0f;
    float continentalness = 0.0f;  // low = ocean, high = deep inland
    float erosion = 0.0f;          // low = jagged/mountainous, high = flat
    float weirdness = 0.0f;        // near 0 carves a river valley
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
    float caneDensity = 0.0f;  // cactus in desert, reeds in swamp/jungle
};

// Vote-blended biome weights over a neighbourhood, used to smooth borders
// (surface colour, height blending) instead of a hard biome edge.
struct BiomeWeights {
    std::array<float, static_cast<std::size_t>(BiomeKind::Count)> weight{};

    float weightOf(BiomeKind biome) const { return weight[static_cast<std::size_t>(biome)]; }
};

// Climate-parameter biome classifier with river carving, surface builders
// and decorator density tables. Independent of vf::WorldGen: samples are
// pure functions of world column coordinates, so identical seed and column
// always yield an identical result.
class BiomeGen {
public:
    explicit BiomeGen(const BiomeGenParams& params = {});

    ClimateSample climateAt(int gx, int gz) const;
    BiomeKind biomeFromClimate(const ClimateSample& climate) const;
    BiomeKind biomeAt(int gx, int gz) const;

    bool isRiver(int gx, int gz) const;

    SurfaceLayers surfaceFor(BiomeKind biome) const;

    // Surface block at height `height` for the column at (gx, gz): river
    // water/sand overrides the biome's ordinary top block.
    Block surfaceBlockAt(int gx, int gz, int height) const;

    DecoratorDensity decoratorsFor(BiomeKind biome) const;

    // Box-blurred biome vote over a (2*radius+1)^2 neighbourhood, weights
    // sum to 1.
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

} // namespace vf
