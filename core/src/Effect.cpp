#include "vf/Effect.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

int shiftedInterval(int base, int amplifier) {
    int interval = base >> std::min(amplifier, 30);
    return interval < 1 ? 1 : interval;
}

float clampZero(float value) { return value < 0.0f ? 0.0f : value; }

} // namespace

bool isInstantEffect(EffectKind kind) {
    return kind == EffectKind::InstantHealth || kind == EffectKind::InstantDamage;
}

bool isHarmfulEffect(EffectKind kind) {
    switch (kind) {
        case EffectKind::Slowness:
        case EffectKind::MiningFatigue:
        case EffectKind::InstantDamage:
        case EffectKind::Nausea:
        case EffectKind::Weakness:
        case EffectKind::Poison:
        case EffectKind::Wither:
        case EffectKind::Levitation:
            return true;
        default:
            return false;
    }
}

EffectKind invertedEffect(EffectKind kind) {
    switch (kind) {
        case EffectKind::Speed:          return EffectKind::Slowness;
        case EffectKind::Slowness:       return EffectKind::Speed;
        case EffectKind::InstantHealth:  return EffectKind::InstantDamage;
        case EffectKind::InstantDamage:  return EffectKind::InstantHealth;
        case EffectKind::Poison:         return EffectKind::InstantDamage;
        case EffectKind::JumpBoost:      return EffectKind::Slowness;
        case EffectKind::NightVision:    return EffectKind::Invisibility;
        case EffectKind::Invisibility:   return EffectKind::NightVision;
        case EffectKind::Strength:       return EffectKind::Weakness;
        case EffectKind::Weakness:       return EffectKind::Strength;
        default:                         return EffectKind::Count;
    }
}

EffectInstance makeEffect(EffectKind kind, int level, int durationTicks) {
    EffectInstance effect;
    effect.kind = kind;
    effect.amplifier = std::max(0, level - 1);
    effect.durationTicks = isInstantEffect(kind) ? 1 : std::max(0, durationTicks);
    return effect;
}

int periodicIntervalTicks(EffectKind kind, int amplifier) {
    switch (kind) {
        case EffectKind::Regeneration: return shiftedInterval(50, amplifier);
        case EffectKind::Poison:       return shiftedInterval(25, amplifier);
        case EffectKind::Wither:       return shiftedInterval(40, amplifier);
        case EffectKind::Saturation:   return 1;
        default:                       return 0;
    }
}

bool EffectSet::Pending::empty() const {
    return regenerationHeal <= 0.0f && instantHeal <= 0.0f && poisonDamage <= 0.0f &&
           witherDamage <= 0.0f && instantDamage <= 0.0f && saturationGain <= 0.0f &&
           expiredCount == 0;
}

const EffectInstance* findEffect(const EffectSet& set, EffectKind kind) {
    const EffectInstance* best = nullptr;
    for (const EffectInstance& effect : set.active) {
        if (effect.kind != kind) continue;
        if (!best || effect.amplifier > best->amplifier) best = &effect;
    }
    return best;
}

bool hasEffect(const EffectSet& set, EffectKind kind) { return findEffect(set, kind) != nullptr; }

int effectLevel(const EffectSet& set, EffectKind kind) {
    const EffectInstance* effect = findEffect(set, kind);
    return effect ? effect->level() : 0;
}

int effectDuration(const EffectSet& set, EffectKind kind) {
    const EffectInstance* effect = findEffect(set, kind);
    return effect ? effect->durationTicks : 0;
}

bool applyEffect(EffectSet& set, const EffectInstance& effect) {
    if (effect.durationTicks <= 0 && !isInstantEffect(effect.kind)) return false;
    if (isInstantEffect(effect.kind)) {
        EffectInstance instant = effect;
        instant.durationTicks = 1;
        set.active.push_back(instant);
        return true;
    }
    for (EffectInstance& existing : set.active) {
        if (existing.kind != effect.kind) continue;
        bool stronger = effect.amplifier > existing.amplifier;
        bool sameButLonger =
            effect.amplifier == existing.amplifier && effect.durationTicks > existing.durationTicks;
        if (!stronger && !sameButLonger) return false;
        existing = effect;
        return true;
    }
    set.active.push_back(effect);
    return true;
}

bool removeEffect(EffectSet& set, EffectKind kind) {
    std::size_t before = set.active.size();
    set.active.erase(std::remove_if(set.active.begin(), set.active.end(),
                                    [kind](const EffectInstance& e) { return e.kind == kind; }),
                     set.active.end());
    return set.active.size() != before;
}

