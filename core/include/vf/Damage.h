#pragma once

#include <array>
#include <cstdint>
#include "Effect.h"
#include "Enchant.h"
#include "Math.h"

namespace vf {

enum class DamageType : uint8_t {
    Melee, Projectile, Fall, Fire, Lava, Drowning, Explosion, Magic, Void, Starve, Cactus,
    Suffocation,
    Count
};

bool bypassesArmor(DamageType type);
bool bypassesInvulnerability(DamageType type);
bool bypassesMitigation(DamageType type);
bool isFireDamage(DamageType type);
bool isProjectileDamage(DamageType type);

struct HurtSource {
    DamageType type = DamageType::Melee;
    Vec3 origin;
    int attackerId = -1;
    int ownerId = -1;
    bool byPlayer = false;
    bool fiery = false;
};

HurtSource meleeSource(int attackerId, const Vec3& origin, bool byPlayer);
HurtSource projectileSource(int projectileId, int ownerId, const Vec3& origin, bool byPlayer);
HurtSource environmentSource(DamageType type, const Vec3& origin = {});

struct DamageEvent {
    HurtSource source;
    float amount = 0.0f;
    bool critical = false;
    float knockbackStrength = 0.0f;
    Vec3 direction;
};

struct Armor {
    enum class Slot : uint8_t { Helmet, Chestplate, Leggings, Boots, Count };
    enum class Material : uint8_t { None, Leather, Chain, Iron, Gold, Diamond };

    Material material = Material::None;
    Slot slot = Slot::Chestplate;
    int durability = 0;
    int maxDurability = 0;
    EnchantSet enchants;

    bool worn() const { return material != Material::None && durability > 0; }
};

Armor makeArmor(Armor::Material material, Armor::Slot slot);
int armorPoints(const Armor& piece);
float armorToughness(const Armor& piece);
int armorEnchantability(Armor::Material material);
Enchantment::Target armorTarget(Armor::Slot slot);

struct ArmorSet {
    std::array<Armor, 4> pieces{};

    Armor& at(Armor::Slot slot) { return pieces[static_cast<std::size_t>(slot)]; }
    const Armor& at(Armor::Slot slot) const { return pieces[static_cast<std::size_t>(slot)]; }
    void equip(const Armor& piece) { at(piece.slot) = piece; }

    int totalPoints() const;
    float totalToughness() const;
    int protectionPoints(DamageType type) const;
    int levelOf(EnchantKind kind) const;
};

float armorReduction(int points, float toughness, float damage);

float protectionFactor(int protectionPoints);

struct CombatStats {
    float baseMaxHealth = 20.0f;
    float health = 20.0f;
    float absorption = 0.0f;
    float baseAttackDamage = 1.0f;
    float attackSpeed = 4.0f;
    int attackChargeTicks = 0;
    int invulnerableTicks = 0;
    float lastDamageTaken = 0.0f;
    ArmorSet armor;
    bool alive = true;
};

constexpr int invulnerabilityTicks = 10;
constexpr float criticalMultiplier = 1.5f;

float effectiveMaxHealth(const CombatStats& stats, const EffectSet& effects);
int attackCooldownTicks(float attackSpeed);
float attackCharge(const CombatStats& stats);
void registerSwing(CombatStats& stats);

EffectSet::Pending tickCombat(CombatStats& stats, EffectSet& effects);

struct CombatResult {
    bool applied = false;
    bool blockedByInvulnerability = false;
    float incoming = 0.0f;
    float afterArmor = 0.0f;
    float afterResistance = 0.0f;
    float afterProtection = 0.0f;
    float absorbedByAbsorption = 0.0f;
    float healthLost = 0.0f;
    float thornsDamage = 0.0f;
    int armorDurabilityLost = 0;
    bool fatal = false;
    Vec3 knockback;
};

float meleeAttackDamage(const CombatStats& attacker, const EnchantSet& weapon,
                        const EffectSet& attackerEffects, bool undeadTarget, bool arthropodTarget,
                        bool critical);

float fallDamage(float fallDistance, const EffectSet& effects);

Vec3 knockbackVector(const Vec3& attackerPos, const Vec3& targetPos, float strength);

int wearArmor(ArmorSet& armor, float incomingDamage, uint32_t& rng);

CombatResult applyDamage(CombatStats& target, const EffectSet& targetEffects,
                         const DamageEvent& event, uint32_t& rng);

}
