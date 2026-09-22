#include "testHarness.h"
#include "vf/Buoyancy.h"

using namespace vf;

namespace {

constexpr float playerWidth = 0.6f;
constexpr float playerHeight = 1.8f;
constexpr float playerEye = 1.62f;

void fillColumn(World& world, Block block, int y0, int y1) {
    for (int x = -2; x <= 2; ++x)
        for (int z = -2; z <= 2; ++z)
            for (int y = y0; y <= y1; ++y)
                world.setBlock(x, y, z, block);
}

SweptBox playerAt(float x, float y, float z) {
    return makeBoxAtFeet({x, y, z}, playerWidth, playerHeight);
}

FluidSample eyesUnderWater() {
    FluidSample sample;
    sample.fluid = Block::Water;
    sample.submergedFraction = 1.0f;
    sample.feetInFluid = true;
    sample.eyesInFluid = true;
    return sample;
}

FluidSample standingInLava() {
    FluidSample sample;
    sample.fluid = Block::Lava;
    sample.submergedFraction = 0.4f;
    sample.feetInFluid = true;
    return sample;
}

}

VF_TEST(buoyancyFluidTablesCoverWaterAndLava) {
    BuoyancyParams params;
    CHECK(isFluidBlock(Block::Water));
    CHECK(isFluidBlock(Block::Lava));
    CHECK(!isFluidBlock(Block::Air));
    CHECK(!isFluidBlock(Block::Ice));
    CHECK_NEAR(fluidDensity(Block::Water, params), params.waterDensity, 1e-6f);
    CHECK_NEAR(fluidDensity(Block::Lava, params), params.lavaDensity, 1e-6f);
    CHECK_NEAR(fluidDensity(Block::Air, params), 0.0f, 1e-6f);
    CHECK_NEAR(fluidLinearDrag(Block::Water, params), params.waterLinearDrag, 1e-6f);
    CHECK_NEAR(fluidLinearDrag(Block::Lava, params), params.lavaLinearDrag, 1e-6f);
    CHECK_NEAR(fluidLinearDrag(Block::Stone, params), 1.0f, 1e-6f);
}

VF_TEST(buoyancySampleFlagsRequireSubmersion) {
    FluidSample touching;
    touching.fluid = Block::Water;
    CHECK(!touching.inFluid());
    CHECK(!touching.inWater());
    CHECK(!touching.inLava());

    touching.submergedFraction = 0.2f;
    CHECK(touching.inFluid());
    CHECK(touching.inWater());
    CHECK(!touching.inLava());

    FluidSample dry;
    CHECK(!dry.inFluid());
    CHECK(!dry.inWater());
}

VF_TEST(buoyancySampleOutsideFluidIsDry) {
    World world;
    fillColumn(world, Block::Stone, -1, -1);
    FluidSample sample = sampleFluid(world, playerAt(0.5f, 0.0f, 0.5f), playerEye);
    CHECK(sample.fluid == Block::Air);
    CHECK(!sample.inFluid());
    CHECK_NEAR(sample.submergedFraction, 0.0f, 1e-6f);
    CHECK(!sample.feetInFluid);
    CHECK(!sample.eyesInFluid);
}

VF_TEST(buoyancySampleMeasuresDeepWater) {
    World world;
    fillColumn(world, Block::Water, 0, 2);
    FluidSample sample = sampleFluid(world, playerAt(0.5f, 0.0f, 0.5f), playerEye);
    CHECK(sample.inWater());
    CHECK_NEAR(sample.submergedFraction, 1.0f, 1e-5f);
    CHECK_NEAR(sample.submergedVolume, playerWidth * playerWidth * playerHeight, 1e-5f);
    CHECK_NEAR(sample.surfaceY, 3.0f, 1e-5f);
    CHECK(sample.feetInFluid);
    CHECK(sample.eyesInFluid);
    CHECK_NEAR(sample.eyeDepth, 3.0f - playerEye, 1e-5f);
}

VF_TEST(buoyancySampleMeasuresWadingDepth) {
    World world;
    fillColumn(world, Block::Water, 0, 0);
    FluidSample sample = sampleFluid(world, playerAt(0.5f, 0.0f, 0.5f), playerEye);
    CHECK(sample.inWater());
    CHECK_NEAR(sample.submergedFraction, 1.0f / playerHeight, 1e-5f);
    CHECK_NEAR(sample.surfaceY, 1.0f, 1e-5f);
    CHECK(sample.feetInFluid);
    CHECK(!sample.eyesInFluid);
    CHECK_NEAR(sample.eyeDepth, 0.0f, 1e-6f);
}

