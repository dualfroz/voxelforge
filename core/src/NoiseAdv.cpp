#include "vf/NoiseAdv.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vf {

namespace {

constexpr float simplexF2 = 0.3660254037844386f;
constexpr float simplexG2 = 0.21132486540518713f;
constexpr float simplexF3 = 1.0f / 3.0f;
constexpr float simplexG3 = 1.0f / 6.0f;

constexpr float gradients2D[8][2] = {
    {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
    {0.7071068f, 0.7071068f}, {-0.7071068f, 0.7071068f},
    {0.7071068f, -0.7071068f}, {-0.7071068f, -0.7071068f},
};

constexpr float gradients3D[12][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
    {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
};

float quinticFade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float clampUnit(float v) {
    return std::max(-1.0f, std::min(1.0f, v));
}

}

void NoiseAdv::reseed(uint32_t seed) {
    seedValue = seed;

    std::array<int, 256> base{};
    for (int i = 0; i < 256; ++i) base[i] = i;

    uint32_t state = seed ^ 0x9E3779B9u;
    if (state == 0) state = 0x1u;
    auto nextRandom = [&state]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };

    for (int i = 255; i > 0; --i) {
        int j = static_cast<int>(nextRandom() % static_cast<uint32_t>(i + 1));
        std::swap(base[i], base[j]);
    }

    for (int i = 0; i < 512; ++i) perm[static_cast<std::size_t>(i)] = base[static_cast<std::size_t>(i & 255)];
}

float NoiseAdv::gradDot2D(int hashIndex, float x, float y) const {
    const float* g = gradients2D[hashIndex & 7];
    return g[0] * x + g[1] * y;
}

float NoiseAdv::gradDot3D(int hashIndex, float x, float y, float z) const {
    const float* g = gradients3D[hashIndex % 12];
    return g[0] * x + g[1] * y + g[2] * z;
}

uint32_t NoiseAdv::hashCell(int x, int y) const {
    uint32_t h = seedValue;
    h ^= static_cast<uint32_t>(x) * 0x27D4EB2Fu;
    h ^= static_cast<uint32_t>(y) * 0x165667B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

float NoiseAdv::latticeValue(int x, int y) const {
    uint32_t h = hashCell(x, y);
    return (static_cast<float>(h & 0xFFFFFFu) / 16777215.0f) * 2.0f - 1.0f;
}

float NoiseAdv::simplex2D(float x, float y) const {
    float s = (x + y) * simplexF2;
    int i = static_cast<int>(std::floor(x + s));
    int j = static_cast<int>(std::floor(y + s));

    float t = static_cast<float>(i + j) * simplexG2;
    float x0Origin = static_cast<float>(i) - t;
    float y0Origin = static_cast<float>(j) - t;
    float x0 = x - x0Origin;
    float y0 = y - y0Origin;

    int i1 = 0, j1 = 0;
    if (x0 > y0) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }

    float x1 = x0 - static_cast<float>(i1) + simplexG2;
    float y1 = y0 - static_cast<float>(j1) + simplexG2;
    float x2 = x0 - 1.0f + 2.0f * simplexG2;
    float y2 = y0 - 1.0f + 2.0f * simplexG2;

    int ii = i & 255;
    int jj = j & 255;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0.0f) {
        int hash0 = perm[static_cast<std::size_t>(ii + perm[static_cast<std::size_t>(jj)])];
        t0 *= t0;
        n0 = t0 * t0 * gradDot2D(hash0, x0, y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0.0f) {
        int hash1 = perm[static_cast<std::size_t>(ii + i1 + perm[static_cast<std::size_t>(jj + j1)])];
        t1 *= t1;
        n1 = t1 * t1 * gradDot2D(hash1, x1, y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0.0f) {
        int hash2 = perm[static_cast<std::size_t>(ii + 1 + perm[static_cast<std::size_t>(jj + 1)])];
        t2 *= t2;
        n2 = t2 * t2 * gradDot2D(hash2, x2, y2);
    }

    return clampUnit(70.0f * (n0 + n1 + n2));
}

float NoiseAdv::simplex3D(float x, float y, float z) const {
    float s = (x + y + z) * simplexF3;
    int i = static_cast<int>(std::floor(x + s));
    int j = static_cast<int>(std::floor(y + s));
    int k = static_cast<int>(std::floor(z + s));

    float t = static_cast<float>(i + j + k) * simplexG3;
    float x0 = x - (static_cast<float>(i) - t);
    float y0 = y - (static_cast<float>(j) - t);
    float z0 = z - (static_cast<float>(k) - t);

    int i1, j1, k1;
    int i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
        else               { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
    } else {
        if (y0 < z0)      { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
        else if (x0 < z0) { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
        else              { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
    }

    float x1 = x0 - static_cast<float>(i1) + simplexG3;
    float y1 = y0 - static_cast<float>(j1) + simplexG3;
    float z1 = z0 - static_cast<float>(k1) + simplexG3;
    float x2 = x0 - static_cast<float>(i2) + 2.0f * simplexG3;
    float y2 = y0 - static_cast<float>(j2) + 2.0f * simplexG3;
    float z2 = z0 - static_cast<float>(k2) + 2.0f * simplexG3;
    float x3 = x0 - 1.0f + 3.0f * simplexG3;
    float y3 = y0 - 1.0f + 3.0f * simplexG3;
    float z3 = z0 - 1.0f + 3.0f * simplexG3;

    int ii = i & 255;
    int jj = j & 255;
    int kk = k & 255;

    float n0 = 0.0f, n1 = 0.0f, n2 = 0.0f, n3 = 0.0f;

    float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
    if (t0 > 0.0f) {
        int hash0 = perm[static_cast<std::size_t>(ii + perm[static_cast<std::size_t>(jj + perm[static_cast<std::size_t>(kk)])])];
        t0 *= t0;
        n0 = t0 * t0 * gradDot3D(hash0, x0, y0, z0);
    }
    float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
    if (t1 > 0.0f) {
        int hash1 = perm[static_cast<std::size_t>(ii + i1 + perm[static_cast<std::size_t>(jj + j1 + perm[static_cast<std::size_t>(kk + k1)])])];
        t1 *= t1;
        n1 = t1 * t1 * gradDot3D(hash1, x1, y1, z1);
    }
    float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
    if (t2 > 0.0f) {
        int hash2 = perm[static_cast<std::size_t>(ii + i2 + perm[static_cast<std::size_t>(jj + j2 + perm[static_cast<std::size_t>(kk + k2)])])];
        t2 *= t2;
        n2 = t2 * t2 * gradDot3D(hash2, x2, y2, z2);
    }
    float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
    if (t3 > 0.0f) {
        int hash3 = perm[static_cast<std::size_t>(ii + 1 + perm[static_cast<std::size_t>(jj + 1 + perm[static_cast<std::size_t>(kk + 1)])])];
        t3 *= t3;
        n3 = t3 * t3 * gradDot3D(hash3, x3, y3, z3);
    }

    return clampUnit(32.0f * (n0 + n1 + n2 + n3));
}

float NoiseAdv::value2D(float x, float y) const {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - static_cast<float>(xi);
    float yf = y - static_cast<float>(yi);

    float v00 = latticeValue(xi, yi);
    float v10 = latticeValue(xi + 1, yi);
    float v01 = latticeValue(xi, yi + 1);
    float v11 = latticeValue(xi + 1, yi + 1);

    float u = quinticFade(xf);
    float v = quinticFade(yf);

    float top = v00 + (v10 - v00) * u;
    float bottom = v01 + (v11 - v01) * u;
    return clampUnit(top + (bottom - top) * v);
}

WorleyResult NoiseAdv::worley2D(float x, float y, DistanceMetric metric) const {
    int cellX = static_cast<int>(std::floor(x));
    int cellY = static_cast<int>(std::floor(y));

    float best1 = std::numeric_limits<float>::max();
    float best2 = std::numeric_limits<float>::max();
    int bestCellX = cellX;
    int bestCellY = cellY;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int neighborX = cellX + dx;
            int neighborY = cellY + dy;
            uint32_t h = hashCell(neighborX, neighborY);
            float jitterX = static_cast<float>(h & 0xFFFFu) / 65535.0f;
            float jitterY = static_cast<float>((h >> 16) & 0xFFFFu) / 65535.0f;
            float pointX = static_cast<float>(neighborX) + jitterX;
            float pointY = static_cast<float>(neighborY) + jitterY;

            float distX = x - pointX;
            float distY = y - pointY;
            float distance;
            switch (metric) {
                case DistanceMetric::Manhattan:
                    distance = std::fabs(distX) + std::fabs(distY);
                    break;
                case DistanceMetric::Chebyshev:
                    distance = std::max(std::fabs(distX), std::fabs(distY));
                    break;
                default:
                    distance = std::sqrt(distX * distX + distY * distY);
                    break;
            }

            if (distance < best1) {
                best2 = best1;
                best1 = distance;
                bestCellX = neighborX;
                bestCellY = neighborY;
            } else if (distance < best2) {
                best2 = distance;
            }
        }
    }

    WorleyResult result;
    result.f1 = best1;
    result.f2 = best2;
    result.cellX = bestCellX;
    result.cellY = bestCellY;
    return result;
}

float NoiseAdv::ridged2D(float x, float y) const {
    return 1.0f - std::fabs(simplex2D(x, y));
}

float NoiseAdv::ridgedFbm2D(float x, float y, const FbmParams& fbmParams) const {
    float sum = 0.0f;
    float freq = fbmParams.frequency;
    float amp = fbmParams.amplitude;
    float weight = 1.0f;
    float norm = 0.0f;

    for (int octave = 0; octave < fbmParams.octaves; ++octave) {
        float signal = 1.0f - std::fabs(simplex2D(x * freq, y * freq));
        signal *= signal;
        signal *= weight;
        weight = std::max(0.0f, std::min(1.0f, signal));

        sum += signal * amp;
        norm += amp;
        freq *= fbmParams.lacunarity;
        amp *= fbmParams.gain;
    }

    return norm > 0.0f ? sum / norm : 0.0f;
}

float NoiseAdv::fbm2D(float x, float y, const FbmParams& fbmParams) const {
    float sum = 0.0f;
    float freq = fbmParams.frequency;
    float amp = fbmParams.amplitude;
    float norm = 0.0f;

    for (int octave = 0; octave < fbmParams.octaves; ++octave) {
        sum += simplex2D(x * freq, y * freq) * amp;
        norm += amp;
        freq *= fbmParams.lacunarity;
        amp *= fbmParams.gain;
    }

    return norm > 0.0f ? clampUnit(sum / norm) : 0.0f;
}

float NoiseAdv::fbm3D(float x, float y, float z, const FbmParams& fbmParams) const {
    float sum = 0.0f;
    float freq = fbmParams.frequency;
    float amp = fbmParams.amplitude;
    float norm = 0.0f;

    for (int octave = 0; octave < fbmParams.octaves; ++octave) {
        sum += simplex3D(x * freq, y * freq, z * freq) * amp;
        norm += amp;
        freq *= fbmParams.lacunarity;
        amp *= fbmParams.gain;
    }

    return norm > 0.0f ? clampUnit(sum / norm) : 0.0f;
}

float NoiseAdv::warpedFbm2D(float x, float y, const FbmParams& fbmParams, float warpStrength) const {
    float warpX = simplex2D(x * 0.5f + 17.0f, y * 0.5f - 43.0f);
    float warpY = simplex2D(x * 0.5f - 89.0f, y * 0.5f + 5.0f);
    return fbm2D(x + warpX * warpStrength, y + warpY * warpStrength, fbmParams);
}

}
