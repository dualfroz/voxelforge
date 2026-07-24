#include "vf/Damage.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float unitFloat(uint32_t& rng) { return static_cast<float>(xorshift(rng)) / 4294967295.0f; }

int rollBelow(uint32_t& rng, int bound) {
    if (bound <= 1) return 0;
    return static_cast<int>(xorshift(rng) % static_cast<uint32_t>(bound));
}

// Vanilla per-slot durability bases, scaled by the material factor below.
int slotDurabilityBase(Armor::Slot slot) {
    switch (slot) {
        case Armor::Slot::Helmet:     return 11;
        case Armor::Slot::Chestplate: return 16;
        case Armor::Slot::Leggings:   return 15;
        case Armor::Slot::Boots:      return 13;
        default:                      return 0;
    }
}

int materialDurabilityFactor(Armor::Material material) {
    switch (material) {
        case Armor::Material::Leather: return 5;
        case Armor::Material::Chain:   return 15;
        case Armor::Material::Iron:    return 15;
        case Armor::Material::Gold:    return 7;
        case Armor::Material::Diamond: return 33;
        default:                       return 0;
    }
}

int protectionPointsFor(const EnchantSet& enchants, DamageType type) {
    int points = 0;
    int general = enchants.levelOf(EnchantKind::Protection);
    if (general > 0 && type != DamageType::Void && type != DamageType::Starve) points += general;
    if (isFireDamage(type)) points += 2 * enchants.levelOf(EnchantKind::FireProtection);
    if (type == DamageType::Explosion) points += 2 * enchants.levelOf(EnchantKind::BlastProtection);
    if (isProjectileDamage(type)) {
        points += 2 * enchants.levelOf(EnchantKind::ProjectileProtection);
    }
    if (type == DamageType::Fall) points += 3 * enchants.levelOf(EnchantKind::FeatherFalling);
    return points;
}

bool unbreakingBlocksWear(int unbreakingLevel, uint32_t& rng) {
    if (unbreakingLevel <= 0) return false;
    float ignoreChance = 0.6f + 0.4f / static_cast<float>(unbreakingLevel + 1);
    return unitFloat(rng) < ignoreChance;
}

Vec3 knockbackAlong(const Vec3& heading, float strength) {
    Vec3 flat = normalize(Vec3{heading.x, 0.0f, heading.z});
    if (length(flat) <= 0.0f) flat = {1.0f, 0.0f, 0.0f};
    float horizontal = 0.4f + strength * 0.5f;
    return {flat.x * horizontal, 0.4f, flat.z * horizontal};
}

} // namespace

bool bypassesArmor(DamageType type) {
    switch (type) {
        case DamageType::Void:
        case DamageType::Starve:
        case DamageType::Suffocation:
        case DamageType::Magic:
            return true;
        default:
            return false;
    }
}

bool bypassesInvulnerability(DamageType type) {
    return type == DamageType::Void || type == DamageType::Starve;
}

bool bypassesMitigation(DamageType type) { return type == DamageType::Void; }

bool isFireDamage(DamageType type) {
    return type == DamageType::Fire || type == DamageType::Lava;
}

bool isProjectileDamage(DamageType type) { return type == DamageType::Projectile; }

HurtSource meleeSource(int attackerId, const Vec3& origin, bool byPlayer) {
    HurtSource source;
    source.type = DamageType::Melee;
    source.origin = origin;
    source.attackerId = attackerId;
    source.ownerId = attackerId;
    source.byPlayer = byPlayer;
    return source;
}

HurtSource projectileSource(int projectileId, int ownerId, const Vec3& origin, bool byPlayer) {
    HurtSource source;
    source.type = DamageType::Projectile;
    source.origin = origin;
    source.attackerId = projectileId;
    source.ownerId = ownerId;
    source.byPlayer = byPlayer;
    return source;
}

HurtSource environmentSource(DamageType type, const Vec3& origin) {
    HurtSource source;
    source.type = type;
    source.origin = origin;
    source.fiery = isFireDamage(type);
    return source;
}

Armor makeArmor(Armor::Material material, Armor::Slot slot) {
    Armor piece;
    piece.material = material;
    piece.slot = slot;
    piece.maxDurability = slotDurabilityBase(slot) * materialDurabilityFactor(material);
    piece.durability = piece.maxDurability;
    return piece;
}

