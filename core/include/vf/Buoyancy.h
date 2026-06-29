#pragma once

#include <cstdint>

#include "Collide.h"
#include "Damage.h"
#include "Effect.h"
#include "Fluid.h"
#include "Math.h"
#include "World.h"

namespace vf {

// Being in a fluid, measured by volume rather than by a flag.
//
// The current pushing a body around is read straight out of Fluid.h's FluidSim -
// the levels its cellular automaton already settles - instead of a second flow
// model that would disagree with the water the world actually renders. Breath
// and drowning are turned into DamageEvents for Damage.h; nothing here computes
// health on its own.

bool isFluidBlock(Block block);

struct FluidSample {
    Block fluid = Block::Air;
    float submergedFraction = 0.0f;   // 0 dry, 1 fully under
    float submergedVolume = 0.0f;
    float surfaceY = 0.0f;            // top of the highest fluid column touching the box
    float eyeDepth = 0.0f;            // metres of fluid above the eyes, 0 when they are clear
    bool feetInFluid = false;
    bool eyesInFluid = false;

    bool inFluid() const { return fluid != Block::Air && submergedFraction > 0.0f; }
    bool inWater() const { return fluid == Block::Water; }
    bool inLava() const { return fluid == Block::Lava; }
};

// Every cell is a full column of fluid: the cheap sample, for worlds where water
// was stamped into blocks rather than simulated.
FluidSample sampleFluid(const World& world, const SweptBox& box, float eyeHeight);
// Column heights come from the automaton's levels, so a one-eighth flow edge
// only wets the soles of a body standing in it.
FluidSample sampleFluid(const World& world, const FluidSim& fluid, const SweptBox& box,
                        float eyeHeight);

struct BuoyancyParams {
    float gravity = 0.08f;
    // Below 1 the body still sinks under its own weight; above 1 it is pushed
    // back out. Lava is the denser of the two and holds a body up.
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
    int drownIntervalTicks = 20;
    float drownDamage = 2.0f;
    int lavaBurnIntervalTicks = 10;
    float lavaBurnDamage = 4.0f;
};

float fluidDensity(Block fluid, const BuoyancyParams& params);
float fluidLinearDrag(Block fluid, const BuoyancyParams& params);

// Air left in the lungs and the two countdowns that turn an empty bar into
// periodic damage.
struct BreathState {
    int airTicks = 300;
    int drownTimer = 0;
    int burnTimer = 0;
};

// Reads the settled fluid levels as a flow direction: water runs from the full
// cells toward the shallow ones and pours into any gap under them.
class CurrentField {
public:
    CurrentField() = default;
    CurrentField(const World& world, const FluidSim& fluid);

    void attach(const World& world, const FluidSim& fluid);
    bool valid() const { return world_ != nullptr && fluid_ != nullptr; }
    const FluidSim* fluid() const { return fluid_; }

    // Unit flow at one cell, zero where there is no fluid or nowhere to run.
    Vec3 flowAt(int gx, int gy, int gz) const;
    // Mean flow over every fluid cell the box occupies.
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

// Buoyancy, drag and deliberate swimming applied to a velocity that has already
// had gravity subtracted for this tick. Partial submersion scales all three, so
// half a body in water gets half the lift.
Vec3 buoyantVelocity(const FluidSample& sample, const Vec3& velocity,
                     const BuoyancyParams& params, bool swimUp, bool sinkDown);

// Advances the breath bar. Returns true when the tick owes damage, filling
// `hit` with a Damage.h event of the right type.
bool updateBreath(const FluidSample& sample, BreathState& breath, const BuoyancyParams& params,
                  const EffectSet& effects, const Vec3& position, DamageEvent& hit);

// One fluid tick for a body: sample, lift, drag, current and breath.
BuoyancyResult stepBuoyancy(const World& world, const SweptBox& box, const Vec3& velocity,
                            const CurrentField& current, const BuoyancyParams& params,
                            const EffectSet& effects, BreathState& breath, float eyeHeight,
                            bool swimUp, bool sinkDown);

} // namespace vf
