#include "testHarness.h"
#include "vf/NoiseAdv.h"

using namespace vf;

VF_TEST(noiseAdvSimplex2DStaysInRangeAndIsDeterministic) {
    NoiseAdv noise(42u);
    for (int ix = -50; ix <= 50; ++ix) {
        for (int iz = -50; iz <= 50; iz += 5) {
            float x = static_cast<float>(ix) * 0.13f;
            float z = static_cast<float>(iz) * 0.13f;
            float first = noise.simplex2D(x, z);
            float second = noise.simplex2D(x, z);
            CHECK(first >= -1.0f && first <= 1.0f);
            CHECK_EQ(first, second);
        }
    }
}

VF_TEST(noiseAdvSimplex3DStaysInRangeAndIsDeterministic) {
    NoiseAdv noise(7u);
    for (int ix = -20; ix <= 20; ix += 2) {
        for (int iy = -20; iy <= 20; iy += 4) {
            for (int iz = -20; iz <= 20; iz += 4) {
                float x = static_cast<float>(ix) * 0.2f;
                float y = static_cast<float>(iy) * 0.2f;
                float z = static_cast<float>(iz) * 0.2f;
                float first = noise.simplex3D(x, y, z);
                float second = noise.simplex3D(x, y, z);
                CHECK(first >= -1.0f && first <= 1.0f);
                CHECK_EQ(first, second);
            }
        }
    }
}

VF_TEST(noiseAdvSimplexVariesAcrossSpace) {
    NoiseAdv noise(99u);
    float lo = 2.0f, hi = -2.0f;
    for (int ix = 0; ix < 200; ++ix) {
        float value = noise.simplex2D(static_cast<float>(ix) * 0.07f, 3.5f);
        lo = value < lo ? value : lo;
        hi = value > hi ? value : hi;
    }
    CHECK(hi - lo > 0.5f);
}

VF_TEST(noiseAdvDifferentSeedsDiverge) {
    NoiseAdv a(1u);
    NoiseAdv b(2u);
    int differing = 0;
    for (int ix = 0; ix < 60; ++ix) {
        float x = static_cast<float>(ix) * 0.11f;
        if (a.simplex2D(x, 1.0f) != b.simplex2D(x, 1.0f)) ++differing;
    }
    CHECK(differing > 50);
}

VF_TEST(noiseAdvValue2DStaysInRangeAndIsDeterministic) {
    NoiseAdv noise(5u);
    for (int ix = -30; ix <= 30; ++ix) {
        for (int iz = -30; iz <= 30; iz += 3) {
            float x = static_cast<float>(ix) * 0.31f;
            float z = static_cast<float>(iz) * 0.31f;
            float first = noise.value2D(x, z);
            float second = noise.value2D(x, z);
            CHECK(first >= -1.0f && first <= 1.0f);
            CHECK_EQ(first, second);
        }
    }
}

VF_TEST(noiseAdvValue2DIsSmoothAtLatticePoints) {
    NoiseAdv noise(5u);
    float atLattice = noise.value2D(4.0f, -7.0f);
    float nearby = noise.value2D(4.001f, -7.001f);
    CHECK_NEAR(atLattice, nearby, 0.01f);
}

VF_TEST(noiseAdvFbm2DIsNormalisedAndDeterministic) {
    NoiseAdv noise(123u);
    FbmParams params;
    params.octaves = 6;
    params.lacunarity = 2.1f;
    params.gain = 0.5f;
    params.frequency = 0.05f;

    for (int ix = -80; ix <= 80; ix += 4) {
        for (int iz = -80; iz <= 80; iz += 8) {
            float x = static_cast<float>(ix);
            float z = static_cast<float>(iz);
            float first = noise.fbm2D(x, z, params);
            float second = noise.fbm2D(x, z, params);
            CHECK(first >= -1.0f && first <= 1.0f);
            CHECK_EQ(first, second);
        }
    }
}

VF_TEST(noiseAdvFbm2DOctaveCountAddsDetail) {
    NoiseAdv single(55u);
    NoiseAdv multi(55u);
    FbmParams oneOctave;
    oneOctave.octaves = 1;
    oneOctave.frequency = 0.08f;
    FbmParams sixOctaves;
    sixOctaves.octaves = 6;
    sixOctaves.frequency = 0.08f;
    sixOctaves.lacunarity = 2.0f;
    sixOctaves.gain = 0.5f;

    int differing = 0;
    for (int ix = 0; ix < 40; ++ix) {
        float x = static_cast<float>(ix) * 1.7f;
        float a = single.fbm2D(x, 2.0f, oneOctave);
        float b = multi.fbm2D(x, 2.0f, sixOctaves);
        if (a != b) ++differing;
    }
    CHECK(differing > 30);
}

VF_TEST(noiseAdvFbm3DIsNormalisedAndDeterministic) {
    NoiseAdv noise(321u);
    FbmParams params;
    params.octaves = 4;
    params.frequency = 0.06f;

    for (int ix = -30; ix <= 30; ix += 5) {
        for (int iy = -10; iy <= 10; iy += 5) {
            for (int iz = -30; iz <= 30; iz += 10) {
                float x = static_cast<float>(ix);
                float y = static_cast<float>(iy);
                float z = static_cast<float>(iz);
                float first = noise.fbm3D(x, y, z, params);
                float second = noise.fbm3D(x, y, z, params);
                CHECK(first >= -1.0f && first <= 1.0f);
                CHECK_EQ(first, second);
            }
        }
    }
}

VF_TEST(noiseAdvRidged2DStaysInZeroOneRange) {
    NoiseAdv noise(8u);
    for (int ix = -40; ix <= 40; ++ix) {
        float value = noise.ridged2D(static_cast<float>(ix) * 0.09f, 1.5f);
        CHECK(value >= 0.0f && value <= 1.0f);
    }
}