VF_TEST(buoyancySampleSpansSeveralCellsHorizontally) {
    World world;
    fillColumn(world, Block::Water, 0, 2);
    FluidSample sample = sampleFluid(world, playerAt(1.0f, 0.5f, 1.0f), playerEye);
    CHECK_NEAR(sample.submergedFraction, 1.0f, 1e-5f);
    CHECK_NEAR(sample.eyeDepth, 3.0f - 0.5f - playerEye, 1e-5f);
}

VF_TEST(buoyancySamplePrefersTheDominantFluid) {
    World world;
    fillColumn(world, Block::Lava, 0, 0);
    fillColumn(world, Block::Water, 1, 2);

    FluidSample mostlyWater = sampleFluid(world, playerAt(0.5f, 0.5f, 0.5f), playerEye);
    CHECK(mostlyWater.inWater());
    CHECK_NEAR(mostlyWater.submergedFraction, 1.3f / playerHeight, 1e-5f);
    CHECK(!mostlyWater.feetInFluid);
    CHECK(mostlyWater.eyesInFluid);

    World lavaPool;
    fillColumn(lavaPool, Block::Lava, 0, 2);
    FluidSample lava = sampleFluid(lavaPool, playerAt(0.5f, 0.0f, 0.5f), playerEye);
    CHECK(lava.inLava());
    CHECK(lava.eyesInFluid);
}

VF_TEST(buoyancySampleReadsPartialLevelsFromTheSimulation) {
    World world;
    FluidSim sim(world, {-2, 0, -2}, {5, 4, 5});
    sim.setLevel(0, 0, 0, fluidMax / 2);
    world.setBlock(1, 0, 0, Block::Water);

    SweptBox box = makeBoxAtFeet({0.5f, 0.0f, 0.5f}, 0.6f, 1.8f);
    FluidSample sample = sampleFluid(world, sim, box, playerEye);
    CHECK(sample.inWater());
    CHECK_NEAR(sample.surfaceY, 0.5f, 1e-5f);
    CHECK_NEAR(sample.submergedFraction, 0.5f / 1.8f, 1e-5f);
    CHECK(sample.feetInFluid);

    FluidSample stale = sampleFluid(world, sim, makeBoxAtFeet({1.5f, 0.0f, 0.5f}, 0.6f, 1.8f),
                                    playerEye);
    CHECK(!stale.inFluid());
    CHECK(sampleFluid(world, makeBoxAtFeet({1.5f, 0.0f, 0.5f}, 0.6f, 1.8f), playerEye).inWater());
}

VF_TEST(buoyancyVelocityAppliesGravityOutsideFluid) {
    BuoyancyParams params;
    FluidSample dry;
    Vec3 velocity = buoyantVelocity(dry, {0.5f, 0.0f, 0.0f}, params, true, false);
    CHECK_NEAR(velocity.x, 0.5f, 1e-6f);
    CHECK_NEAR(velocity.y, -params.gravity, 1e-6f);
}

VF_TEST(buoyancyWaterSinksSlowlyAndLavaFloats) {
    BuoyancyParams params;
    Vec3 inWater = buoyantVelocity(eyesUnderWater(), {0.0f, 0.0f, 0.0f}, params, false, false);
    CHECK(inWater.y < 0.0f);
    CHECK(inWater.y > -params.gravity);
    CHECK_NEAR(inWater.y, -0.0128f, 1e-4f);

    FluidSample lava = eyesUnderWater();
    lava.fluid = Block::Lava;
    Vec3 inLava = buoyantVelocity(lava, {0.0f, 0.0f, 0.0f}, params, false, false);
    CHECK(inLava.y > 0.0f);
}

