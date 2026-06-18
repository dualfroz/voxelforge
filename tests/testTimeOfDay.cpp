#include "testHarness.h"
#include "vf/TimeOfDay.h"

using namespace vf;

VF_TEST(dayFractionAndCountTrackAbsoluteTick) {
    TimeOfDay clock;
    CHECK_EQ(clock.dayCount(), static_cast<WorldTick>(0));
    CHECK_NEAR(clock.dayFraction(), 0.0, 1e-9);

    clock.setTick(12000);
    CHECK_NEAR(clock.dayFraction(), 0.5, 1e-9);
    CHECK_EQ(clock.timeOfDayTicks(), static_cast<WorldTick>(12000));

    clock.setTick(24000 * 3 + 6000);
    CHECK_EQ(clock.dayCount(), static_cast<WorldTick>(3));
    CHECK_NEAR(clock.dayFraction(), 0.25, 1e-9);
}

VF_TEST(phasesMapToClockPositions) {
    TimeOfDay clock;
    clock.setTick(0);
    CHECK(clock.phase() == DayPhase::Night);
    clock.setTick(6000);
    CHECK(clock.phase() == DayPhase::Dawn);
    clock.setTick(12000);
    CHECK(clock.phase() == DayPhase::Day);
    clock.setTick(18000);
    CHECK(clock.phase() == DayPhase::Dusk);
    clock.setTick(21600);
    CHECK(clock.phase() == DayPhase::Night);
}

VF_TEST(skyLightPeaksAtNoonAndBottomsAtMidnight) {
    TimeOfDay clock;
    clock.setTick(12000);
    CHECK_EQ(clock.skyLightLevel(), 15);
    clock.setTick(0);
    CHECK_EQ(clock.skyLightLevel(), 4);

    clock.setTick(6000);
    int dawnLevel = clock.skyLightLevel();
    CHECK(dawnLevel > 4);
    CHECK(dawnLevel < 15);
}

VF_TEST(skyLightRisesMonotonicallyThroughDawn) {
    TimeOfDay clock;
    int previous = -1;
    for (WorldTick t = 4800; t <= 7200; t += 200) {
        clock.setTick(t);
        int level = clock.skyLightLevel();
        CHECK(level >= previous);
        previous = level;
    }
    clock.setTick(4800);
    CHECK_EQ(clock.skyLightLevel(), 4);
    clock.setTick(7200);
    CHECK_EQ(clock.skyLightLevel(), 15);
}

VF_TEST(sunHeightPositiveByDayNegativeByNight) {
    TimeOfDay clock;
    clock.setTick(12000);
    CHECK(clock.sunHeight() > 0.99);
    clock.setTick(0);
    CHECK(clock.sunHeight() < -0.99);
}

VF_TEST(moonPhaseAdvancesOncePerDayAndWraps) {
    TimeOfDay clock;
    clock.setTick(0);
    CHECK_EQ(clock.moonPhase(), 0);
    clock.setTick(24000);
    CHECK_EQ(clock.moonPhase(), 1);
    clock.setTick(24000 * 7);
    CHECK_EQ(clock.moonPhase(), 7);
    clock.setTick(24000 * 8);
    CHECK_EQ(clock.moonPhase(), 0);
}

VF_TEST(sunriseCrossingDetectedExactlyOnce) {
    TimeOfDay clock(0);
    clock.advance(5999);
    CHECK(!clock.crossedSunrise());
    clock.advance(1);
    CHECK(clock.crossedSunrise());
    CHECK(!clock.crossedSunset());

    clock.advance(1);
    CHECK(!clock.crossedSunrise());
}

VF_TEST(sunsetCrossingDetectedAcrossInterval) {
    TimeOfDay clock(6000);
    clock.advance(12000);
    CHECK(clock.crossedSunset());
    CHECK(!clock.crossedSunrise());
    CHECK_EQ(clock.absoluteTick(), static_cast<WorldTick>(18000));
}

VF_TEST(advanceAccumulatesDaysDeterministically) {
    TimeOfDay a(0);
    for (int i = 0; i < 240; ++i) a.advance(100);
    TimeOfDay b(0);
    b.advance(24000);
    CHECK_EQ(a.absoluteTick(), b.absoluteTick());
    CHECK_EQ(a.dayCount(), b.dayCount());
    CHECK_NEAR(a.dayFraction(), b.dayFraction(), 1e-9);
}
