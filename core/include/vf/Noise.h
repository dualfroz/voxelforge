#pragma once

#include <array>
#include <cstdint>

namespace vf {

// Improved Perlin noise (Ken Perlin, 2002) over a seeded permutation table.
// Output of noise2D/noise3D is in the range [-1, 1].
class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed);

    float noise2D(float x, float y) const;
    float noise3D(float x, float y, float z) const;

    // Fractal Brownian motion: sum of `octaves` scaled noise layers.
    // Result normalised to roughly [-1, 1].
    float fbm2D(float x, float y, int octaves, float lacunarity, float gain) const;

private:
    std::array<int, 512> perm;
};

} // namespace vf
