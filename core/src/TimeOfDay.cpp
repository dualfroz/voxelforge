#include "vf/TimeOfDay.h"

#include <cmath>

namespace vf {

namespace {

WorldTick floorDivTick(WorldTick a, WorldTick b) {
    WorldTick q = a / b;
    WorldTick r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

WorldTick floorModTick(WorldTick a, WorldTick b) {
    WorldTick r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

WorldTick countBoundaryCrossings(WorldTick previous, WorldTick current,
                                 WorldTick boundary, WorldTick period) {
    WorldTick after = floorDivTick(current - boundary, period);
    WorldTick before = floorDivTick(previous - boundary, period);
    return after - before;
}

} // namespace

const char* dayPhaseName(DayPhase phase) {
    switch (phase) {
        case DayPhase::Dawn: return "dawn";
        case DayPhase::Day: return "day";
        case DayPhase::Dusk: return "dusk";
        case DayPhase::Night: return "night";
    }
    return "unknown";
}

TimeOfDay::TimeOfDay(WorldTick startTick, const TimeOfDayConfig& config)
    : config(config), tickCounter(startTick) {}

void TimeOfDay::advance(WorldTick ticks) {
    if (ticks <= 0) {
        sunriseCrossed = false;
        sunsetCrossed = false;
        return;
    }
    WorldTick previous = tickCounter;
    tickCounter += ticks;
    recomputeCrossings(previous, tickCounter);
}

void TimeOfDay::setTick(WorldTick absoluteTick) {
    WorldTick previous = tickCounter;
    tickCounter = absoluteTick;
    if (absoluteTick > previous) recomputeCrossings(previous, tickCounter);
    else {
        sunriseCrossed = false;
        sunsetCrossed = false;
    }
}

void TimeOfDay::reset(WorldTick absoluteTick) {
    tickCounter = absoluteTick;
    sunriseCrossed = false;
    sunsetCrossed = false;
}

void TimeOfDay::recomputeCrossings(WorldTick previous, WorldTick current) {
    WorldTick period = config.ticksPerDay;
    WorldTick sunriseTick = static_cast<WorldTick>(std::llround(config.sunriseFraction * period));
    WorldTick sunsetTick = static_cast<WorldTick>(std::llround(config.sunsetFraction * period));
    sunriseCrossed = countBoundaryCrossings(previous, current, sunriseTick, period) > 0;
    sunsetCrossed = countBoundaryCrossings(previous, current, sunsetTick, period) > 0;
}

WorldTick TimeOfDay::timeOfDayTicks() const {
    return floorModTick(tickCounter, config.ticksPerDay);
}

double TimeOfDay::dayFraction() const {
    return static_cast<double>(timeOfDayTicks()) / static_cast<double>(config.ticksPerDay);
}

WorldTick TimeOfDay::dayCount() const {
    return floorDivTick(tickCounter, config.ticksPerDay);
}

DayPhase TimeOfDay::phase() const {
    double f = dayFraction();
    if (f >= config.dawnStart && f < config.dawnEnd) return DayPhase::Dawn;
    if (f >= config.dawnEnd && f < config.duskStart) return DayPhase::Day;
    if (f >= config.duskStart && f < config.duskEnd) return DayPhase::Dusk;
    return DayPhase::Night;
}

bool TimeOfDay::isDaylight() const {
    DayPhase p = phase();
    return p == DayPhase::Day || p == DayPhase::Dawn || p == DayPhase::Dusk;
}

bool TimeOfDay::isNight() const { return phase() == DayPhase::Night; }

double TimeOfDay::skyBrightness() const {
    double f = dayFraction();
    double nightLevel = static_cast<double>(config.nightSkyLight) /
                        static_cast<double>(config.daySkyLight);
    switch (phase()) {
        case DayPhase::Day:
            return 1.0;
        case DayPhase::Night:
            return nightLevel;
        case DayPhase::Dawn: {
            double t = (f - config.dawnStart) / (config.dawnEnd - config.dawnStart);
            return nightLevel + clamp01(t) * (1.0 - nightLevel);
        }
        case DayPhase::Dusk: {
            double t = (f - config.duskStart) / (config.duskEnd - config.duskStart);
            return 1.0 - clamp01(t) * (1.0 - nightLevel);
        }
    }
    return nightLevel;
}

int TimeOfDay::skyLightLevel() const {
    double level = skyBrightness() * static_cast<double>(config.daySkyLight);
    int rounded = static_cast<int>(std::lround(level));
    if (rounded < 0) rounded = 0;
    if (rounded > config.daySkyLight) rounded = config.daySkyLight;
    return rounded;
}

double TimeOfDay::sunHeight() const {
    double f = dayFraction();
    return std::sin((f - config.sunriseFraction) * 2.0 * M_PI);
}

int TimeOfDay::moonPhase() const {
    WorldTick day = dayCount();
    WorldTick m = day % config.moonPhaseCount;
    if (m < 0) m += config.moonPhaseCount;
    return static_cast<int>(m);
}

double TimeOfDay::moonBrightness() const {
    int half = config.moonPhaseCount / 2;
    int distance = std::abs(moonPhase() - half);
    return 1.0 - static_cast<double>(distance) / static_cast<double>(half);
}

} // namespace vf
