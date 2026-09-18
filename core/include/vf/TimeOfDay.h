#pragma once

#include <cstdint>

namespace vf {

using WorldTick = long long;

enum class DayPhase : uint8_t {
    Dawn = 0,
    Day = 1,
    Dusk = 2,
    Night = 3,
};

const char* dayPhaseName(DayPhase phase);

struct TimeOfDayConfig {
    WorldTick ticksPerDay = 24000;
    double dawnStart = 0.20;
    double dawnEnd = 0.30;
    double duskStart = 0.70;
    double duskEnd = 0.80;
    double sunriseFraction = 0.25;
    double sunsetFraction = 0.75;
    int nightSkyLight = 4;
    int daySkyLight = 15;
    int moonPhaseCount = 8;
};

class TimeOfDay {
public:
    explicit TimeOfDay(WorldTick startTick = 0, const TimeOfDayConfig& config = {});

    void advance(WorldTick ticks = 1);
    void setTick(WorldTick absoluteTick);
    void reset(WorldTick absoluteTick = 0);

    WorldTick absoluteTick() const { return tickCounter; }
    WorldTick timeOfDayTicks() const;
    double dayFraction() const;
    WorldTick dayCount() const;

    DayPhase phase() const;
    bool isDaylight() const;
    bool isNight() const;

    int skyLightLevel() const;
    double skyBrightness() const;
    double sunHeight() const;

    int moonPhase() const;
    double moonBrightness() const;

    bool crossedSunrise() const { return sunriseCrossed; }
    bool crossedSunset() const { return sunsetCrossed; }

    const TimeOfDayConfig& settings() const { return config; }

private:
    void recomputeCrossings(WorldTick previous, WorldTick current);

    TimeOfDayConfig config;
    WorldTick tickCounter;
    bool sunriseCrossed = false;
    bool sunsetCrossed = false;
};

}
