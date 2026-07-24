#include "vf/Enchant.h"

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

int rollBelow(uint32_t& rng, int bound) {
    if (bound <= 1) return 0;
    return static_cast<int>(xorshift(rng) % static_cast<uint32_t>(bound));
}

float unitFloat(uint32_t& rng) { return static_cast<float>(xorshift(rng)) / 4294967295.0f; }

bool isProtectionFamily(EnchantKind kind) {
    return kind == EnchantKind::Protection || kind == EnchantKind::FireProtection ||
           kind == EnchantKind::BlastProtection || kind == EnchantKind::ProjectileProtection;
}

bool isDamageFamily(EnchantKind kind) {
    return kind == EnchantKind::Sharpness || kind == EnchantKind::Smite ||
           kind == EnchantKind::BaneOfArthropods;
}

bool isArmourTarget(Enchantment::Target target) {
    return target == Enchantment::Target::Helmet || target == Enchantment::Target::Chestplate ||
           target == Enchantment::Target::Leggings || target == Enchantment::Target::Boots;
}

bool isDiggerTarget(Enchantment::Target target) {
    return target == Enchantment::Target::Pickaxe || target == Enchantment::Target::Shovel ||
           target == Enchantment::Target::Axe;
}

int levelSpan(int base, int step, int level) { return base + (level - 1) * step; }

} // namespace

int maxEnchantLevel(EnchantKind kind) {
    switch (kind) {
        case EnchantKind::Protection:
        case EnchantKind::FireProtection:
        case EnchantKind::BlastProtection:
        case EnchantKind::ProjectileProtection:
        case EnchantKind::FeatherFalling:      return 4;
        case EnchantKind::Respiration:
        case EnchantKind::Thorns:
        case EnchantKind::Looting:
        case EnchantKind::Unbreaking:
        case EnchantKind::Fortune:             return 3;
        case EnchantKind::AquaAffinity:
        case EnchantKind::SilkTouch:
        case EnchantKind::Flame:
        case EnchantKind::Infinity:
        case EnchantKind::Mending:             return 1;
        case EnchantKind::Knockback:
        case EnchantKind::FireAspect:
        case EnchantKind::Punch:               return 2;
        case EnchantKind::Sharpness:
        case EnchantKind::Smite:
        case EnchantKind::BaneOfArthropods:
        case EnchantKind::Efficiency:
        case EnchantKind::Power:               return 5;
        default:                               return 1;
    }
}

int enchantWeight(EnchantKind kind) {
    switch (kind) {
        case EnchantKind::Protection:
        case EnchantKind::Sharpness:
        case EnchantKind::Efficiency:
        case EnchantKind::Power:               return 10;
        case EnchantKind::FireProtection:
        case EnchantKind::ProjectileProtection:
        case EnchantKind::FeatherFalling:
        case EnchantKind::Smite:
        case EnchantKind::BaneOfArthropods:
        case EnchantKind::Knockback:
        case EnchantKind::Unbreaking:          return 5;
        case EnchantKind::BlastProtection:
        case EnchantKind::Respiration:
        case EnchantKind::AquaAffinity:
        case EnchantKind::FireAspect:
        case EnchantKind::Looting:
        case EnchantKind::Fortune:
        case EnchantKind::Punch:
        case EnchantKind::Flame:
        case EnchantKind::Mending:             return 2;
        default:                               return 1;
    }
}

int anvilCostMultiplier(EnchantKind kind) {
    switch (enchantWeight(kind)) {
        case 10: return 1;
        case 5:  return 2;
        case 2:  return 4;
        default: return 8;
    }
}

