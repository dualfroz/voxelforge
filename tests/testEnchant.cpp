#include <algorithm>

#include "testHarness.h"
#include "vf/Enchant.h"

using namespace vf;

namespace {

void surroundWithShelves(World& world, const Vec3i& table, int count) {
    int placed = 0;
    for (int dy = 0; dy <= 1 && placed < count; ++dy) {
        for (int dx = -2; dx <= 2 && placed < count; ++dx) {
            for (int dz = -2; dz <= 2 && placed < count; ++dz) {
                if (std::max(dx < 0 ? -dx : dx, dz < 0 ? -dz : dz) != 2) continue;
                world.setBlock(table.x + dx, table.y + dy, table.z + dz, Block::Bookshelf);
                ++placed;
            }
        }
    }
}

bool sameOffer(const EnchantOffer& a, const EnchantOffer& b) {
    if (a.requiredLevels != b.requiredLevels || a.lapisCost != b.lapisCost) return false;
    if (a.enchants.entries.size() != b.enchants.entries.size()) return false;
    for (std::size_t i = 0; i < a.enchants.entries.size(); ++i) {
        if (a.enchants.entries[i].kind != b.enchants.entries[i].kind) return false;
        if (a.enchants.entries[i].level != b.enchants.entries[i].level) return false;
    }
    return true;
}

EnchantSet setOf(EnchantKind kind, int level) {
    EnchantSet set;
    addEnchant(set, kind, level);
    return set;
}

}

VF_TEST(bookshelvesOnTheRingRaiseTheOfferedLevelsUpToTheCap) {
    World bare;
    CHECK_EQ(bookshelfCount(bare, {0, 64, 0}), 0);

    World lightlyStocked;
    surroundWithShelves(lightlyStocked, {0, 64, 0}, 6);
    CHECK_EQ(bookshelfCount(lightlyStocked, {0, 64, 0}), 6);

    World library;
    surroundWithShelves(library, {0, 64, 0}, 30);
    CHECK_EQ(bookshelfCount(library, {0, 64, 0}), maxBookshelfPower);
}

VF_TEST(shelvesWithSomethingSolidInBetweenDoNotCount) {
    World world;
    world.setBlock(2, 64, 0, Block::Bookshelf);
    CHECK_EQ(bookshelfCount(world, {0, 64, 0}), 1);

    world.setBlock(1, 64, 0, Block::Stone);
    CHECK_EQ(bookshelfCount(world, {0, 64, 0}), 0);

    world.setBlock(1, 64, 0, Block::Torch);
    CHECK_EQ(bookshelfCount(world, {0, 64, 0}), 1);
}

VF_TEST(offersAreDeterministicForTheSameSeedAndMoveOnAfterOneIsTaken) {
    EnchantTableState first;
    first.seed = 20250904u;
    first.bookshelves = 15;
    first.target = Enchantment::Target::Sword;
    first.enchantability = 10;
    refreshOffers(first);

    EnchantTableState second;
    second.seed = 20250904u;
    second.bookshelves = 15;
    second.target = Enchantment::Target::Sword;
    second.enchantability = 10;
    refreshOffers(second);

    for (int slot = 0; slot < 3; ++slot) {
        CHECK(sameOffer(first.offers[static_cast<std::size_t>(slot)],
                        second.offers[static_cast<std::size_t>(slot)]));
    }

    uint32_t seedBefore = first.seed;
    EnchantOffer granted = takeOffer(first, 2, 40, 3);
    CHECK(granted.valid());
    CHECK(first.seed != seedBefore);
    CHECK_EQ(granted.lapisCost, 3);
}