VF_TEST(buoyancySwimmingAndDragShapeTheVelocity) {
    BuoyancyParams params;
    FluidSample water = eyesUnderWater();

    Vec3 swimming = buoyantVelocity(water, {0.0f, 0.0f, 0.0f}, params, true, false);
    CHECK(swimming.y > 0.0f);
    Vec3 diving = buoyantVelocity(water, {0.0f, 0.0f, 0.0f}, params, false, true);
    Vec3 idle = buoyantVelocity(water, {0.0f, 0.0f, 0.0f}, params, false, false);
    CHECK(diving.y < idle.y);

    Vec3 coasting = buoyantVelocity(water, {1.0f, 0.0f, 0.0f}, params, false, false);
    CHECK(coasting.x < params.waterLinearDrag);
    CHECK(coasting.x > 0.0f);

    FluidSample shallow = water;
    shallow.submergedFraction = 0.25f;
    Vec3 wading = buoyantVelocity(shallow, {1.0f, 0.0f, 0.0f}, params, false, false);
    CHECK(wading.x > coasting.x);

    Vec3 rising = buoyantVelocity(water, {0.0f, 2.0f, 0.0f}, params, true, false);
    CHECK_NEAR(rising.y, params.maxRiseSpeed, 1e-6f);
}

VF_TEST(buoyancyBreathRunsOutThenDrowns) {
    BuoyancyParams params;
    BreathState breath;
    EffectSet effects;
    DamageEvent hit;
    FluidSample water = eyesUnderWater();

    for (int tick = 0; tick < params.maxAirTicks; ++tick)
        CHECK(!updateBreath(water, breath, params, effects, {}, hit));
    CHECK_EQ(breath.airTicks, 0);

    int damageTicks = 0;
    for (int tick = 0; tick < params.drownIntervalTicks * 3; ++tick)
        if (updateBreath(water, breath, params, effects, {1.0f, 2.0f, 3.0f}, hit)) ++damageTicks;
    CHECK_EQ(damageTicks, 3);
    CHECK(hit.source.type == DamageType::Drowning);
    CHECK_NEAR(hit.amount, params.drownDamage, 1e-6f);
    CHECK_NEAR(hit.source.origin.y, 2.0f, 1e-6f);

    FluidSample surfaced = water;
    surfaced.eyesInFluid = false;
    CHECK(!updateBreath(surfaced, breath, params, effects, {}, hit));
    CHECK_EQ(breath.airTicks, params.airRefillPerTick);
    CHECK_EQ(breath.drownTimer, 0);
    for (int tick = 0; tick < params.maxAirTicks; ++tick)
        updateBreath(surfaced, breath, params, effects, {}, hit);
    CHECK_EQ(breath.airTicks, params.maxAirTicks);
}

VF_TEST(buoyancyWaterBreathingKeepsAir) {
    BuoyancyParams params;
    BreathState breath;
    EffectSet effects;
    applyEffect(effects, makeEffect(EffectKind::WaterBreathing, 1, 6000));
    DamageEvent hit;

    for (int tick = 0; tick < params.maxAirTicks * 2; ++tick)
        CHECK(!updateBreath(eyesUnderWater(), breath, params, effects, {}, hit));
    CHECK_EQ(breath.airTicks, params.maxAirTicks);
}

VF_TEST(buoyancyLavaBurnsUnlessFireResistant) {
    BuoyancyParams params;
    BreathState breath;
    EffectSet effects;
    DamageEvent hit;

    int burns = 0;
    for (int tick = 0; tick < params.lavaBurnIntervalTicks * 2; ++tick)
        if (updateBreath(standingInLava(), breath, params, effects, {}, hit)) ++burns;
    CHECK_EQ(burns, 2);
    CHECK(hit.source.type == DamageType::Lava);
    CHECK(hit.source.fiery);
    CHECK_NEAR(hit.amount, params.lavaBurnDamage, 1e-6f);
    CHECK_EQ(breath.airTicks, params.maxAirTicks);

    EffectSet resistant;
    applyEffect(resistant, makeEffect(EffectKind::FireResistance, 1, 6000));
    BreathState safe;
    for (int tick = 0; tick < params.lavaBurnIntervalTicks * 2; ++tick)
        CHECK(!updateBreath(standingInLava(), safe, params, resistant, {}, hit));
    CHECK_EQ(safe.burnTimer, 0);
}

