#pragma once

#include <cstdint>
#include "Block.h"
#include "Item.h"

namespace vf {

enum class DamageSource : uint8_t { Fall, Starvation, Drowning, MobAttack, Fire, Lava, Void, Generic };

struct SurvivalConfig {
    float maxHealth = 20.0f;
    float maxHunger = 20.0f;
    float maxSaturation = 20.0f;

    float regenIntervalSeconds = 0.5f;
    int regenHungerThreshold = 18;
    float exhaustionPerRegenHeart = 6.0f;
    float exhaustionThreshold = 4.0f;

    float starveIntervalSeconds = 4.0f;
    float starveDamagePerTick = 1.0f;

    float safeFallDistance = 3.0f;
    float fallDamagePerBlock = 1.0f;

    float breathHoldSeconds = 15.0f;
    float drownIntervalSeconds = 1.0f;
    float drownDamagePerTick = 2.0f;
};

struct SurvivalState {
    float health;
    float hunger;
    float saturation;
    float exhaustion = 0.0f;
    float regenTimer = 0.0f;
    float starveTimer = 0.0f;
    float airRemaining;
    float drownTimer = 0.0f;
    bool alive = true;

    explicit SurvivalState(const SurvivalConfig& config = {});
};

enum class ExertionAction : uint8_t { Walk, Sprint, Jump, SprintJump, Swim, MineBlock, AttackHit };

float exhaustionCost(ExertionAction action);

void addExhaustion(SurvivalState& state, const SurvivalConfig& config, float amount);
void applyExertion(SurvivalState& state, const SurvivalConfig& config, ExertionAction action,
                   float units = 1.0f);

void tickHungerAndRegen(SurvivalState& state, const SurvivalConfig& config, float dt);

bool applyDamage(SurvivalState& state, DamageSource source, float amount);

void eat(SurvivalState& state, const SurvivalConfig& config, ItemId food);

float applyFallDamage(SurvivalState& state, const SurvivalConfig& config, float fallDistance);

void tickBreath(SurvivalState& state, const SurvivalConfig& config, float dt, bool isUnderwater);

float blockHardness(Block block);

float miningSeconds(Block block, ItemId tool);

}
