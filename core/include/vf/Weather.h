#pragma once

#include <cstdint>
#include <vector>
#include "Math.h"

namespace vf {

enum class WeatherState : uint8_t {
    Clear = 0,
    Rain = 1,
    Thunder = 2,
};

enum class Precipitation : uint8_t {
    None = 0,
    Rain = 1,
    Snow = 2,
};

const char* weatherStateName(WeatherState state);

struct LightningStrike {
    Vec3i pos;
    long long tick;
};

struct WeatherConfig {
    long long clearMin = 6000;
    long long clearMax = 18000;
    long long rainMin = 6000;
    long long rainMax = 12000;
    long long thunderMin = 3000;
    long long thunderMax = 9000;
    // Odds (in 256ths) that a rain spell escalates into a thunderstorm.
    uint32_t thunderChance = 96;
    // Mean ticks between lightning strikes while thundering.
    long long strikeInterval = 120;
    // Horizontal window strikes are placed in, and their nominal ground height.
    int strikeMinX = -128;
    int strikeMaxX = 128;
    int strikeMinZ = -128;
    int strikeMaxZ = 128;
    int strikeGroundY = 64;
    // Per-tick gradient ramp toward the target intensity.
    double rainRampPerTick = 0.01;
    double thunderRampPerTick = 0.01;
    // Below this biome temperature falling precipitation is snow, not rain.
    float snowTemperature = 0.15f;
};

// Deterministic weather state machine. Clear, Rain and Thunder alternate on
// seeded durations; a rain spell may escalate to thunder and always decays back
// through rain before clearing. Rain and thunder gradients ramp smoothly so the
// sky fades rather than snapping. While thundering, lightning strikes are placed
// at seeded positions. Two Weather instances with the same seed and config
// produce identical state, gradient and strike sequences.
class Weather {
public:
    explicit Weather(uint32_t seed = 1u, const WeatherConfig& config = {});

    void tick();
    void advance(long long ticks);

    WeatherState state() const { return currentState; }
    bool isRaining() const { return currentState != WeatherState::Clear; }
    bool isThundering() const { return currentState == WeatherState::Thunder; }

    double rainGradient() const { return rainLevel; }
    double thunderGradient() const { return thunderLevel; }
    long long ticksUntilChange() const { return stateTimer; }
    long long currentTick() const { return tickCounter; }

    Precipitation precipitationAt(float temperature) const;

    const std::vector<LightningStrike>& strikesThisTick() const { return tickStrikes; }
    long long totalStrikes() const { return strikeCount; }

    const WeatherConfig& settings() const { return config; }

private:
    uint32_t nextRandom();
    long long rollDuration(long long lo, long long hi);
    void enterState(WeatherState next);
    void stepGradients();
    void stepLightning();

    WeatherConfig config;
    uint32_t rngState;
    WeatherState currentState = WeatherState::Clear;
    long long stateTimer;
    long long tickCounter = 0;
    double rainLevel = 0.0;
    double thunderLevel = 0.0;
    long long strikeCount = 0;
    std::vector<LightningStrike> tickStrikes;
};

} // namespace vf
