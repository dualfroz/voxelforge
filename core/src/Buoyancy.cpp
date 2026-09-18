#include "vf/Buoyancy.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

constexpr float fullySubmerged = 0.999f;

struct FluidCell {
    Block fluid = Block::Air;
    float height = 0.0f;
};

struct FluidForces {
    Vec3 velocity;
    Vec3 buoyantAcceleration;
    Vec3 dragDelta;
};

int firstCell(float value) { return static_cast<int>(std::floor(value)); }
int lastCell(float value) { return static_cast<int>(std::ceil(value)) - 1; }

bool insideRegion(const FluidSim& sim, int gx, int gy, int gz) {
    const Vec3i& origin = sim.regionOrigin();
    const Vec3i& dims = sim.regionDims();
    return gx >= origin.x && gx < origin.x + dims.x && gy >= origin.y && gy < origin.y + dims.y &&
           gz >= origin.z && gz < origin.z + dims.z;
}

FluidCell fluidCellAt(const World& world, const FluidSim* sim, int gx, int gy, int gz) {
    Block block = world.blockAt(gx, gy, gz);
    if (block == Block::Lava) return {Block::Lava, 1.0f};
    if (sim && insideRegion(*sim, gx, gy, gz)) {
        uint8_t level = sim->level(gx, gy, gz);
        if (level == 0) return {};
        bool covered = sim->level(gx, gy + 1, gz) > 0;
        return {Block::Water, covered ? 1.0f : static_cast<float>(level) / fluidMax};
    }
    if (block == Block::Water) return {Block::Water, 1.0f};
    return {};
}

float surfaceAbove(const World& world, const FluidSim* sim, int gx, int gy, int gz, Block fluid) {
    FluidCell cell = fluidCellAt(world, sim, gx, gy, gz);
    for (;;) {
        FluidCell above = fluidCellAt(world, sim, gx, gy + 1, gz);
        if (above.fluid != fluid) break;
        cell = above;
        ++gy;
    }
    return static_cast<float>(gy) + cell.height;
}

bool pointInFluid(const World& world, const FluidSim* sim, float x, float y, float z, Block fluid) {
    int gy = firstCell(y);
    FluidCell cell = fluidCellAt(world, sim, firstCell(x), gy, firstCell(z));
    return cell.fluid == fluid && y < static_cast<float>(gy) + cell.height;
}

FluidSample sampleWith(const World& world, const FluidSim* sim, const SweptBox& box,
                       float eyeHeight) {
    float waterVolume = 0.0f;
    float lavaVolume = 0.0f;
    float waterSurface = box.min.y;
    float lavaSurface = box.min.y;

    for (int x = firstCell(box.min.x); x <= lastCell(box.max.x); ++x)
        for (int y = firstCell(box.min.y); y <= lastCell(box.max.y); ++y)
            for (int z = firstCell(box.min.z); z <= lastCell(box.max.z); ++z) {
                FluidCell cell = fluidCellAt(world, sim, x, y, z);
                if (cell.fluid == Block::Air) continue;
                float spanX = std::min(box.max.x, x + 1.0f) - std::max(box.min.x, static_cast<float>(x));
                float spanY = std::min(box.max.y, y + cell.height) - std::max(box.min.y, static_cast<float>(y));
                float spanZ = std::min(box.max.z, z + 1.0f) - std::max(box.min.z, static_cast<float>(z));
                if (spanX <= 0.0f || spanY <= 0.0f || spanZ <= 0.0f) continue;
                float volume = spanX * spanY * spanZ;
                float surface = surfaceAbove(world, sim, x, y, z, cell.fluid);
                if (cell.fluid == Block::Lava) {
                    lavaVolume += volume;
                    lavaSurface = std::max(lavaSurface, surface);
                } else {
                    waterVolume += volume;
                    waterSurface = std::max(waterSurface, surface);
                }
            }

    FluidSample sample;
    if (lavaVolume <= 0.0f && waterVolume <= 0.0f) return sample;

    bool lavaWins = lavaVolume >= waterVolume;
    sample.fluid = lavaWins ? Block::Lava : Block::Water;
    sample.submergedVolume = lavaWins ? lavaVolume : waterVolume;
    sample.surfaceY = lavaWins ? lavaSurface : waterSurface;
    sample.submergedFraction = std::min(1.0f, sample.submergedVolume / box.volume());

    Vec3 centre = box.centre();
    float eyeY = box.min.y + eyeHeight;
    sample.feetInFluid = pointInFluid(world, sim, centre.x, box.min.y, centre.z, sample.fluid);
    sample.eyesInFluid = pointInFluid(world, sim, centre.x, eyeY, centre.z, sample.fluid);
    if (sample.eyesInFluid) {
        float surface = surfaceAbove(world, sim, firstCell(centre.x), firstCell(eyeY),
                                     firstCell(centre.z), sample.fluid);
        sample.eyeDepth = std::max(0.0f, surface - eyeY);
    }
    return sample;
}