int armorPoints(const Armor& piece) {
    if (!piece.worn()) return 0;
    switch (piece.material) {
        case Armor::Material::Leather:
            switch (piece.slot) {
                case Armor::Slot::Helmet:     return 1;
                case Armor::Slot::Chestplate: return 3;
                case Armor::Slot::Leggings:   return 2;
                default:                      return 1;
            }
        case Armor::Material::Chain:
            switch (piece.slot) {
                case Armor::Slot::Helmet:     return 2;
                case Armor::Slot::Chestplate: return 5;
                case Armor::Slot::Leggings:   return 4;
                default:                      return 1;
            }
        case Armor::Material::Iron:
            switch (piece.slot) {
                case Armor::Slot::Helmet:     return 2;
                case Armor::Slot::Chestplate: return 6;
                case Armor::Slot::Leggings:   return 5;
                default:                      return 2;
            }
        case Armor::Material::Gold:
            switch (piece.slot) {
                case Armor::Slot::Helmet:     return 2;
                case Armor::Slot::Chestplate: return 5;
                case Armor::Slot::Leggings:   return 3;
                default:                      return 1;
            }
        case Armor::Material::Diamond:
            switch (piece.slot) {
                case Armor::Slot::Helmet:     return 3;
                case Armor::Slot::Chestplate: return 8;
                case Armor::Slot::Leggings:   return 6;
                default:                      return 3;
            }
        default:
            return 0;
    }
}

float armorToughness(const Armor& piece) {
    if (!piece.worn()) return 0.0f;
    return piece.material == Armor::Material::Diamond ? 2.0f : 0.0f;
}

int armorEnchantability(Armor::Material material) {
    switch (material) {
        case Armor::Material::Leather: return 15;
        case Armor::Material::Chain:   return 12;
        case Armor::Material::Iron:    return 9;
        case Armor::Material::Gold:    return 25;
        case Armor::Material::Diamond: return 10;
        default:                       return 0;
    }
}

Enchantment::Target armorTarget(Armor::Slot slot) {
    switch (slot) {
        case Armor::Slot::Helmet:     return Enchantment::Target::Helmet;
        case Armor::Slot::Chestplate: return Enchantment::Target::Chestplate;
        case Armor::Slot::Leggings:   return Enchantment::Target::Leggings;
        default:                      return Enchantment::Target::Boots;
    }
}

int ArmorSet::totalPoints() const {
    int total = 0;
    for (const Armor& piece : pieces) total += armorPoints(piece);
    return total;
}

float ArmorSet::totalToughness() const {
    float total = 0.0f;
    for (const Armor& piece : pieces) total += armorToughness(piece);
    return total;
}

int ArmorSet::protectionPoints(DamageType type) const {
    int total = 0;
    for (const Armor& piece : pieces) {
        if (!piece.worn()) continue;
        total += protectionPointsFor(piece.enchants, type);
    }
    return total;
}

int ArmorSet::levelOf(EnchantKind kind) const {
    int best = 0;
    for (const Armor& piece : pieces) {
        if (!piece.worn()) continue;
        best = std::max(best, piece.enchants.levelOf(kind));
    }
    return best;
}

float armorReduction(int points, float toughness, float damage) {
    if (points <= 0) return damage;
    float pointsF = static_cast<float>(points);
    float defence = std::max(pointsF / 5.0f, pointsF - damage / (2.0f + toughness / 4.0f));
    defence = std::min(20.0f, defence);
    return damage * (1.0f - defence / 25.0f);
}

float protectionFactor(int points) {
    int capped = std::min(points, 20);
    if (capped <= 0) return 1.0f;
    return 1.0f - static_cast<float>(capped) * 0.04f;
}

float effectiveMaxHealth(const CombatStats& stats, const EffectSet& effects) {
    return stats.baseMaxHealth + effectModifiers(effects).maxHealthBonus;
}

int attackCooldownTicks(float attackSpeed) {
    if (attackSpeed <= 0.0f) return ticksPerSecond;
    int ticks = static_cast<int>(std::lround(static_cast<float>(ticksPerSecond) / attackSpeed));
    return std::max(1, ticks);
}

float attackCharge(const CombatStats& stats) {
    int cooldown = attackCooldownTicks(stats.attackSpeed);
    float charge = static_cast<float>(stats.attackChargeTicks) / static_cast<float>(cooldown);
    return std::min(1.0f, std::max(0.0f, charge));
}

void registerSwing(CombatStats& stats) { stats.attackChargeTicks = 0; }

EffectSet::Pending tickCombat(CombatStats& stats, EffectSet& effects) {
    if (stats.invulnerableTicks > 0) --stats.invulnerableTicks;
    if (stats.invulnerableTicks == 0) stats.lastDamageTaken = 0.0f;

    int cooldown = attackCooldownTicks(stats.attackSpeed);
    if (stats.attackChargeTicks < cooldown) ++stats.attackChargeTicks;

    EffectSet::Pending pending = tickEffects(effects);
    EffectModifier modifier = effectModifiers(effects);
    stats.absorption = std::max(stats.absorption, modifier.absorptionHearts);

    float maxHealth = stats.baseMaxHealth + modifier.maxHealthBonus;
    applyEffectTick(pending, stats.health, maxHealth);
    if (stats.health <= 0.0f) {
        stats.health = 0.0f;
        stats.alive = false;
    }
    return pending;
}

