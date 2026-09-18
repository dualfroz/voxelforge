#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "Item.h"
#include "Math.h"
#include "World.h"

namespace vf {

enum class EnchantKind : uint8_t {
    Protection, FireProtection, BlastProtection, ProjectileProtection, FeatherFalling,
    Respiration, AquaAffinity, Thorns,
    Sharpness, Smite, BaneOfArthropods, Knockback, FireAspect, Looting,
    Efficiency, SilkTouch, Unbreaking, Fortune,
    Power, Punch, Flame, Infinity, Mending,
    Count
};

struct Enchantment {
    enum class Target : uint8_t {
        Sword, Axe, Pickaxe, Shovel, Bow, Helmet, Chestplate, Leggings, Boots, Book,
        Count
    };

    EnchantKind kind = EnchantKind::Protection;
    int level = 1;
};

int maxEnchantLevel(EnchantKind kind);
int enchantWeight(EnchantKind kind);
int anvilCostMultiplier(EnchantKind kind);
int enchantMinCost(EnchantKind kind, int level);
int enchantMaxCost(EnchantKind kind, int level);
bool enchantsConflict(EnchantKind a, EnchantKind b);
bool enchantApplies(EnchantKind kind, Enchantment::Target target);

int enchantabilityOf(ItemId item);
Enchantment::Target enchantTargetOf(ItemId item);

struct EnchantSet {
    std::vector<Enchantment> entries;

    int levelOf(EnchantKind kind) const;
    bool has(EnchantKind kind) const { return levelOf(kind) > 0; }
    bool conflictsWith(EnchantKind kind) const;
    bool empty() const { return entries.empty(); }
};

bool addEnchant(EnchantSet& set, EnchantKind kind, int level);
bool removeEnchant(EnchantSet& set, EnchantKind kind);

struct EnchantOffer {
    EnchantSet enchants;
    int requiredLevels = 0;
    int lapisCost = 0;
    int priorWork = 0;
    bool tooExpensive = false;

    bool valid() const { return !tooExpensive && requiredLevels > 0 && !enchants.empty(); }
};

struct EnchantTableState {
    uint32_t seed = 1;
    int bookshelves = 0;
    Enchantment::Target target = Enchantment::Target::Sword;
    int enchantability = 14;
    std::array<EnchantOffer, 3> offers{};
};

constexpr int maxBookshelfPower = 15;
constexpr int anvilTooExpensiveCost = 40;

int bookshelfCount(const World& world, const Vec3i& tablePos);

EnchantSet rollEnchantments(Enchantment::Target target, int enchantability, int enchantLevel,
                            uint32_t& rng);

void refreshOffers(EnchantTableState& state);

EnchantOffer takeOffer(EnchantTableState& state, int slot, int playerLevels, int lapisCount);

int anvilRepairCost(int priorWork);

EnchantOffer anvilCombine(const EnchantSet& target, int targetPriorWork, const EnchantSet& sacrifice,
                          int sacrificePriorWork);

}
