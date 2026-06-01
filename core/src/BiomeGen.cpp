#include "vf/BiomeGen.h"

#include <cmath>

namespace vf {

const char* biomeKindName(BiomeKind biome) {
    switch (biome) {
        case BiomeKind::Ocean:     return "Ocean";
        case BiomeKind::Beach:     return "Beach";
        case BiomeKind::Desert:    return "Desert";
        case BiomeKind::Plains:    return "Plains";
        case BiomeKind::Forest:    return "Forest";
        case BiomeKind::Taiga:     return "Taiga";
        case BiomeKind::Tundra:    return "Tundra";
        case BiomeKind::Swamp:     return "Swamp";
        case BiomeKind::Jungle:    return "Jungle";
        case BiomeKind::Savanna:   return "Savanna";
        case BiomeKind::Mountains: return "Mountains";
        case BiomeKind::Count: break;
    }
    return "Unknown";
}

BiomeGen::BiomeGen(const BiomeGenParams& biomeParams)
    : params(biomeParams),
      temperatureNoise(biomeParams.seed ^ 0x9E3779B1u),
      humidityNoise(biomeParams.seed ^ 0x85EBCA6Bu),
      continentalnessNoise(biomeParams.seed ^ 0xC2B2AE35u),
      erosionNoise(biomeParams.seed ^ 0x27D4EB2Fu),
      weirdnessNoise(biomeParams.seed ^ 0x165667B1u) {}

ClimateSample BiomeGen::climateAt(int gx, int gz) const {
    FbmParams fbmParams;
    fbmParams.octaves = 4;
    fbmParams.lacunarity = 2.0f;
    fbmParams.gain = 0.5f;

    float cx = static_cast<float>(gx);
    float cz = static_cast<float>(gz);

    ClimateSample climate;
    climate.temperature = temperatureNoise.fbm2D(cx * params.climateFrequency, cz * params.climateFrequency, fbmParams);
    climate.humidity = humidityNoise.fbm2D(cx * params.climateFrequency, cz * params.climateFrequency, fbmParams);
    climate.continentalness = continentalnessNoise.fbm2D(
        cx * params.climateFrequency * 0.6f, cz * params.climateFrequency * 0.6f, fbmParams);
    climate.erosion = erosionNoise.fbm2D(cx * params.erosionFrequency, cz * params.erosionFrequency, fbmParams);
    climate.weirdness = weirdnessNoise.fbm2D(cx * params.weirdnessFrequency, cz * params.weirdnessFrequency, fbmParams);
    return climate;
}

BiomeKind BiomeGen::biomeFromClimate(const ClimateSample& climate) const {
    if (climate.continentalness < -0.35f) return BiomeKind::Ocean;
    if (climate.continentalness < -0.2f) return BiomeKind::Beach;
    if (climate.erosion > 0.45f) return BiomeKind::Mountains;

    if (climate.temperature < -0.35f) return BiomeKind::Tundra;
    if (climate.temperature < -0.05f) return BiomeKind::Taiga;

    if (climate.temperature > 0.45f) {
        if (climate.humidity < -0.1f) return BiomeKind::Desert;
        if (climate.humidity > 0.35f) return BiomeKind::Jungle;
        return BiomeKind::Savanna;
    }

    if (climate.humidity > 0.4f) return BiomeKind::Swamp;
    if (climate.humidity > 0.0f) return BiomeKind::Forest;
    return BiomeKind::Plains;
}

BiomeKind BiomeGen::biomeAt(int gx, int gz) const {
    return biomeFromClimate(climateAt(gx, gz));
}

bool BiomeGen::isRiver(int gx, int gz) const {
    ClimateSample climate = climateAt(gx, gz);
    if (climate.continentalness < -0.35f) return false;  // no rivers carved into open ocean
    return std::fabs(climate.weirdness) < params.riverWidth;
}

SurfaceLayers BiomeGen::surfaceFor(BiomeKind biome) const {
    switch (biome) {
        case BiomeKind::Ocean:     return {Block::Sand, Block::Sand, 4};
        case BiomeKind::Beach:     return {Block::Sand, Block::Sand, 3};
        case BiomeKind::Desert:    return {Block::Sand, Block::Sandstone, 5};
        case BiomeKind::Plains:    return {Block::Grass, Block::Dirt, 3};
        case BiomeKind::Forest:    return {Block::Grass, Block::Dirt, 3};
        case BiomeKind::Taiga:     return {Block::Grass, Block::Dirt, 3};
        case BiomeKind::Tundra:    return {Block::Snow, Block::Dirt, 2};
        case BiomeKind::Swamp:     return {Block::Grass, Block::Dirt, 4};
        case BiomeKind::Jungle:    return {Block::Grass, Block::Dirt, 3};
        case BiomeKind::Savanna:   return {Block::Grass, Block::Dirt, 2};
        case BiomeKind::Mountains: return {Block::Stone, Block::Stone, 1};
        case BiomeKind::Count: break;
    }
    return {Block::Grass, Block::Dirt, 3};
}

Block BiomeGen::surfaceBlockAt(int gx, int gz, int height) const {
    if (isRiver(gx, gz) && height <= params.seaLevel + 1) return Block::Water;

    BiomeKind biome = biomeAt(gx, gz);
    if (biome == BiomeKind::Mountains && height > params.seaLevel + 46) return Block::Snow;

    return surfaceFor(biome).top;
}

DecoratorDensity BiomeGen::decoratorsFor(BiomeKind biome) const {
    switch (biome) {
        case BiomeKind::Ocean:     return {0.0f, 0.0f, 0.0f};
        case BiomeKind::Beach:     return {0.0f, 0.02f, 0.0f};
        case BiomeKind::Desert:    return {0.0f, 0.0f, 0.05f};
        case BiomeKind::Plains:    return {0.02f, 0.08f, 0.0f};
        case BiomeKind::Forest:    return {0.22f, 0.08f, 0.0f};
        case BiomeKind::Taiga:     return {0.20f, 0.04f, 0.0f};
        case BiomeKind::Tundra:    return {0.03f, 0.01f, 0.0f};
        case BiomeKind::Swamp:     return {0.10f, 0.06f, 0.10f};
        case BiomeKind::Jungle:    return {0.35f, 0.12f, 0.02f};
        case BiomeKind::Savanna:   return {0.05f, 0.05f, 0.0f};
        case BiomeKind::Mountains: return {0.01f, 0.0f, 0.0f};
        case BiomeKind::Count: break;
    }
    return {0.0f, 0.0f, 0.0f};
}

BiomeWeights BiomeGen::biomeBlend(int gx, int gz, int radius) const {
    BiomeWeights weights;
    int total = 0;
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            BiomeKind biome = biomeAt(gx + dx, gz + dz);
            weights.weight[static_cast<std::size_t>(biome)] += 1.0f;
            ++total;
        }
    }
    for (float& w : weights.weight) w /= static_cast<float>(total);
    return weights;
}

} // namespace vf