VF_TEST(topRowNeedsThirtyLevelsInAFullyShelvedLibrary) {
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        EnchantTableState table;
        table.seed = seed * 2654435761u + 1u;
        table.bookshelves = 15;
        table.target = Enchantment::Target::Sword;
        table.enchantability = 14;
        refreshOffers(table);
        CHECK_EQ(table.offers[2].requiredLevels, 30);
        CHECK_EQ(table.offers[2].lapisCost, 3);
        CHECK(table.offers[0].requiredLevels <= table.offers[1].requiredLevels);
        CHECK(table.offers[1].requiredLevels <= table.offers[2].requiredLevels);
    }
}

VF_TEST(anEmptyTableOffersFarLessThanAFullyShelvedOne) {
    int bareTotal = 0;
    int shelvedTotal = 0;
    for (uint32_t seed = 1; seed <= 40; ++seed) {
        EnchantTableState bare;
        bare.seed = seed * 40503u + 7u;
        bare.bookshelves = 0;
        bare.target = Enchantment::Target::Pickaxe;
        bare.enchantability = 10;
        refreshOffers(bare);

        EnchantTableState shelved = bare;
        shelved.bookshelves = 15;
        refreshOffers(shelved);

        for (int slot = 0; slot < 3; ++slot) {
            bareTotal += bare.offers[static_cast<std::size_t>(slot)].requiredLevels;
            shelvedTotal += shelved.offers[static_cast<std::size_t>(slot)].requiredLevels;
        }
    }
    CHECK(shelvedTotal > bareTotal * 2);
}

VF_TEST(takingAnOfferFailsWithoutEnoughLevelsOrLapis) {
    EnchantTableState table;
    table.seed = 987654321u;
    table.bookshelves = 15;
    table.target = Enchantment::Target::Chestplate;
    table.enchantability = 9;
    refreshOffers(table);

    int cost = table.offers[2].requiredLevels;
    CHECK(cost > 0);
    CHECK(!takeOffer(table, 2, cost - 1, 3).valid());
    CHECK(!takeOffer(table, 2, cost, 2).valid());
    CHECK(!takeOffer(table, 7, 100, 64).valid());
    CHECK(takeOffer(table, 2, cost, 3).valid());
}

VF_TEST(rolledEnchantmentsRespectTargetsCapsAndConflicts) {
    for (uint32_t seed = 1; seed <= 300; ++seed) {
        uint32_t rng = seed * 2246822519u + 3u;
        EnchantSet rolled = rollEnchantments(Enchantment::Target::Sword, 14, 30, rng);
        for (std::size_t i = 0; i < rolled.entries.size(); ++i) {
            const Enchantment& entry = rolled.entries[i];
            CHECK(entry.level >= 1);
            CHECK(entry.level <= maxEnchantLevel(entry.kind));
            CHECK(enchantApplies(entry.kind, Enchantment::Target::Sword));
            for (std::size_t j = i + 1; j < rolled.entries.size(); ++j) {
                CHECK(entry.kind != rolled.entries[j].kind);
                CHECK(!enchantsConflict(entry.kind, rolled.entries[j].kind));
            }
        }
    }
}

VF_TEST(pickaxeRollsNeverProduceSwordOrBowEnchantments) {
    bool sawDiggerEnchant = false;
    for (uint32_t seed = 1; seed <= 200; ++seed) {
        uint32_t rng = seed * 374761393u + 11u;
        EnchantSet rolled = rollEnchantments(Enchantment::Target::Pickaxe, 10, 30, rng);
        for (const Enchantment& entry : rolled.entries) {
            CHECK(entry.kind != EnchantKind::Sharpness);
            CHECK(entry.kind != EnchantKind::Looting);
            CHECK(entry.kind != EnchantKind::Power);
            CHECK(entry.kind != EnchantKind::Protection);
            if (entry.kind == EnchantKind::Efficiency || entry.kind == EnchantKind::Fortune ||
                entry.kind == EnchantKind::SilkTouch) {
                sawDiggerEnchant = true;
            }
        }
    }
    CHECK(sawDiggerEnchant);
}

