#include "vf/Survival.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vf {

namespace {

constexpr float breathRecoverRate = 2.0f;

} // namespace

SurvivalState::SurvivalState(const SurvivalConfig& config)
    : health(config.maxHealth), hunger(config.maxHunger), saturation(config.maxSaturation * 0.25f),
      airRemaining(config.breathHoldSeconds) {}

float exhaustionCost(ExertionAction action) {
    switch (action) {
        case ExertionAction::Walk:       return 0.01f;
        case ExertionAction::Sprint:     return 0.1f;
        case ExertionAction::Jump:       return 0.05f;
        case ExertionAction::SprintJump: return 0.2f;
        case ExertionAction::Swim:       return 0.01f;
        case ExertionAction::MineBlock:  return 0.005f;
        case ExertionAction::AttackHit:  return 0.1f;
    }
    return 0.0f;
}

void addExhaustion(SurvivalState& state, const SurvivalConfig&, float amount) {
    if (amount > 0.0f) state.exhaustion += amount;
}

void applyExertion(SurvivalState& state, const SurvivalConfig& config, ExertionAction action,
                   float units) {
    addExhaustion(state, config, exhaustionCost(action) * units);
}

bool applyDamage(SurvivalState& state, DamageSource, float amount) {
    if (!state.alive || amount <= 0.0f) return false;
    state.health -= amount;
    if (state.health <= 0.0f) {
        state.health = 0.0f;
        state.alive = false;
        return true;
    }
    return false;
}

void tickHungerAndRegen(SurvivalState& state, const SurvivalConfig& config, float dt) {
    if (!state.alive) return;

    while (state.exhaustion >= config.exhaustionThreshold) {
        state.exhaustion -= config.exhaustionThreshold;
        if (state.saturation > 0.0f) {
            state.saturation = std::max(0.0f, state.saturation - 1.0f);
        } else {
            state.hunger = std::max(0.0f, state.hunger - 1.0f);
        }
    }

    if (state.hunger >= static_cast<float>(config.regenHungerThreshold) && state.health < config.maxHealth) {
        state.regenTimer += dt;
        while (state.regenTimer >= config.regenIntervalSeconds && state.health < config.maxHealth) {
            state.regenTimer -= config.regenIntervalSeconds;
            state.health = std::min(config.maxHealth, state.health + 1.0f);
            addExhaustion(state, config, config.exhaustionPerRegenHeart);
        }
    } else {
        state.regenTimer = 0.0f;
    }

    if (state.hunger <= 0.0f) {
        state.starveTimer += dt;
        while (state.starveTimer >= config.starveIntervalSeconds) {
            state.starveTimer -= config.starveIntervalSeconds;
            applyDamage(state, DamageSource::Starvation, config.starveDamagePerTick);
            if (!state.alive) break;
        }
    } else {
        state.starveTimer = 0.0f;
    }
}

void eat(SurvivalState& state, const SurvivalConfig& config, ItemId food) {
    if (!isFood(food)) return;
    FoodValue value = foodValue(food);
    state.hunger = std::min(config.maxHunger, state.hunger + static_cast<float>(value.hunger));
    state.saturation = std::min({config.maxSaturation, state.hunger, state.saturation + value.saturation});
}

float applyFallDamage(SurvivalState& state, const SurvivalConfig& config, float fallDistance) {
    if (fallDistance <= config.safeFallDistance) return 0.0f;
    float damage = (fallDistance - config.safeFallDistance) * config.fallDamagePerBlock;
    applyDamage(state, DamageSource::Fall, damage);
    return damage;
}

void tickBreath(SurvivalState& state, const SurvivalConfig& config, float dt, bool isUnderwater) {
    if (!state.alive) return;

    if (!isUnderwater) {
        state.airRemaining = std::min(config.breathHoldSeconds, state.airRemaining + dt * breathRecoverRate);
        state.drownTimer = 0.0f;
        return;
    }

    state.airRemaining = std::max(0.0f, state.airRemaining - dt);
    if (state.airRemaining > 0.0f) return;

    state.drownTimer += dt;
    while (state.drownTimer >= config.drownIntervalSeconds) {
        state.drownTimer -= config.drownIntervalSeconds;
        applyDamage(state, DamageSource::Drowning, config.drownDamagePerTick);
        if (!state.alive) break;
    }
}

float blockHardness(Block block) {
    constexpr float unbreakable = std::numeric_limits<float>::infinity();
    switch (block) {
        case Block::Air:            return 0.0f;
        case Block::Water:
        case Block::Lava:
        case Block::Bedrock:        return unbreakable;

        case Block::Leaves:         return 0.2f;
        case Block::RedstoneWire:
        case Block::RedstoneTorch:
        case Block::Torch:
        case Block::Lever:
        case Block::Button:
        case Block::Repeater:
        case Block::Comparator:
        case Block::Tnt:            return 0.0f;

        case Block::Dirt:
        case Block::Grass:
        case Block::Sand:
        case Block::Gravel:
        case Block::Snow:
        case Block::Ice:            return 0.5f;

        case Block::Sandstone:      return 0.8f;
        case Block::Ladder:         return 0.4f;
        case Block::Glowstone:      return 0.3f;

        case Block::Wood:           return 2.0f;
        case Block::Planks:         return 2.0f;
        case Block::CraftingTable:  return 2.5f;
        case Block::Chest:          return 2.5f;

        case Block::Stone:          return 1.5f;
        case Block::Cobblestone:    return 2.0f;
        case Block::Furnace:
        case Block::FurnaceLit:     return 3.5f;

        case Block::CoalOre:
        case Block::IronOre:
        case Block::GoldOre:
        case Block::DiamondOre:
        case Block::RedstoneOre:
        case Block::EmeraldOre:
        case Block::LapisOre:       return 3.0f;

        case Block::Obsidian:       return 50.0f;
        case Block::RedstoneBlock:  return 5.0f;
        case Block::RedstoneLamp:
        case Block::RedstoneLampLit: return 0.3f;
        case Block::Piston:
        case Block::PistonSticky:
        case Block::PistonHead:     return 1.5f;

        default:                    return 1.5f;
    }
}

float miningSeconds(Block block, ItemId tool) {
    float hardness = blockHardness(block);
    if (!std::isfinite(hardness)) return hardness;
    float multiplier = miningSpeedMultiplier(tool, block);
    if (multiplier <= 0.0f) multiplier = 1.0f;
    return hardness / multiplier;
}

} // namespace vf
