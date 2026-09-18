#pragma once

#include <cstdint>

#include "Collide.h"
#include "Damage.h"
#include "Effect.h"
#include "Fluid.h"
#include "Math.h"
#include "World.h"

namespace vf {

bool isFluidBlock(Block block);

struct FluidSample {
    Block fluid = Block::Air;
    float submergedFraction = 0.0f;
    float submergedVolume = 0.0f;
    float surfaceY = 0.0f;
    float eyeDepth = 0.0f;
    bool feetInFluid = false;
    bool eyesInFluid = false;

    bool inFluid() const { return fluid != Block::Air && submergedFraction > 0.0f; }
    bool inWater() const { return inFluid() && fluid == Block::Water; }
    bool inLava() const { return inFluid() && fluid == Block::Lava; }
};

FluidSample sampleFluid(const World& world, const SweptBox& box, float eyeHeight);
FluidSample sampleFluid(const World& world, const FluidSim& fluid, const SweptBox& box,
                        float eyeHeight);

struct BuoyancyParams {
    float gravity = 0.08f;
    float waterDensity = 0.80f;
    float lavaDensity = 1.40f;
    float waterLinearDrag = 0.80f;
    float lavaLinearDrag = 0.50f;
    float quadraticDrag = 0.03f;
    float swimUpAccel = 0.039f;
    float sinkAccel = 0.030f;
    float currentStrength = 0.020f;
    float maxRiseSpeed = 0.36f;
    int maxAirTicks = 300;
    int airRefillPerTick = 4;
    int drownIntervalTicks = 20;
    float drownDamage = 2.0f;
    int lavaBurnIntervalTicks = 10;
    float lavaBurnDamage = 4.0f;
};

float fluidDensity(Block fluid, const BuoyancyParams& params);
float fluidLinearDrag(Block fluid, const BuoyancyParams& params);

struct BreathState {
    int airTicks = 300;
    int drownTimer = 0;
    int burnTimer = 0;
};

class CurrentField {
public:
    CurrentField() = default;
    CurrentField(const World& world, const FluidSim& fluid);

    void attach(const World& world, const FluidSim& fluid);
    bool valid() const { return world_ != nullptr && fluid_ != nullptr; }
    const FluidSim* fluid() const { return fluid_; }

    Vec3 flowAt(int gx, int gy, int gz) const;
    Vec3 flowAtBox(const SweptBox& box) const;

private:
    const World* world_ = nullptr;
    const FluidSim* fluid_ = nullptr;
};

struct BuoyancyResult {
    FluidSample sample;
    Vec3 velocity;
    Vec3 buoyantAcceleration;
    Vec3 dragDelta;
    Vec3 currentPush;
    bool submerged = false;
    bool eyesUnder = false;
    bool drowning = false;
    bool tookFluidDamage = false;
    DamageEvent fluidHit;
};

Vec3 buoyantVelocity(const FluidSample& sample, const Vec3& velocity,
                     const BuoyancyParams& params, bool swimUp, bool sinkDown);

bool updateBreath(const FluidSample& sample, BreathState& breath, const BuoyancyParams& params,
                  const EffectSet& effects, const Vec3& position, DamageEvent& hit);

BuoyancyResult stepBuoyancy(const World& world, const SweptBox& box, const Vec3& velocity,
                            const CurrentField& current, const BuoyancyParams& params,
                            const EffectSet& effects, BreathState& breath, float eyeHeight,
                            bool swimUp, bool sinkDown);

}