VF_TEST(conflictingFamiliesRejectEachOtherOnTheSameItem) {
    CHECK(enchantsConflict(EnchantKind::Sharpness, EnchantKind::Smite));
    CHECK(enchantsConflict(EnchantKind::Smite, EnchantKind::BaneOfArthropods));
    CHECK(enchantsConflict(EnchantKind::SilkTouch, EnchantKind::Fortune));
    CHECK(enchantsConflict(EnchantKind::Protection, EnchantKind::BlastProtection));
    CHECK(enchantsConflict(EnchantKind::Infinity, EnchantKind::Mending));
    CHECK(!enchantsConflict(EnchantKind::Sharpness, EnchantKind::Knockback));
    CHECK(!enchantsConflict(EnchantKind::Sharpness, EnchantKind::Sharpness));

    EnchantSet sword;
    CHECK(addEnchant(sword, EnchantKind::Sharpness, 5));
    CHECK(!addEnchant(sword, EnchantKind::Smite, 3));
    CHECK(!addEnchant(sword, EnchantKind::Sharpness, 6));
    CHECK(!addEnchant(sword, EnchantKind::Sharpness, 4));
    CHECK(addEnchant(sword, EnchantKind::Knockback, 2));
    CHECK_EQ(sword.levelOf(EnchantKind::Sharpness), 5);
    CHECK_EQ(sword.levelOf(EnchantKind::Smite), 0);
    CHECK(sword.conflictsWith(EnchantKind::BaneOfArthropods));

    CHECK(removeEnchant(sword, EnchantKind::Sharpness));
    CHECK(addEnchant(sword, EnchantKind::Smite, 3));
}

VF_TEST(anvilCombiningTwoEqualLevelsStepsUpAndBumpsPriorWork) {
    EnchantSet target = setOf(EnchantKind::Sharpness, 3);
    EnchantSet sacrifice = setOf(EnchantKind::Sharpness, 3);

    EnchantOffer merged = anvilCombine(target, 0, sacrifice, 0);
    CHECK(merged.valid());
    CHECK_EQ(merged.enchants.levelOf(EnchantKind::Sharpness), 4);
    CHECK_EQ(merged.requiredLevels, 4);
    CHECK_EQ(merged.priorWork, 1);

    EnchantOffer capped = anvilCombine(setOf(EnchantKind::Sharpness, 5), 0,
                                       setOf(EnchantKind::Sharpness, 5), 0);
    CHECK_EQ(capped.enchants.levelOf(EnchantKind::Sharpness), 5);

    EnchantOffer upgraded = anvilCombine(setOf(EnchantKind::Sharpness, 1), 0,
                                         setOf(EnchantKind::Sharpness, 4), 0);
    CHECK_EQ(upgraded.enchants.levelOf(EnchantKind::Sharpness), 4);

    EnchantOffer downgraded = anvilCombine(setOf(EnchantKind::Sharpness, 4), 0,
                                           setOf(EnchantKind::Sharpness, 1), 0);
    CHECK_EQ(downgraded.enchants.levelOf(EnchantKind::Sharpness), 4);
}

VF_TEST(anvilChargesAPenaltyForConflictingEnchantsAndDropsThem) {
    EnchantSet sword = setOf(EnchantKind::Sharpness, 3);
    EnchantSet book = setOf(EnchantKind::Smite, 2);

    EnchantOffer merged = anvilCombine(sword, 0, book, 0);
    CHECK_EQ(merged.enchants.levelOf(EnchantKind::Smite), 0);
    CHECK_EQ(merged.enchants.levelOf(EnchantKind::Sharpness), 3);
    CHECK_EQ(merged.requiredLevels, 1);

    EnchantSet compatible = setOf(EnchantKind::Knockback, 2);
    EnchantOffer stacked = anvilCombine(sword, 0, compatible, 0);
    CHECK_EQ(stacked.enchants.levelOf(EnchantKind::Knockback), 2);
    CHECK_EQ(stacked.requiredLevels, anvilCostMultiplier(EnchantKind::Knockback) * 2);
}

