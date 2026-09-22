#include "testHarness.h"
#include "vf/BiomeGen.h"

#include <array>
#include <cmath>
#include <set>
#include <string>

using namespace vf;

namespace {

ClimateSample makeClimate(float temperature, float humidity, float continentalness, float erosion, float weirdness) {
    ClimateSample climate;
    climate.temperature = temperature;
    climate.humidity = humidity;
    climate.continentalness = continentalness;
    climate.erosion = erosion;
    climate.weirdness = weirdness;
    return climate;
}

}

VF_TEST(biomeGenClimateMapsColdDryToTundra) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(-0.6f, -0.2f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Tundra);
}

VF_TEST(biomeGenClimateMapsHotDryToDesert) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.7f, -0.4f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Desert);
}

VF_TEST(biomeGenClimateMapsHotWetToJungle) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.6f, 0.6f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Jungle);
}

VF_TEST(biomeGenClimateMapsHotMidHumidityToSavanna) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.6f, 0.1f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Savanna);
}

VF_TEST(biomeGenClimateMapsMildWetToSwamp) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.1f, 0.6f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Swamp);
}

VF_TEST(biomeGenClimateMapsMildDampToForest) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.1f, 0.2f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Forest);
}

VF_TEST(biomeGenClimateMapsMildDryToPlains) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.1f, -0.2f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Plains);
}

VF_TEST(biomeGenClimateMapsCoolMildToTaiga) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(-0.2f, 0.0f, 0.3f, 0.0f, 0.5f)) == BiomeKind::Taiga);
}

VF_TEST(biomeGenClimateMapsLowContinentalnessToOceanRegardlessOfOtherAxes) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.8f, 0.8f, -0.6f, 0.9f, 0.5f)) == BiomeKind::Ocean);
    CHECK(gen.biomeFromClimate(makeClimate(-0.8f, -0.8f, -0.6f, -0.9f, 0.5f)) == BiomeKind::Ocean);
}

VF_TEST(biomeGenClimateMapsShallowContinentalnessToBeach) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.2f, 0.2f, -0.25f, 0.0f, 0.5f)) == BiomeKind::Beach);
}

VF_TEST(biomeGenClimateMapsHighErosionToMountains) {
    BiomeGen gen;
    CHECK(gen.biomeFromClimate(makeClimate(0.2f, 0.2f, 0.4f, 0.6f, 0.5f)) == BiomeKind::Mountains);
}

VF_TEST(biomeGenProducesEveryCatalogueBiomeOverAWideScan) {
    BiomeGen gen;
    std::set<int> seen;
    for (int gx = -800; gx <= 800; gx += 11)
        for (int gz = -800; gz <= 800; gz += 11)
            seen.insert(static_cast<int>(gen.biomeAt(gx, gz)));
    CHECK(seen.size() >= 8);
}

VF_TEST(biomeGenIsDeterministicForSameSeedAndColumn) {
    BiomeGen first(BiomeGenParams{4242u, 0.003f, 0.006f, 0.01f, 0.045f, 26});
    BiomeGen second(BiomeGenParams{4242u, 0.003f, 0.006f, 0.01f, 0.045f, 26});
    for (int gx = -100; gx <= 100; gx += 7) {
        for (int gz = -100; gz <= 100; gz += 7) {
            ClimateSample a = first.climateAt(gx, gz);
            ClimateSample b = second.climateAt(gx, gz);
            CHECK_NEAR(a.temperature, b.temperature, 1e-6f);
            CHECK_NEAR(a.humidity, b.humidity, 1e-6f);
            CHECK(first.biomeAt(gx, gz) == second.biomeAt(gx, gz));
        }
    }
}

VF_TEST(biomeGenDifferentSeedsDivergeSomewhere) {
    BiomeGen a(BiomeGenParams{1u, 0.003f, 0.006f, 0.01f, 0.045f, 26});
    BiomeGen b(BiomeGenParams{2u, 0.003f, 0.006f, 0.01f, 0.045f, 26});
    int differing = 0;
    for (int gx = -400; gx <= 400; gx += 13)
        for (int gz = -400; gz <= 400; gz += 13)
            if (a.biomeAt(gx, gz) != b.biomeAt(gx, gz)) ++differing;
    CHECK(differing > 0);
}