FluidForces applyFluidForces(const FluidSample& sample, const Vec3& velocity,
                             const BuoyancyParams& params, bool swimUp, bool sinkDown) {
    FluidForces out;
    Vec3 moving = velocity;
    if (!sample.inFluid()) {
        moving.y -= params.gravity;
        out.velocity = moving;
        return out;
    }

    float fraction = sample.submergedFraction;
    float lift = params.gravity * fluidDensity(sample.fluid, params) * fraction;
    out.buoyantAcceleration = {0.0f, lift, 0.0f};
    moving.y += lift - params.gravity;
    if (swimUp) moving.y += params.swimUpAccel;
    if (sinkDown) moving.y -= params.sinkAccel;

    Vec3 beforeDrag = moving;
    float retained = 1.0f - (1.0f - fluidLinearDrag(sample.fluid, params)) * fraction;
    moving = moving * retained;
    float speed = length(moving);
    moving = moving * (1.0f / (1.0f + params.quadraticDrag * speed * fraction));
    out.dragDelta = moving - beforeDrag;

    moving.y = std::min(moving.y, params.maxRiseSpeed);
    out.velocity = moving;
    return out;
}

}

bool isFluidBlock(Block block) { return block == Block::Water || block == Block::Lava; }

FluidSample sampleFluid(const World& world, const SweptBox& box, float eyeHeight) {
    return sampleWith(world, nullptr, box, eyeHeight);
}

FluidSample sampleFluid(const World& world, const FluidSim& fluid, const SweptBox& box,
                        float eyeHeight) {
    return sampleWith(world, &fluid, box, eyeHeight);
}

float fluidDensity(Block fluid, const BuoyancyParams& params) {
    switch (fluid) {
        case Block::Water: return params.waterDensity;
        case Block::Lava:  return params.lavaDensity;
        default:           return 0.0f;
    }
}

float fluidLinearDrag(Block fluid, const BuoyancyParams& params) {
    switch (fluid) {
        case Block::Water: return params.waterLinearDrag;
        case Block::Lava:  return params.lavaLinearDrag;
        default:           return 1.0f;
    }
}

CurrentField::CurrentField(const World& world, const FluidSim& fluid) { attach(world, fluid); }

void CurrentField::attach(const World& world, const FluidSim& fluid) {
    world_ = &world;
    fluid_ = &fluid;
}

Vec3 CurrentField::flowAt(int gx, int gy, int gz) const {
    if (!valid()) return {};
    int here = fluid_->level(gx, gy, gz);
    if (here == 0) return {};

    const int horizontal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    Vec3 flow;
    for (const auto& step : horizontal) {
        int nx = gx + step[0];
        int nz = gz + step[1];
        if (world_->solidAt(nx, gy, nz)) continue;
        float drop = static_cast<float>(here - fluid_->level(nx, gy, nz));
        flow.x += static_cast<float>(step[0]) * drop;
        flow.z += static_cast<float>(step[1]) * drop;
    }
    float magnitude = length(flow);
    if (magnitude <= 0.0f) return {};
    return flow * (1.0f / magnitude);
}