VF_TEST(repeatedAnvilWorkEventuallyBecomesTooExpensive) {
    CHECK_EQ(anvilRepairCost(0), 0);
    CHECK_EQ(anvilRepairCost(1), 1);
    CHECK_EQ(anvilRepairCost(3), 7);
    CHECK_EQ(anvilRepairCost(6), 63);

    EnchantSet sword = setOf(EnchantKind::Unbreaking, 1);
    int priorWork = 0;
    int repairs = 0;
    for (; repairs < 12; ++repairs) {
        int held = sword.levelOf(EnchantKind::Unbreaking);
        EnchantOffer merged = anvilCombine(sword, priorWork, setOf(EnchantKind::Unbreaking, held), 0);
        if (merged.tooExpensive) break;
        sword = merged.enchants;
        priorWork = merged.priorWork;
    }
    CHECK(repairs > 2);
    CHECK(repairs < 8);
    CHECK(priorWork >= repairs);
    CHECK(anvilCombine(sword, priorWork, setOf(EnchantKind::Unbreaking, 1), 0).tooExpensive);
    CHECK_EQ(sword.levelOf(EnchantKind::Unbreaking), maxEnchantLevel(EnchantKind::Unbreaking));
}

VF_TEST(costBandsAndWeightsFollowTheRarityLadder) {
    CHECK_EQ(enchantMinCost(EnchantKind::Sharpness, 1), 1);
    CHECK_EQ(enchantMaxCost(EnchantKind::Sharpness, 1), 21);
    CHECK_EQ(enchantMinCost(EnchantKind::Sharpness, 5), 45);
    CHECK_EQ(enchantMinCost(EnchantKind::Mending, 1), 25);

    CHECK_EQ(enchantWeight(EnchantKind::Protection), 10);
    CHECK_EQ(enchantWeight(EnchantKind::SilkTouch), 1);
    CHECK_EQ(anvilCostMultiplier(EnchantKind::Sharpness), 1);
    CHECK_EQ(anvilCostMultiplier(EnchantKind::SilkTouch), 8);

    for (int k = 0; k < static_cast<int>(EnchantKind::Count); ++k) {
        EnchantKind kind = static_cast<EnchantKind>(k);
        for (int level = 1; level <= maxEnchantLevel(kind); ++level) {
            CHECK(enchantMaxCost(kind, level) > enchantMinCost(kind, level));
        }
    }
}

VF_TEST(toolEnchantabilityFollowsTheMaterialAndMapsToTheRightTarget) {
    CHECK_EQ(enchantabilityOf(ItemId::GoldSword), 22);
    CHECK_EQ(enchantabilityOf(ItemId::DiamondPickaxe), 10);
    CHECK_EQ(enchantabilityOf(ItemId::StoneAxe), 5);
    CHECK_EQ(enchantabilityOf(ItemId::Bread), 0);

    CHECK(enchantTargetOf(ItemId::IronSword) == Enchantment::Target::Sword);
    CHECK(enchantTargetOf(ItemId::IronPickaxe) == Enchantment::Target::Pickaxe);
    CHECK(enchantTargetOf(ItemId::IronShovel) == Enchantment::Target::Shovel);
    CHECK(enchantTargetOf(ItemId::Apple) == Enchantment::Target::Book);

    CHECK(enchantApplies(EnchantKind::FeatherFalling, Enchantment::Target::Boots));
    CHECK(!enchantApplies(EnchantKind::FeatherFalling, Enchantment::Target::Helmet));
    CHECK(enchantApplies(EnchantKind::Respiration, Enchantment::Target::Helmet));
    CHECK(enchantApplies(EnchantKind::Unbreaking, Enchantment::Target::Bow));
    CHECK(!enchantApplies(EnchantKind::Looting, Enchantment::Target::Axe));
    CHECK(enchantApplies(EnchantKind::Sharpness, Enchantment::Target::Axe));
}