VF_TEST(noiseAdvRidgedFbmStaysInZeroOneRangeAndIsDeterministic) {
    NoiseAdv noise(17u);
    FbmParams params;
    params.octaves = 5;
    params.frequency = 0.04f;
    params.lacunarity = 2.0f;
    params.gain = 0.5f;

    for (int ix = -60; ix <= 60; ix += 3) {
        for (int iz = -60; iz <= 60; iz += 6) {
            float x = static_cast<float>(ix);
            float z = static_cast<float>(iz);
            float first = noise.ridgedFbm2D(x, z, params);
            float second = noise.ridgedFbm2D(x, z, params);
            CHECK(first >= 0.0f && first <= 1.0f);
            CHECK_EQ(first, second);
        }
    }
}

VF_TEST(noiseAdvRidgedFbmProducesConnectedCrests) {
    NoiseAdv noise(17u);
    FbmParams params;
    params.octaves = 5;
    params.frequency = 0.04f;
    params.lacunarity = 2.0f;
    params.gain = 0.5f;

    int highValues = 0;
    for (int ix = -80; ix <= 80; ix += 2) {
        float value = noise.ridgedFbm2D(static_cast<float>(ix), 4.0f, params);
        if (value > 0.6f) ++highValues;
    }
    CHECK(highValues > 0);
}

VF_TEST(noiseAdvWorleyF2IsNeverBelowF1) {
    NoiseAdv noise(64u);
    for (int ix = -40; ix <= 40; ++ix) {
        for (int iz = -40; iz <= 40; iz += 2) {
            float x = static_cast<float>(ix) * 0.37f;
            float z = static_cast<float>(iz) * 0.37f;
            WorleyResult euclidean = noise.worley2D(x, z, DistanceMetric::Euclidean);
            WorleyResult manhattan = noise.worley2D(x, z, DistanceMetric::Manhattan);
            WorleyResult chebyshev = noise.worley2D(x, z, DistanceMetric::Chebyshev);
            CHECK(euclidean.f2 >= euclidean.f1);
            CHECK(manhattan.f2 >= manhattan.f1);
            CHECK(chebyshev.f2 >= chebyshev.f1);
            CHECK(euclidean.f1 >= 0.0f);
        }
    }
}

VF_TEST(noiseAdvWorleyIsDeterministicAndVaries) {
    NoiseAdv noise(64u);
    WorleyResult first = noise.worley2D(11.3f, -4.7f, DistanceMetric::Euclidean);
    WorleyResult second = noise.worley2D(11.3f, -4.7f, DistanceMetric::Euclidean);
    CHECK_NEAR(first.f1, second.f1, 1e-6f);
    CHECK_EQ(first.cellX, second.cellX);
    CHECK_EQ(first.cellY, second.cellY);

    float lo = 1000.0f, hi = -1000.0f;
    for (int ix = 0; ix < 100; ++ix) {
        WorleyResult sample = noise.worley2D(static_cast<float>(ix) * 0.3f, 0.0f);
        lo = sample.f1 < lo ? sample.f1 : lo;
        hi = sample.f1 > hi ? sample.f1 : hi;
    }
    CHECK(hi > lo);
}

VF_TEST(noiseAdvWorleyMetricsDisagreeOnSomeSamples) {
    NoiseAdv noise(64u);
    int disagreements = 0;
    for (int ix = -30; ix <= 30; ++ix) {
        float x = static_cast<float>(ix) * 0.41f;
        WorleyResult euclidean = noise.worley2D(x, 2.3f, DistanceMetric::Euclidean);
        WorleyResult chebyshev = noise.worley2D(x, 2.3f, DistanceMetric::Chebyshev);
        if (euclidean.f1 != chebyshev.f1) ++disagreements;
    }
    CHECK(disagreements > 10);
}

VF_TEST(noiseAdvWarpedFbmStaysInRangeAndIsDeterministic) {
    NoiseAdv noise(256u);
    FbmParams params;
    params.octaves = 4;
    params.frequency = 0.05f;

    for (int ix = -50; ix <= 50; ix += 5) {
        for (int iz = -50; iz <= 50; iz += 5) {
            float x = static_cast<float>(ix);
            float z = static_cast<float>(iz);
            float first = noise.warpedFbm2D(x, z, params, 8.0f);
            float second = noise.warpedFbm2D(x, z, params, 8.0f);
            CHECK(first >= -1.0f && first <= 1.0f);
            CHECK_EQ(first, second);
        }
    }
}

VF_TEST(noiseAdvWarpDiffersFromUnwarpedFbm) {
    NoiseAdv noise(256u);
    FbmParams params;
    params.octaves = 4;
    params.frequency = 0.05f;

    int differing = 0;
    for (int ix = -40; ix <= 40; ix += 2) {
        float x = static_cast<float>(ix);
        float plain = noise.fbm2D(x, 3.0f, params);
        float warped = noise.warpedFbm2D(x, 3.0f, params, 6.0f);
        if (plain != warped) ++differing;
    }
    CHECK(differing > 30);
}

VF_TEST(noiseAdvReseedChangesOutputDeterministically) {
    NoiseAdv noise(1u);
    float before = noise.simplex2D(5.5f, -2.2f);
    noise.reseed(2u);
    float afterFirst = noise.simplex2D(5.5f, -2.2f);
    noise.reseed(2u);
    float afterSecond = noise.simplex2D(5.5f, -2.2f);
    CHECK(before != afterFirst);
    CHECK_EQ(afterFirst, afterSecond);
}