int enchantMinCost(EnchantKind kind, int level) {
    switch (kind) {
        case EnchantKind::Protection:           return levelSpan(1, 11, level);
        case EnchantKind::FireProtection:       return levelSpan(10, 8, level);
        case EnchantKind::BlastProtection:      return levelSpan(5, 8, level);
        case EnchantKind::ProjectileProtection: return levelSpan(3, 6, level);
        case EnchantKind::FeatherFalling:       return levelSpan(5, 6, level);
        case EnchantKind::Respiration:          return 10 * level;
        case EnchantKind::AquaAffinity:         return 1;
        case EnchantKind::Thorns:               return levelSpan(10, 20, level);
        case EnchantKind::Sharpness:            return levelSpan(1, 11, level);
        case EnchantKind::Smite:                return levelSpan(5, 8, level);
        case EnchantKind::BaneOfArthropods:     return levelSpan(5, 8, level);
        case EnchantKind::Knockback:            return levelSpan(5, 20, level);
        case EnchantKind::FireAspect:           return levelSpan(10, 20, level);
        case EnchantKind::Looting:              return levelSpan(15, 9, level);
        case EnchantKind::Efficiency:           return levelSpan(1, 10, level);
        case EnchantKind::SilkTouch:            return 15;
        case EnchantKind::Unbreaking:           return levelSpan(5, 8, level);
        case EnchantKind::Fortune:              return levelSpan(15, 9, level);
        case EnchantKind::Power:                return levelSpan(1, 10, level);
        case EnchantKind::Punch:                return levelSpan(12, 20, level);
        case EnchantKind::Flame:                return 20;
        case EnchantKind::Infinity:             return 20;
        case EnchantKind::Mending:              return 25 * level;
        default:                                return 1;
    }
}

int enchantMaxCost(EnchantKind kind, int level) {
    switch (kind) {
        case EnchantKind::Protection:           return enchantMinCost(kind, level) + 11;
        case EnchantKind::FireProtection:
        case EnchantKind::BlastProtection:      return enchantMinCost(kind, level) + 8;
        case EnchantKind::ProjectileProtection:
        case EnchantKind::FeatherFalling:       return enchantMinCost(kind, level) + 6;
        case EnchantKind::Respiration:          return enchantMinCost(kind, level) + 30;
        case EnchantKind::AquaAffinity:         return 41;
        case EnchantKind::Sharpness:
        case EnchantKind::Smite:
        case EnchantKind::BaneOfArthropods:     return enchantMinCost(kind, level) + 20;
        case EnchantKind::Power:                return enchantMinCost(kind, level) + 15;
        case EnchantKind::Punch:                return enchantMinCost(kind, level) + 25;
        default:                                return enchantMinCost(kind, level) + 50;
    }
}

bool enchantsConflict(EnchantKind a, EnchantKind b) {
    if (a == b) return false;
    if (isProtectionFamily(a) && isProtectionFamily(b)) return true;
    if (isDamageFamily(a) && isDamageFamily(b)) return true;
    if ((a == EnchantKind::SilkTouch && b == EnchantKind::Fortune) ||
        (a == EnchantKind::Fortune && b == EnchantKind::SilkTouch))
        return true;
    if ((a == EnchantKind::SilkTouch && b == EnchantKind::Looting) ||
        (a == EnchantKind::Looting && b == EnchantKind::SilkTouch))
        return true;
    if ((a == EnchantKind::Infinity && b == EnchantKind::Mending) ||
        (a == EnchantKind::Mending && b == EnchantKind::Infinity))
        return true;
    return false;
}

bool enchantApplies(EnchantKind kind, Enchantment::Target target) {
    if (target == Enchantment::Target::Book) return true;
    switch (kind) {
        case EnchantKind::Unbreaking:
        case EnchantKind::Mending:
            return true;
        case EnchantKind::Protection:
        case EnchantKind::FireProtection:
        case EnchantKind::BlastProtection:
        case EnchantKind::ProjectileProtection:
        case EnchantKind::Thorns:
            return isArmourTarget(target);
        case EnchantKind::FeatherFalling:
            return target == Enchantment::Target::Boots;
        case EnchantKind::Respiration:
        case EnchantKind::AquaAffinity:
            return target == Enchantment::Target::Helmet;
        case EnchantKind::Knockback:
        case EnchantKind::FireAspect:
        case EnchantKind::Looting:
            return target == Enchantment::Target::Sword;
        case EnchantKind::Sharpness:
        case EnchantKind::Smite:
        case EnchantKind::BaneOfArthropods:
            return target == Enchantment::Target::Sword || target == Enchantment::Target::Axe;
        case EnchantKind::Efficiency:
        case EnchantKind::SilkTouch:
        case EnchantKind::Fortune:
            return isDiggerTarget(target);
        case EnchantKind::Power:
        case EnchantKind::Punch:
        case EnchantKind::Flame:
        case EnchantKind::Infinity:
            return target == Enchantment::Target::Bow;
        default:
            return false;
    }
}

