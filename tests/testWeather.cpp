#include "testHarness.h"
#include "vf/Weather.h"

using namespace vf;

namespace {

void runToState(Weather& weather, WeatherState target, long long limit) {
    for (long long i = 0; i < limit && weather.state() != target; ++i) weather.tick();
}

} // namespace

VF_TEST(weatherStartsClearWithZeroGradient) {
    Weather weather(1u);
    CHECK(weather.state() == WeatherState::Clear);
    CHECK(!weather.isRaining());
    for (int i = 0; i < 5; ++i) {
        weather.tick();
        CHECK_NEAR(weather.rainGradient(), 0.0, 1e-9);
    }
}

VF_TEST(clearEventuallyBecomesRain) {
    WeatherConfig cfg;
    cfg.clearMin = 40;
    cfg.clearMax = 40;
    Weather weather(7u, cfg);
    runToState(weather, WeatherState::Rain, 200);
    CHECK(weather.state() == WeatherState::Rain);
    CHECK(weather.isRaining());
}

VF_TEST(rainGradientRampsUpWhileRainingAndDownWhenClear) {
    WeatherConfig cfg;
    cfg.clearMin = cfg.clearMax = 40;
    cfg.rainMin = cfg.rainMax = 60;
    cfg.thunderChance = 0;
    cfg.rainRampPerTick = 0.25;
    Weather weather(3u, cfg);

    runToState(weather, WeatherState::Rain, 200);
    for (int i = 0; i < 20; ++i) weather.tick();
    double wet = weather.rainGradient();
    CHECK(wet > 0.9);

    runToState(weather, WeatherState::Clear, 200);
    for (int i = 0; i < 20; ++i) weather.tick();
    double dry = weather.rainGradient();
    CHECK(dry < 0.1);
    CHECK(wet > dry);
}

VF_TEST(precipitationDependsOnStateAndTemperature) {
    WeatherConfig cfg;
    cfg.clearMin = cfg.clearMax = 30;
    Weather weather(5u, cfg);
    CHECK(weather.precipitationAt(-1.0f) == Precipitation::None);

    runToState(weather, WeatherState::Rain, 200);
    CHECK(weather.precipitationAt(-1.0f) == Precipitation::Snow);
    CHECK(weather.precipitationAt(1.0f) == Precipitation::Rain);
}

VF_TEST(lightningOnlyStrikesWhileThundering) {
    WeatherConfig cfg;
    cfg.clearMin = cfg.clearMax = 20;
    cfg.rainMin = cfg.rainMax = 20;
    cfg.thunderMin = cfg.thunderMax = 300;
    cfg.thunderChance = 256;
    cfg.strikeInterval = 8;
    Weather weather(11u, cfg);

    long long strikesBeforeThunder = 0;
    for (int i = 0; i < 40; ++i) {
        weather.tick();
        if (!weather.isThundering()) strikesBeforeThunder += weather.strikesThisTick().size();
    }
    CHECK_EQ(strikesBeforeThunder, static_cast<long long>(0));

    runToState(weather, WeatherState::Thunder, 200);
    long long strikesDuringThunder = 0;
    for (int i = 0; i < 200; ++i) {
        weather.tick();
        for (const LightningStrike& strike : weather.strikesThisTick()) {
            CHECK(strike.pos.x >= cfg.strikeMinX && strike.pos.x < cfg.strikeMaxX);
            CHECK(strike.pos.z >= cfg.strikeMinZ && strike.pos.z < cfg.strikeMaxZ);
            ++strikesDuringThunder;
        }
    }
    CHECK(strikesDuringThunder > 0);
}

VF_TEST(sameSeedProducesIdenticalWeatherAndStrikes) {
    WeatherConfig cfg;
    cfg.clearMin = 50;
    cfg.clearMax = 200;
    cfg.rainMin = 50;
    cfg.rainMax = 200;
    cfg.thunderMin = 50;
    cfg.thunderMax = 200;
    cfg.thunderChance = 128;
    cfg.strikeInterval = 20;

    Weather left(99u, cfg);
    Weather right(99u, cfg);
    for (int i = 0; i < 3000; ++i) {
        left.tick();
        right.tick();
        CHECK(left.state() == right.state());
        CHECK_NEAR(left.rainGradient(), right.rainGradient(), 1e-12);
        CHECK_NEAR(left.thunderGradient(), right.thunderGradient(), 1e-12);
        CHECK_EQ(left.strikesThisTick().size(), right.strikesThisTick().size());
        for (std::size_t k = 0; k < left.strikesThisTick().size(); ++k) {
            CHECK(left.strikesThisTick()[k].pos == right.strikesThisTick()[k].pos);
        }
    }
    CHECK_EQ(left.totalStrikes(), right.totalStrikes());
}

VF_TEST(differentSeedsDivergeInStrikeTiming) {
    WeatherConfig cfg;
    cfg.clearMin = cfg.clearMax = 20;
    cfg.rainMin = cfg.rainMax = 20;
    cfg.thunderMin = cfg.thunderMax = 400;
    cfg.thunderChance = 256;
    cfg.strikeInterval = 8;

    Weather left(1u, cfg);
    Weather right(2u, cfg);
    bool diverged = false;
    for (int i = 0; i < 600; ++i) {
        left.tick();
        right.tick();
        if (left.strikesThisTick().size() != right.strikesThisTick().size()) {
            diverged = true;
            break;
        }
        for (std::size_t k = 0; k < left.strikesThisTick().size(); ++k) {
            if (left.strikesThisTick()[k].pos != right.strikesThisTick()[k].pos) {
                diverged = true;
                break;
            }
        }
        if (diverged) break;
    }
    CHECK(diverged);
}