VF_TEST(biomeGenRiverCorrelatesWithWeirdnessNearZero) {
    BiomeGen gen;
    int riverButFarFromZero = 0;
    int nonRiverButNearZero = 0;
    int riverSamples = 0;
    for (int gx = -600; gx <= 600; gx += 5) {
        for (int gz = -600; gz <= 600; gz += 5) {
            ClimateSample climate = gen.climateAt(gx, gz);
            bool river = gen.isRiver(gx, gz);
            if (river) {
                ++riverSamples;
                if (std::fabs(climate.weirdness) >= gen.settings().riverWidth) ++riverButFarFromZero;
            } else if (climate.continentalness >= -0.35f && std::fabs(climate.weirdness) < gen.settings().riverWidth) {
                ++nonRiverButNearZero;
            }
        }
    }
    CHECK(riverSamples > 0);
    CHECK_EQ(riverButFarFromZero, 0);
    CHECK_EQ(nonRiverButNearZero, 0);
}

VF_TEST(biomeGenSurfaceLayersMatchBiomeExpectation) {
    BiomeGen gen;
    CHECK(gen.surfaceFor(BiomeKind::Tundra).top == Block::Snow);
    CHECK(gen.surfaceFor(BiomeKind::Desert).top == Block::Sand);
    CHECK(gen.surfaceFor(BiomeKind::Ocean).top == Block::Sand);
    CHECK(gen.surfaceFor(BiomeKind::Beach).top == Block::Sand);
    CHECK(gen.surfaceFor(BiomeKind::Plains).top == Block::Grass);
    CHECK(gen.surfaceFor(BiomeKind::Forest).top == Block::Grass);
    CHECK(gen.surfaceFor(BiomeKind::Jungle).top == Block::Grass);
    CHECK(gen.surfaceFor(BiomeKind::Mountains).top == Block::Stone);
    CHECK(gen.surfaceFor(BiomeKind::Plains).filler == Block::Dirt);
    CHECK(gen.surfaceFor(BiomeKind::Desert).filler == Block::Sandstone);
}

VF_TEST(biomeGenSurfaceBlockAtPrefersRiverWaterOverLandTop) {
    BiomeGen gen;
    bool foundRiverWater = false;
    for (int gx = -600; gx <= 600 && !foundRiverWater; gx += 3) {
        for (int gz = -600; gz <= 600 && !foundRiverWater; gz += 3) {
            if (gen.isRiver(gx, gz)) {
                Block surface = gen.surfaceBlockAt(gx, gz, gen.settings().seaLevel);
                CHECK(surface == Block::Water);
                foundRiverWater = true;
            }
        }
    }
    CHECK(foundRiverWater);
}

VF_TEST(biomeGenDecoratorDensityRanksJungleAboveDesertForTrees) {
    BiomeGen gen;
    CHECK(gen.decoratorsFor(BiomeKind::Jungle).treeDensity > gen.decoratorsFor(BiomeKind::Desert).treeDensity);
    CHECK(gen.decoratorsFor(BiomeKind::Desert).caneDensity > 0.0f);
    CHECK(gen.decoratorsFor(BiomeKind::Desert).treeDensity == 0.0f);
    CHECK(gen.decoratorsFor(BiomeKind::Ocean).treeDensity == 0.0f);
    CHECK(gen.decoratorsFor(BiomeKind::Ocean).bushDensity == 0.0f);
}

VF_TEST(biomeGenBiomeBlendWeightsSumToOne) {
    BiomeGen gen;
    for (int gx = -50; gx <= 50; gx += 17) {
        for (int gz = -50; gz <= 50; gz += 17) {
            BiomeWeights weights = gen.biomeBlend(gx, gz, 2);
            float total = 0.0f;
            for (float w : weights.weight) total += w;
            CHECK_NEAR(total, 1.0f, 1e-4f);
        }
    }
}

VF_TEST(biomeGenBiomeBlendIsPureVoteOfSampledBiomes) {
    BiomeGen gen;
    int gx = 4000, gz = -3500;
    BiomeWeights weights = gen.biomeBlend(gx, gz, 3);
    int sampleCount = 0;
    std::array<int, static_cast<std::size_t>(BiomeKind::Count)> counts{};
    for (int dz = -3; dz <= 3; ++dz) {
        for (int dx = -3; dx <= 3; ++dx) {
            BiomeKind biome = gen.biomeAt(gx + dx, gz + dz);
            counts[static_cast<std::size_t>(biome)] += 1;
            ++sampleCount;
        }
    }
    for (std::size_t i = 0; i < counts.size(); ++i) {
        float expected = static_cast<float>(counts[i]) / static_cast<float>(sampleCount);
        CHECK_NEAR(weights.weight[i], expected, 1e-5f);
    }
}

VF_TEST(biomeGenNameTableCoversEveryBiome) {
    std::set<std::string> names;
    for (int i = 0; i < static_cast<int>(BiomeKind::Count); ++i) {
        std::string name = biomeKindName(static_cast<BiomeKind>(i));
        CHECK(!name.empty());
        names.insert(name);
    }
    CHECK_EQ(names.size(), static_cast<std::size_t>(BiomeKind::Count));
}