int enchantabilityOf(ItemId item) {
    if (!isTool(item)) return 0;
    switch (itemInfo(item).toolTier) {
        case ToolTier::Wood:    return 15;
        case ToolTier::Stone:   return 5;
        case ToolTier::Iron:    return 14;
        case ToolTier::Gold:    return 22;
        case ToolTier::Diamond: return 10;
        default:                return 0;
    }
}

Enchantment::Target enchantTargetOf(ItemId item) {
    switch (itemInfo(item).toolType) {
        case ToolType::Sword:   return Enchantment::Target::Sword;
        case ToolType::Axe:     return Enchantment::Target::Axe;
        case ToolType::Pickaxe: return Enchantment::Target::Pickaxe;
        case ToolType::Shovel:  return Enchantment::Target::Shovel;
        default:                return Enchantment::Target::Book;
    }
}

int EnchantSet::levelOf(EnchantKind kind) const {
    for (const Enchantment& entry : entries) {
        if (entry.kind == kind) return entry.level;
    }
    return 0;
}

bool EnchantSet::conflictsWith(EnchantKind kind) const {
    for (const Enchantment& entry : entries) {
        if (enchantsConflict(entry.kind, kind)) return true;
    }
    return false;
}

bool addEnchant(EnchantSet& set, EnchantKind kind, int level) {
    if (level < 1 || level > maxEnchantLevel(kind)) return false;
    if (set.conflictsWith(kind)) return false;
    for (Enchantment& entry : set.entries) {
        if (entry.kind != kind) continue;
        if (level <= entry.level) return false;
        entry.level = level;
        return true;
    }
    set.entries.push_back({kind, level});
    return true;
}

bool removeEnchant(EnchantSet& set, EnchantKind kind) {
    std::size_t before = set.entries.size();
    set.entries.erase(std::remove_if(set.entries.begin(), set.entries.end(),
                                     [kind](const Enchantment& e) { return e.kind == kind; }),
                      set.entries.end());
    return set.entries.size() != before;
}

int bookshelfCount(const World& world, const Vec3i& tablePos) {
    int found = 0;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dz = -2; dz <= 2; ++dz) {
            if (std::max(std::abs(dx), std::abs(dz)) != 2) continue;
            int stepX = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
            int stepZ = dz > 0 ? 1 : (dz < 0 ? -1 : 0);
            for (int dy = 0; dy <= 1; ++dy) {
                Block shelf = world.blockAt(tablePos.x + dx, tablePos.y + dy, tablePos.z + dz);
                if (shelf != Block::Bookshelf) continue;
                Block blocker =
                    world.blockAt(tablePos.x + stepX, tablePos.y + dy, tablePos.z + stepZ);
                if (isOpaque(blocker)) continue;
                ++found;
            }
        }
    }
    return std::min(found, maxBookshelfPower);
}

EnchantSet rollEnchantments(Enchantment::Target target, int enchantability, int enchantLevel,
                            uint32_t& rng) {
    EnchantSet chosen;
    if (enchantLevel < 1) return chosen;

    int bonus = enchantability / 4 + 1;
    int modified = enchantLevel + rollBelow(rng, bonus) + rollBelow(rng, bonus) + 1;
    float spread = 1.0f + (unitFloat(rng) + unitFloat(rng) - 1.0f) * 0.15f;
    modified = std::max(1, static_cast<int>(std::lround(static_cast<float>(modified) * spread)));

    std::vector<Enchantment> candidates;
    for (int slot = 0; slot < 3; ++slot) {
        candidates.clear();
        int totalWeight = 0;
        for (int k = 0; k < static_cast<int>(EnchantKind::Count); ++k) {
            EnchantKind kind = static_cast<EnchantKind>(k);
            if (!enchantApplies(kind, target)) continue;
            if (chosen.levelOf(kind) > 0 || chosen.conflictsWith(kind)) continue;
            for (int level = maxEnchantLevel(kind); level >= 1; --level) {
                if (modified < enchantMinCost(kind, level)) continue;
                if (modified > enchantMaxCost(kind, level)) continue;
                candidates.push_back({kind, level});
                totalWeight += enchantWeight(kind);
                break;
            }
        }
        if (candidates.empty()) break;
        int pick = rollBelow(rng, totalWeight);
        Enchantment picked = candidates.back();
        for (const Enchantment& candidate : candidates) {
            pick -= enchantWeight(candidate.kind);
            if (pick < 0) {
                picked = candidate;
                break;
            }
        }
        chosen.entries.push_back(picked);
        if (slot == 2) break;
        modified = modified * 4 / 5;
        if (rollBelow(rng, 50) > modified) break;
    }
    return chosen;
}