VF_TEST(buoyancyCurrentFollowsFallingLevels) {
    CurrentField detached;
    CHECK(!detached.valid());
    Vec3 none = detached.flowAt(0, 0, 0);
    CHECK_NEAR(length(none), 0.0f, 1e-6f);

    World world;
    FluidSim sim(world, {-4, 0, -4}, {9, 2, 9});
    for (int x = -4; x <= 4; ++x)
        for (int z = -4; z <= 4; ++z)
            world.setBlock(x, -1, z, Block::Stone);
    sim.setLevel(-1, 0, 0, 8);
    sim.setLevel(0, 0, 0, 7);
    sim.setLevel(1, 0, 0, 6);
    for (int x = -1; x <= 1; ++x) {
        world.setBlock(x, 0, 1, Block::Stone);
        world.setBlock(x, 0, -1, Block::Stone);
    }

    CurrentField current(world, sim);
    CHECK(current.valid());
    CHECK(current.fluid() == &sim);
    Vec3 flow = current.flowAt(0, 0, 0);
    CHECK_NEAR(flow.x, 1.0f, 1e-5f);
    CHECK_NEAR(flow.z, 0.0f, 1e-5f);
    CHECK_NEAR(length(current.flowAt(3, 0, 3)), 0.0f, 1e-6f);

    Vec3 boxFlow = current.flowAtBox(makeBoxAtFeet({0.5f, 0.0f, 0.5f}, 0.6f, 1.8f));
    CHECK(boxFlow.x > 0.9f);
}

VF_TEST(buoyancyStillPoolHasNoCurrent) {
    World world;
    FluidSim sim(world, {0, 0, 0}, {3, 1, 3});
    for (int x = -1; x <= 3; ++x)
        for (int z = -1; z <= 3; ++z) {
            bool rim = x < 0 || x > 2 || z < 0 || z > 2;
            if (rim) world.setBlock(x, 0, z, Block::Stone);
            else sim.setLevel(x, 0, z, fluidMax);
        }
    CurrentField current(world, sim);
    for (int x = 0; x <= 2; ++x)
        for (int z = 0; z <= 2; ++z)
            CHECK_NEAR(length(current.flowAt(x, 0, z)), 0.0f, 1e-6f);
}

VF_TEST(buoyancyStepCombinesForcesCurrentAndBreath) {
    World world;
    FluidSim sim(world, {-4, 0, -4}, {9, 4, 9});
    for (int x = -4; x <= 4; ++x)
        for (int z = -4; z <= 4; ++z)
            for (int y = 0; y <= 2; ++y)
                sim.setLevel(x, y, z, static_cast<uint8_t>(x <= 0 ? fluidMax : fluidMax - x));
    CurrentField current(world, sim);
    BuoyancyParams params;
    EffectSet effects;
    BreathState breath;
    breath.airTicks = 1;
    breath.drownTimer = params.drownIntervalTicks - 2;
    SweptBox box = makeBoxAtFeet({0.5f, 0.0f, 0.5f}, 0.6f, 1.8f);

    BuoyancyResult first = stepBuoyancy(world, box, {}, current, params, effects, breath, playerEye,
                                        false, false);
    CHECK(first.submerged);
    CHECK(first.eyesUnder);
    CHECK(first.sample.inWater());
    CHECK(first.currentPush.x > 0.0f);
    CHECK_NEAR(first.currentPush.z, 0.0f, 1e-6f);
    CHECK_NEAR(length(first.currentPush), params.currentStrength, 1e-4f);
    CHECK_NEAR(first.buoyantAcceleration.y, params.gravity * params.waterDensity, 1e-5f);
    CHECK(first.velocity.x > 0.0f);
    CHECK_NEAR(first.velocity.x, first.currentPush.x, 1e-6f);
    CHECK(first.drowning);
    CHECK(!first.tookFluidDamage);

    BuoyancyResult second = stepBuoyancy(world, box, first.velocity, current, params, effects,
                                         breath, playerEye, false, false);
    CHECK(second.drowning);
    CHECK(!second.tookFluidDamage);
    BuoyancyResult third = stepBuoyancy(world, box, second.velocity, current, params, effects,
                                        breath, playerEye, false, false);
    CHECK(third.tookFluidDamage);
    CHECK(third.fluidHit.source.type == DamageType::Drowning);

    World dryWorld;
    BreathState dryBreath;
    BuoyancyResult dry = stepBuoyancy(dryWorld, box, {}, CurrentField{}, params, effects, dryBreath,
                                      playerEye, false, false);
    CHECK(!dry.submerged);
    CHECK(!dry.sample.inFluid());
    CHECK_NEAR(dry.velocity.y, -params.gravity, 1e-6f);
    CHECK_NEAR(length(dry.currentPush), 0.0f, 1e-6f);
}
