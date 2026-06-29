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

// Instant effects resolve on the first tick after application and never linger.
bool isInstantEffect(EffectKind kind);
bool isHarmfulEffect(EffectKind kind);

// Fermented spider eye corruption. Returns EffectKind::Count for effects that
// have no counterpart.
EffectKind invertedEffect(EffectKind kind);

struct EffectInstance {
    EffectKind kind = EffectKind::Speed;
    int amplifier = 0;
    int durationTicks = 0;
    // Scales instant heal/damage magnitude; splash potions land at less than
    // one on anything not standing on the impact point.
    float potency = 1.0f;
    bool ambient = false;

    int level() const { return amplifier + 1; }
    bool expired() const { return durationTicks <= 0; }
};

EffectInstance makeEffect(EffectKind kind, int level, int durationTicks);

// Ticks between two firings of a periodic effect. Higher levels fire faster
// until the interval bottoms out at a single tick.
int periodicIntervalTicks(EffectKind kind, int amplifier);

// Attribute deltas contributed by a whole EffectSet, resolved once per tick
// instead of being queried effect by effect.
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
    // Health and food work produced by one tick, kept separate per source so a
    // caller can honour the rule that poison stops at half a heart while wither
    // does not.
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

// Applies an effect using the vanilla precedence: a stronger amplifier always
// replaces, an equal amplifier replaces only when it lasts longer, a weaker one
// is dropped. Instant effects stack as separate entries and are consumed by the
// next tickEffects call. Returns true when the set actually changed.
bool applyEffect(EffectSet& set, const EffectInstance& effect);

bool removeEffect(EffectSet& set, EffectKind kind);
void clearEffects(EffectSet& set);

EffectModifier effectModifiers(const EffectSet& set);

// Advances every effect by one tick, dropping expired ones and reporting the
// periodic and instant work owed to the entity this tick.
EffectSet::Pending tickEffects(EffectSet& set);

// Applies one tick of pending health work. Poison never takes the last half
// heart; wither and instant damage can kill. Returns the signed health change.
float applyEffectTick(const EffectSet::Pending& pending, float& health, float maxHealth);

} // namespace vf
