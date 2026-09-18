#pragma once

#include <array>
#include <cstdint>

namespace vf {

class PerlinNoise {
public:
    explicit PerlinNoise(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed);

    float noise2D(float x, float y) const;
    float noise3D(float x, float y, float z) const;

    float fbm2D(float x, float y, int octaves, float lacunarity, float gain) const;

private:
    std::array<int, 512> perm;
};

}
