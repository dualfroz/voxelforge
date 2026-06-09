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

// Exhaustion added per unit of `action` (per block moved, per jump, per hit).
float exhaustionCost(ExertionAction action);

void addExhaustion(SurvivalState& state, const SurvivalConfig& config, float amount);
void applyExertion(SurvivalState& state, const SurvivalConfig& config, ExertionAction action,
                   float units = 1.0f);

// One fixed tick: drains accumulated exhaustion into saturation/hunger,
// regenerates health while well fed, and starves the player when hunger is
// empty.
void tickHungerAndRegen(SurvivalState& state, const SurvivalConfig& config, float dt);

// Applies raw damage from `source`, clamping health at zero. Returns true if
// this hit killed the player.
bool applyDamage(SurvivalState& state, DamageSource source, float amount);

// Eats a food item: restores hunger/saturation up to their caps. No-op for
// non-food items.
void eat(SurvivalState& state, const SurvivalConfig& config, ItemId food);

// Damage for a landing after falling `fallDistance` blocks; 0 within the safe
// distance. Applies the damage to `state` and returns the amount applied.
float applyFallDamage(SurvivalState& state, const SurvivalConfig& config, float fallDistance);

// Call once per tick with the player's submersion state. Drains held breath
// while submerged and applies periodic damage once it runs out; recovers
// breath while not submerged.
void tickBreath(SurvivalState& state, const SurvivalConfig& config, float dt, bool isUnderwater);

// Seconds to break `block` bare-handed; Bedrock and fluids are unbreakable
// and return positive infinity.
float blockHardness(Block block);

// Seconds to break `block` with `tool` held (ItemId::None for bare hands),
// combining hardness with the tool's mining speed multiplier.
float miningSeconds(Block block, ItemId tool);

} // namespace vf