Vec3 CurrentField::flowAtBox(const SweptBox& box) const {
    if (!valid()) return {};
    Vec3 flow;
    for (int x = firstCell(box.min.x); x <= lastCell(box.max.x); ++x)
        for (int y = firstCell(box.min.y); y <= lastCell(box.max.y); ++y)
            for (int z = firstCell(box.min.z); z <= lastCell(box.max.z); ++z)
                flow += flowAt(x, y, z);
    float magnitude = length(flow);
    if (magnitude <= 0.0f) return {};
    return flow * (1.0f / magnitude);
}

Vec3 buoyantVelocity(const FluidSample& sample, const Vec3& velocity,
                     const BuoyancyParams& params, bool swimUp, bool sinkDown) {
    return applyFluidForces(sample, velocity, params, swimUp, sinkDown).velocity;
}

bool updateBreath(const FluidSample& sample, BreathState& breath, const BuoyancyParams& params,
                  const EffectSet& effects, const Vec3& position, DamageEvent& hit) {
    EffectModifier modifier = effectModifiers(effects);
    bool damaged = false;

    if (sample.inLava() && !modifier.fireImmune) {
        if (++breath.burnTimer >= params.lavaBurnIntervalTicks) {
            breath.burnTimer = 0;
            hit = DamageEvent{};
            hit.source = environmentSource(DamageType::Lava, position);
            hit.amount = params.lavaBurnDamage;
            damaged = true;
        }
    } else {
        breath.burnTimer = 0;
    }

    if (!sample.inWater() || !sample.eyesInFluid) {
        breath.airTicks = std::min(params.maxAirTicks, breath.airTicks + params.airRefillPerTick);
        breath.drownTimer = 0;
        return damaged;
    }
    if (modifier.waterBreathing) {
        breath.drownTimer = 0;
        return damaged;
    }
    if (breath.airTicks > 0) {
        --breath.airTicks;
        return damaged;
    }
    if (++breath.drownTimer >= params.drownIntervalTicks) {
        breath.drownTimer = 0;
        if (!damaged) {
            hit = DamageEvent{};
            hit.source = environmentSource(DamageType::Drowning, position);
            hit.amount = params.drownDamage;
        }
        damaged = true;
    }
    return damaged;
}

BuoyancyResult stepBuoyancy(const World& world, const SweptBox& box, const Vec3& velocity,
                            const CurrentField& current, const BuoyancyParams& params,
                            const EffectSet& effects, BreathState& breath, float eyeHeight,
                            bool swimUp, bool sinkDown) {
    BuoyancyResult out;
    out.sample = current.valid() ? sampleFluid(world, *current.fluid(), box, eyeHeight)
                                 : sampleFluid(world, box, eyeHeight);

    FluidForces forces = applyFluidForces(out.sample, velocity, params, swimUp, sinkDown);
    out.buoyantAcceleration = forces.buoyantAcceleration;
    out.dragDelta = forces.dragDelta;
    if (out.sample.inWater())
        out.currentPush = current.flowAtBox(box) * (params.currentStrength * out.sample.submergedFraction);
    out.velocity = forces.velocity + out.currentPush;

    out.submerged = out.sample.submergedFraction >= fullySubmerged;
    out.eyesUnder = out.sample.eyesInFluid;
    out.tookFluidDamage = updateBreath(out.sample, breath, params, effects, box.centre(), out.fluidHit);
    out.drowning = out.sample.inWater() && out.eyesUnder && breath.airTicks == 0 &&
                   !effectModifiers(effects).waterBreathing;
    return out;
}

}
