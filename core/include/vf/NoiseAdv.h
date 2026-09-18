#pragma once

#include <array>
#include <cstdint>

namespace vf {

enum class DistanceMetric : uint8_t {
    Euclidean = 0,
    Manhattan = 1,
    Chebyshev = 2,
};

struct WorleyResult {
    float f1 = 0.0f;
    float f2 = 0.0f;
    int cellX = 0;
    int cellY = 0;
};

struct FbmParams {
    int octaves = 5;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
};

class NoiseAdv {
public:
    explicit NoiseAdv(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed);

    float simplex2D(float x, float y) const;
    float simplex3D(float x, float y, float z) const;

    float value2D(float x, float y) const;

    WorleyResult worley2D(float x, float y, DistanceMetric metric = DistanceMetric::Euclidean) const;

    float ridged2D(float x, float y) const;

    float ridgedFbm2D(float x, float y, const FbmParams& params) const;

    float fbm2D(float x, float y, const FbmParams& params) const;
    float fbm3D(float x, float y, float z, const FbmParams& params) const;

    float warpedFbm2D(float x, float y, const FbmParams& params, float warpStrength) const;

private:
    float gradDot2D(int hashIndex, float x, float y) const;
    float gradDot3D(int hashIndex, float x, float y, float z) const;
    uint32_t hashCell(int x, int y) const;
    float latticeValue(int x, int y) const;

    std::array<int, 512> perm{};
    uint32_t seedValue = 1337u;
};

}