void refreshOffers(EnchantTableState& state) {
    uint32_t rng = state.seed != 0 ? state.seed : 0x9e3779b9u;
    int shelves = std::min(std::max(state.bookshelves, 0), maxBookshelfPower);
    int base = 1 + rollBelow(rng, 8) + shelves / 2 + rollBelow(rng, shelves + 1);

    int slotLevels[3] = {
        std::max(base / 3, 1),
        (base * 2) / 3 + 1,
        std::max(base, shelves * 2),
    };

    for (int slot = 0; slot < 3; ++slot) {
        EnchantOffer offer;
        offer.lapisCost = slot + 1;
        offer.requiredLevels = slotLevels[slot];
        uint32_t slotRng = rng ^ (0x85ebca6bu * static_cast<uint32_t>(slot + 1));
        offer.enchants = rollEnchantments(state.target, state.enchantability, slotLevels[slot],
                                          slotRng);
        if (offer.enchants.empty()) offer.requiredLevels = 0;
        state.offers[static_cast<std::size_t>(slot)] = offer;
    }
}

EnchantOffer takeOffer(EnchantTableState& state, int slot, int playerLevels, int lapisCount) {
    if (slot < 0 || slot > 2) return {};
    const EnchantOffer& offer = state.offers[static_cast<std::size_t>(slot)];
    if (!offer.valid()) return {};
    if (playerLevels < offer.requiredLevels || lapisCount < offer.lapisCost) return {};

    EnchantOffer granted = offer;
    state.seed = state.seed * 1664525u + 1013904223u + static_cast<uint32_t>(slot);
    refreshOffers(state);
    return granted;
}

int anvilRepairCost(int priorWork) {
    int clamped = std::min(std::max(priorWork, 0), 30);
    return (1 << clamped) - 1;
}

EnchantOffer anvilCombine(const EnchantSet& target, int targetPriorWork, const EnchantSet& sacrifice,
                          int sacrificePriorWork) {
    EnchantOffer merged;
    merged.enchants = target;
    merged.priorWork = std::max(targetPriorWork, sacrificePriorWork) + 1;

    int cost = anvilRepairCost(targetPriorWork) + anvilRepairCost(sacrificePriorWork);
    for (const Enchantment& incoming : sacrifice.entries) {
        int existing = merged.enchants.levelOf(incoming.kind);
        if (existing == 0) {
            if (merged.enchants.conflictsWith(incoming.kind)) {
                cost += 1;
                continue;
            }
            merged.enchants.entries.push_back(incoming);
            cost += anvilCostMultiplier(incoming.kind) * incoming.level;
            continue;
        }
        int newLevel = existing;
        if (incoming.level > existing) {
            newLevel = incoming.level;
        } else if (incoming.level == existing) {
            newLevel = std::min(existing + 1, maxEnchantLevel(incoming.kind));
        }
        for (Enchantment& entry : merged.enchants.entries) {
            if (entry.kind == incoming.kind) entry.level = newLevel;
        }
        cost += anvilCostMultiplier(incoming.kind) * newLevel;
    }

    merged.requiredLevels = cost;
    merged.tooExpensive = cost >= anvilTooExpensiveCost;
    return merged;
}

} // namespace vf