void clearEffects(EffectSet& set) { set.active.clear(); }

EffectModifier effectModifiers(const EffectSet& set) {
    EffectModifier modifier;
    for (const EffectInstance& effect : set.active) {
        int level = effect.level();
        switch (effect.kind) {
            case EffectKind::Speed:
                modifier.moveSpeedMultiplier *= 1.0f + 0.20f * static_cast<float>(level);
                break;
            case EffectKind::Slowness:
                modifier.moveSpeedMultiplier *=
                    clampZero(1.0f - 0.15f * static_cast<float>(level));
                break;
            case EffectKind::Haste:
                modifier.miningSpeedMultiplier *= 1.0f + 0.20f * static_cast<float>(level);
                break;
            case EffectKind::MiningFatigue:
                modifier.miningSpeedMultiplier *= std::pow(0.3f, static_cast<float>(level));
                break;
            case EffectKind::Strength:
                modifier.attackDamageBonus += 3.0f * static_cast<float>(level);
                break;
            case EffectKind::Weakness:
                modifier.attackDamageBonus -= 4.0f * static_cast<float>(level);
                break;
            case EffectKind::Resistance:
                modifier.incomingDamageMultiplier *=
                    clampZero(1.0f - 0.20f * static_cast<float>(level));
                break;
            case EffectKind::JumpBoost:
                modifier.jumpVelocityBonus += 0.10f * static_cast<float>(level);
                break;
            case EffectKind::HealthBoost:
                modifier.maxHealthBonus += 4.0f * static_cast<float>(level);
                break;
            case EffectKind::Absorption:
                modifier.absorptionHearts += 4.0f * static_cast<float>(level);
                break;
            case EffectKind::Levitation:
                modifier.levitationSpeed += 0.9f * static_cast<float>(level);
                break;
            case EffectKind::Nausea:
                modifier.nauseaIntensity += static_cast<float>(level);
                break;
            case EffectKind::FireResistance: modifier.fireImmune = true; break;
            case EffectKind::WaterBreathing: modifier.waterBreathing = true; break;
            case EffectKind::Invisibility:   modifier.invisible = true; break;
            case EffectKind::NightVision:    modifier.nightVision = true; break;
            default: break;
        }
    }
    return modifier;
}

EffectSet::Pending tickEffects(EffectSet& set) {
    EffectSet::Pending pending;
    for (EffectInstance& effect : set.active) {
        if (effect.durationTicks <= 0) continue;
        switch (effect.kind) {
            case EffectKind::InstantHealth:
                pending.instantHeal +=
                    static_cast<float>(4 << std::min(effect.amplifier, 8)) * effect.potency;
                break;
            case EffectKind::InstantDamage:
                pending.instantDamage +=
                    static_cast<float>(6 << std::min(effect.amplifier, 8)) * effect.potency;
                break;
            default: break;
        }
        int interval = periodicIntervalTicks(effect.kind, effect.amplifier);
        if (interval > 0 && effect.durationTicks % interval == 0) {
            switch (effect.kind) {
                case EffectKind::Regeneration: pending.regenerationHeal += 1.0f; break;
                case EffectKind::Poison:       pending.poisonDamage += 1.0f; break;
                case EffectKind::Wither:       pending.witherDamage += 1.0f; break;
                case EffectKind::Saturation:
                    pending.saturationGain += static_cast<float>(effect.level());
                    break;
                default: break;
            }
        }
        --effect.durationTicks;
    }
    std::size_t before = set.active.size();
    set.active.erase(std::remove_if(set.active.begin(), set.active.end(),
                                    [](const EffectInstance& e) { return e.durationTicks <= 0; }),
                     set.active.end());
    pending.expiredCount = static_cast<int>(before - set.active.size());
    return pending;
}

float applyEffectTick(const EffectSet::Pending& pending, float& health, float maxHealth) {
    float before = health;
    float heal = pending.regenerationHeal + pending.instantHeal;
    if (heal > 0.0f) health = std::min(maxHealth, health + heal);
    if (pending.poisonDamage > 0.0f) {
        float survivable = clampZero(health - 1.0f);
        health -= std::min(pending.poisonDamage, survivable);
    }
    float lethal = pending.witherDamage + pending.instantDamage;
    if (lethal > 0.0f) health = clampZero(health - lethal);
    return health - before;
}

} // namespace vf
