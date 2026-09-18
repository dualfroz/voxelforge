#include "vf/Noise.h"

#include <cmath>
#include <numeric>

namespace vf {

namespace {

inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

inline float lerp(float a, float b, float t) { return a + t * (b - a); }

inline float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

}

void PerlinNoise::reseed(uint32_t seed) {
    std::array<int, 256> base;
    std::iota(base.begin(), base.end(), 0);

    uint32_t state = seed ? seed : 0x9e3779b9u;
    auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };

    for (int i = 255; i > 0; --i) {
        int j = static_cast<int>(next() % static_cast<uint32_t>(i + 1));
        std::swap(base[i], base[j]);
    }
    for (int i = 0; i < 512; ++i) perm[i] = base[i & 255];
}

float PerlinNoise::noise3D(float x, float y, float z) const {
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    int zi = static_cast<int>(std::floor(z)) & 255;

    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float zf = z - std::floor(z);

    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);

    int a = perm[xi] + yi;
    int aa = perm[a] + zi;
    int ab = perm[a + 1] + zi;
    int b = perm[xi + 1] + yi;
    int ba = perm[b] + zi;
    int bb = perm[b + 1] + zi;

    float x1 = lerp(grad(perm[aa], xf, yf, zf), grad(perm[ba], xf - 1, yf, zf), u);
    float x2 = lerp(grad(perm[ab], xf, yf - 1, zf), grad(perm[bb], xf - 1, yf - 1, zf), u);
    float y1 = lerp(x1, x2, v);

    float x3 = lerp(grad(perm[aa + 1], xf, yf, zf - 1), grad(perm[ba + 1], xf - 1, yf, zf - 1), u);
    float x4 = lerp(grad(perm[ab + 1], xf, yf - 1, zf - 1), grad(perm[bb + 1], xf - 1, yf - 1, zf - 1), u);
    float y2 = lerp(x3, x4, v);

    return lerp(y1, y2, w);
}

float PerlinNoise::noise2D(float x, float y) const { return noise3D(x, y, 0.0f); }

float PerlinNoise::fbm2D(float x, float y, int octaves, float lacunarity, float gain) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float totalAmplitude = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += amplitude * noise2D(x * frequency, y * frequency);
        totalAmplitude += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return totalAmplitude > 0.0f ? sum / totalAmplitude : 0.0f;
}

}
