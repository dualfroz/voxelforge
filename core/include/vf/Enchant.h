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
    // What a piece of gear counts as when deciding which enchantments may land
    // on it. Armour slots are separate targets because Respiration, Aqua
    // Affinity and Feather Falling are slot bound.
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

// Vanilla enchantability: how many extra cost points the material buys during
// a table roll. Non-tool items return 0.
int enchantabilityOf(ItemId item);
Enchantment::Target enchantTargetOf(ItemId item);

struct EnchantSet {
    std::vector<Enchantment> entries;

    int levelOf(EnchantKind kind) const;
    bool has(EnchantKind kind) const { return levelOf(kind) > 0; }
    bool conflictsWith(EnchantKind kind) const;
    bool empty() const { return entries.empty(); }
};

// Adds or upgrades an enchantment. Rejects levels above the cap, downgrades and
// anything conflicting with what is already there.
bool addEnchant(EnchantSet& set, EnchantKind kind, int level);
bool removeEnchant(EnchantSet& set, EnchantKind kind);

// One row of the enchanting table, and also the outcome of an anvil merge:
// both are "these enchantments for this many levels".
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

// Counts shelves on the ring two blocks out from the table, at table height and
// one above, ignoring any shelf whose line of sight to the table is blocked.
// Clamped to maxBookshelfPower.
int bookshelfCount(const World& world, const Vec3i& tablePos);

// Rolls a weighted set of enchantments for a target at a given effective level.
// Deterministic in `rng`.
EnchantSet rollEnchantments(Enchantment::Target target, int enchantability, int enchantLevel,
                            uint32_t& rng);

// Fills the three rows from the table seed and shelf count. Calling it twice on
// an unchanged state yields identical offers.
void refreshOffers(EnchantTableState& state);

// Consumes a row when the player can pay for it, reseeding the table so the
// next refresh differs. Returns an invalid offer when the row is unaffordable.
EnchantOffer takeOffer(EnchantTableState& state, int slot, int playerLevels, int lapisCount);

int anvilRepairCost(int priorWork);

// Merges `sacrifice` into `target` the way an anvil does: matching levels step
// up, conflicts cost a penalty level and are dropped, and the prior work
// penalty doubles the base price every time.
EnchantOffer anvilCombine(const EnchantSet& target, int targetPriorWork, const EnchantSet& sacrifice,
                          int sacrificePriorWork);

} // namespace vf