float meleeAttackDamage(const CombatStats& attacker, const EnchantSet& weapon,
                        const EffectSet& attackerEffects, bool undeadTarget, bool arthropodTarget,
                        bool critical) {
    float charge = attackCharge(attacker);
    float damage = attacker.baseAttackDamage * (0.2f + charge * charge * 0.8f);
    damage += effectModifiers(attackerEffects).attackDamageBonus;
    damage = std::max(0.0f, damage);

    float enchantBonus = 0.0f;
    int sharpness = weapon.levelOf(EnchantKind::Sharpness);
    if (sharpness > 0) enchantBonus += 1.0f + 0.5f * static_cast<float>(sharpness - 1);
    if (undeadTarget) enchantBonus += 2.5f * static_cast<float>(weapon.levelOf(EnchantKind::Smite));
    if (arthropodTarget) {
        enchantBonus += 2.5f * static_cast<float>(weapon.levelOf(EnchantKind::BaneOfArthropods));
    }
    // A partially charged swing only lands a fraction of the enchantment bonus.
    damage += enchantBonus * charge;

    if (critical && charge >= 0.9f) damage *= criticalMultiplier;
    return damage;
}

float fallDamage(float fallDistance, const EffectSet& effects) {
    float safeDistance = 3.0f + static_cast<float>(effectLevel(effects, EffectKind::JumpBoost));
    float excess = fallDistance - safeDistance;
    if (excess <= 0.0f) return 0.0f;
    return std::floor(excess);
}

Vec3 knockbackVector(const Vec3& attackerPos, const Vec3& targetPos, float strength) {
    return knockbackAlong(targetPos - attackerPos, strength);
}

int wearArmor(ArmorSet& armor, float incomingDamage, uint32_t& rng) {
    int wear = std::max(1, static_cast<int>(std::floor(incomingDamage / 4.0f)));
    int spent = 0;
    for (Armor& piece : armor.pieces) {
        if (!piece.worn()) continue;
        int unbreaking = piece.enchants.levelOf(EnchantKind::Unbreaking);
        int applied = 0;
        for (int i = 0; i < wear; ++i) {
            if (unbreakingBlocksWear(unbreaking, rng)) continue;
            ++applied;
        }
        applied = std::min(applied, piece.durability);
        piece.durability -= applied;
        spent += applied;
        if (piece.durability <= 0) piece = Armor{};
    }
    return spent;
}

CombatResult applyDamage(CombatStats& target, const EffectSet& targetEffects,
                         const DamageEvent& event, uint32_t& rng) {
    CombatResult result;
    result.incoming = event.amount;
    if (!target.alive || event.amount <= 0.0f) return result;

    DamageType type = event.source.type;
    EffectModifier modifier = effectModifiers(targetEffects);
    if (modifier.fireImmune && isFireDamage(type)) return result;

    float raw = event.amount;
    if (!bypassesInvulnerability(type)) {
        if (target.invulnerableTicks > 0) {
            if (raw <= target.lastDamageTaken) {
                result.blockedByInvulnerability = true;
                result.applied = false;
                return result;
            }
            raw -= target.lastDamageTaken;
            target.lastDamageTaken = event.amount;
        } else {
            target.lastDamageTaken = event.amount;
            target.invulnerableTicks = invulnerabilityTicks;
        }
    }

    float afterArmor = raw;
    if (!bypassesArmor(type)) {
        afterArmor = armorReduction(target.armor.totalPoints(), target.armor.totalToughness(), raw);
    }
    result.afterArmor = afterArmor;

    float afterResistance = afterArmor;
    float afterProtection = afterArmor;
    if (!bypassesMitigation(type)) {
        afterResistance = afterArmor * modifier.incomingDamageMultiplier;
        afterProtection = afterResistance * protectionFactor(target.armor.protectionPoints(type));
    }
    result.afterResistance = afterResistance;
    result.afterProtection = afterProtection;

    float remaining = afterProtection;
    if (target.absorption > 0.0f) {
        float absorbed = std::min(target.absorption, remaining);
        target.absorption -= absorbed;
        remaining -= absorbed;
        result.absorbedByAbsorption = absorbed;
    }

    float before = target.health;
    target.health = std::max(0.0f, target.health - remaining);
    result.healthLost = before - target.health;
    result.applied = true;
    result.fatal = target.health <= 0.0f;
    if (result.fatal) target.alive = false;

    if (!bypassesArmor(type) && afterProtection > 0.0f) {
        result.armorDurabilityLost = wearArmor(target.armor, raw, rng);
    }

    if (type == DamageType::Melee || type == DamageType::Projectile) {
        result.knockback = knockbackAlong(event.direction, event.knockbackStrength);
        int thorns = target.armor.levelOf(EnchantKind::Thorns);
        if (thorns > 0 && event.source.attackerId >= 0) {
            if (rollBelow(rng, 100) < thorns * 15) {
                result.thornsDamage = static_cast<float>(1 + rollBelow(rng, 4));
            }
        }
    }
    return result;
}

} // namespace vf
