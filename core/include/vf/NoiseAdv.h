#pragma once

#include <array>
#include <cstdint>

namespace vf {

// Distance metric used by worley2D to turn a feature-point offset into a
// scalar cell distance.
enum class DistanceMetric : uint8_t {
    Euclidean = 0,
    Manhattan = 1,
    Chebyshev = 2,
};

// Closest (f1) and second-closest (f2) feature point distances for a worley
// sample, plus the integer cell that produced the closest point.
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

// Seeded advanced noise toolkit: gradient simplex noise (2D/3D), cellular
// (worley) noise with selectable distance metric, ridged multifractal,
// domain-warped fBm and lattice value noise. All outputs are clamped or
// normalised to land in [-1, 1] (worley distances excepted, which are
// unbounded by nature). Distinct symbol names from vf::PerlinNoise in
// Noise.h so both can coexist in the same binary.
class NoiseAdv {
public:
    explicit NoiseAdv(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed);

    float simplex2D(float x, float y) const;
    float simplex3D(float x, float y, float z) const;

    // Interpolated lattice noise (not gradient-based), useful where a
    // blockier, less swirly texture than simplex is wanted.
    float value2D(float x, float y) const;

    WorleyResult worley2D(float x, float y, DistanceMetric metric = DistanceMetric::Euclidean) const;

    // Single-octave ridge transform of simplex2D: 1 - |n|, range [0, 1].
    float ridged2D(float x, float y) const;

    // Ridged multifractal fBm: each octave is weighted by the previous
    // octave's ridge signal, producing sharp connected crests. Range [0, 1].
    float ridgedFbm2D(float x, float y, const FbmParams& params) const;

    // Amplitude-normalised fractal Brownian motion. Range [-1, 1].
    float fbm2D(float x, float y, const FbmParams& params) const;
    float fbm3D(float x, float y, float z, const FbmParams& params) const;

    // Samples fbm2D at (x, y) displaced by two independent simplex fields,
    // producing the swirled, warped look used for organic terrain features.
    float warpedFbm2D(float x, float y, const FbmParams& params, float warpStrength) const;

private:
    float gradDot2D(int hashIndex, float x, float y) const;
    float gradDot3D(int hashIndex, float x, float y, float z) const;
    uint32_t hashCell(int x, int y) const;
    float latticeValue(int x, int y) const;

    std::array<int, 512> perm{};
    uint32_t seedValue = 1337u;
};

} // namespace vf
