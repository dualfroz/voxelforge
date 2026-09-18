#include "vf/Weather.h"

namespace vf {

namespace {

double approach(double value, double target, double step) {
    if (value < target) {
        value += step;
        if (value > target) value = target;
    } else if (value > target) {
        value -= step;
        if (value < target) value = target;
    }
    return value;
}

}

const char* weatherStateName(WeatherState state) {
    switch (state) {
        case WeatherState::Clear: return "clear";
        case WeatherState::Rain: return "rain";
        case WeatherState::Thunder: return "thunder";
    }
    return "unknown";
}

Weather::Weather(uint32_t seed, const WeatherConfig& config)
    : config(config), rngState(seed == 0 ? 0x9e3779b9u : seed) {
    stateTimer = rollDuration(config.clearMin, config.clearMax);
}

uint32_t Weather::nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

long long Weather::rollDuration(long long lo, long long hi) {
    if (hi <= lo) return lo;
    long long span = hi - lo;
    return lo + static_cast<long long>(nextRandom() % static_cast<uint32_t>(span));
}

void Weather::enterState(WeatherState next) {
    currentState = next;
    switch (next) {
        case WeatherState::Clear:
            stateTimer = rollDuration(config.clearMin, config.clearMax);
            break;
        case WeatherState::Rain:
            stateTimer = rollDuration(config.rainMin, config.rainMax);
            break;
        case WeatherState::Thunder:
            stateTimer = rollDuration(config.thunderMin, config.thunderMax);
            break;
    }
}

void Weather::stepGradients() {
    double rainTarget = currentState == WeatherState::Clear ? 0.0 : 1.0;
    double thunderTarget = currentState == WeatherState::Thunder ? 1.0 : 0.0;
    rainLevel = approach(rainLevel, rainTarget, config.rainRampPerTick);
    thunderLevel = approach(thunderLevel, thunderTarget, config.thunderRampPerTick);
}

void Weather::stepLightning() {
    if (currentState != WeatherState::Thunder) return;
    if (config.strikeInterval <= 0) return;
    if (nextRandom() % static_cast<uint32_t>(config.strikeInterval) != 0) return;

    int spanX = config.strikeMaxX - config.strikeMinX;
    int spanZ = config.strikeMaxZ - config.strikeMinZ;
    int x = spanX > 0 ? config.strikeMinX + static_cast<int>(nextRandom() % static_cast<uint32_t>(spanX))
                      : config.strikeMinX;
    int z = spanZ > 0 ? config.strikeMinZ + static_cast<int>(nextRandom() % static_cast<uint32_t>(spanZ))
                      : config.strikeMinZ;
    tickStrikes.push_back({Vec3i{x, config.strikeGroundY, z}, tickCounter});
    ++strikeCount;
}

void Weather::tick() {
    tickStrikes.clear();
    --stateTimer;
    if (stateTimer <= 0) {
        switch (currentState) {
            case WeatherState::Clear:
                enterState(WeatherState::Rain);
                break;
            case WeatherState::Rain:
                if (nextRandom() % 256u < config.thunderChance) enterState(WeatherState::Thunder);
                else enterState(WeatherState::Clear);
                break;
            case WeatherState::Thunder:
                enterState(WeatherState::Rain);
                break;
        }
    }
    stepGradients();
    stepLightning();
    ++tickCounter;
}

void Weather::advance(long long ticks) {
    for (long long i = 0; i < ticks; ++i) tick();
}

Precipitation Weather::precipitationAt(float temperature) const {
    if (rainLevel <= 0.0 && currentState == WeatherState::Clear) return Precipitation::None;
    if (currentState == WeatherState::Clear) return Precipitation::None;
    return temperature < config.snowTemperature ? Precipitation::Snow : Precipitation::Rain;
}

}
