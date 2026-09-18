#pragma once

#include <cstdint>
#include <vector>

namespace vf {

constexpr int ticksPerSecond = 20;

enum class EffectKind : uint8_t {
    Speed, Slowness, Haste, MiningFatigue, Strength, InstantHealth, InstantDamage,
    JumpBoost, Nausea, Regeneration, Resistance, FireResistance, WaterBreathing,
    Invisibility, Weakness, Poison, Wither, HealthBoost, Absorption, Saturation,
    Levitation, NightVision,
    Count
};

bool isInstantEffect(EffectKind kind);
bool isHarmfulEffect(EffectKind kind);

EffectKind invertedEffect(EffectKind kind);

struct EffectInstance {
    EffectKind kind = EffectKind::Speed;
    int amplifier = 0;
    int durationTicks = 0;
    float potency = 1.0f;
    bool ambient = false;

    int level() const { return amplifier + 1; }
    bool expired() const { return durationTicks <= 0; }
};

EffectInstance makeEffect(EffectKind kind, int level, int durationTicks);

int periodicIntervalTicks(EffectKind kind, int amplifier);

struct EffectModifier {
    float moveSpeedMultiplier = 1.0f;
    float miningSpeedMultiplier = 1.0f;
    float attackDamageBonus = 0.0f;
    float incomingDamageMultiplier = 1.0f;
    float jumpVelocityBonus = 0.0f;
    float maxHealthBonus = 0.0f;
    float absorptionHearts = 0.0f;
    float levitationSpeed = 0.0f;
    float nauseaIntensity = 0.0f;
    bool fireImmune = false;
    bool waterBreathing = false;
    bool invisible = false;
    bool nightVision = false;
};

struct EffectSet {
    struct Pending {
        float regenerationHeal = 0.0f;
        float instantHeal = 0.0f;
        float poisonDamage = 0.0f;
        float witherDamage = 0.0f;
        float instantDamage = 0.0f;
        float saturationGain = 0.0f;
        int expiredCount = 0;

        bool empty() const;
    };

    std::vector<EffectInstance> active;
};

const EffectInstance* findEffect(const EffectSet& set, EffectKind kind);
bool hasEffect(const EffectSet& set, EffectKind kind);
int effectLevel(const EffectSet& set, EffectKind kind);
int effectDuration(const EffectSet& set, EffectKind kind);

bool applyEffect(EffectSet& set, const EffectInstance& effect);

bool removeEffect(EffectSet& set, EffectKind kind);
void clearEffects(EffectSet& set);

EffectModifier effectModifiers(const EffectSet& set);

EffectSet::Pending tickEffects(EffectSet& set);

float applyEffectTick(const EffectSet::Pending& pending, float& health, float maxHealth);

}
