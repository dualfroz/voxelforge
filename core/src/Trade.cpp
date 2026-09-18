#include "vf/Trade.h"

#include <cmath>

namespace vf {

namespace {

uint32_t nextRandom(uint32_t& state) {
    state += 0x9e3779b9u;
    uint32_t z = state;
    z = (z ^ (z >> 16)) * 0x85ebca6bu;
    z = (z ^ (z >> 13)) * 0xc2b2ae35u;
    return z ^ (z >> 16);
}

int clampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

struct OfferSpec {
    uint8_t tier;
    ItemId inputA;
    int inputACount;
    ItemId inputB;
    int inputBCount;
    ItemId output;
    int outputCount;
    int maxUses;
    int experience;
};

const OfferSpec farmerSpecs[] = {
    {0, ItemId::Grass, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Bread, 6, 16, 1},
    {1, ItemId::Dirt, 26, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Apple, 4, 16, 5},
    {2, ItemId::Leaves, 32, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::CookedBeef, 4, 12, 10},
    {3, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::CookedPorkchop, 5, 12, 15},
    {4, ItemId::EmeraldGem, 6, ItemId::None, 0, ItemId::CookedMutton, 6, 12, 30},
};

const OfferSpec fishermanSpecs[] = {
    {0, ItemId::Stick, 32, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::CoalLump, 3, 16, 1},
    {1, ItemId::CoalLump, 15, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::CookedBeef, 3, 16, 5},
    {2, ItemId::Gravel, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::Ice, 4, 12, 10},
    {3, ItemId::EmeraldGem, 5, ItemId::None, 0, ItemId::IronIngot, 2, 12, 15},
    {4, ItemId::EmeraldGem, 8, ItemId::None, 0, ItemId::DiamondGem, 1, 3, 30},
};

const OfferSpec shepherdSpecs[] = {
    {0, ItemId::Leaves, 18, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Planks, 6, 16, 1},
    {1, ItemId::Snow, 16, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::Ladder, 3, 16, 5},
    {2, ItemId::LapisLazuli, 12, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::Torch, 8, 12, 10},
    {3, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::Glowstone, 2, 12, 15},
    {4, ItemId::EmeraldGem, 6, ItemId::None, 0, ItemId::RedstoneLamp, 1, 9, 30},
};

const OfferSpec fletcherSpecs[] = {
    {0, ItemId::Stick, 32, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Gunpowder, 2, 16, 1},
    {1, ItemId::Gravel, 10, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::Tnt, 1, 12, 5},
    {2, ItemId::Wood, 16, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::WoodSword, 1, 9, 10},
    {3, ItemId::EmeraldGem, 5, ItemId::None, 0, ItemId::StoneSword, 1, 9, 15},
    {4, ItemId::EmeraldGem, 8, ItemId::None, 0, ItemId::IronSword, 1, 6, 30},
};

const OfferSpec librarianSpecs[] = {
    {0, ItemId::Planks, 24, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::LapisLazuli, 4, 16, 1},
    {1, ItemId::Wood, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::RedstoneDust, 6, 16, 5},
    {2, ItemId::LapisLazuli, 9, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::Comparator, 1, 12, 10},
    {3, ItemId::EmeraldGem, 6, ItemId::None, 0, ItemId::Repeater, 2, 12, 15},
    {4, ItemId::EmeraldGem, 12, ItemId::None, 0, ItemId::DiamondGem, 1, 3, 30},
};

const OfferSpec cartographerSpecs[] = {
    {0, ItemId::Planks, 24, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Torch, 4, 16, 1},
    {1, ItemId::Sand, 22, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 5, ItemId::GoldIngot, 1, ItemId::Chest, 1, 12, 5},
    {2, ItemId::Sandstone, 16, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::Ladder, 6, 12, 10},
    {3, ItemId::EmeraldGem, 7, ItemId::None, 0, ItemId::Obsidian, 2, 9, 15},
    {4, ItemId::EmeraldGem, 10, ItemId::None, 0, ItemId::RedstoneBlock, 2, 6, 30},
};

const OfferSpec clericSpecs[] = {
    {0, ItemId::RottenFlesh, 32, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::RedstoneDust, 2, 16, 1},
    {1, ItemId::GoldIngot, 3, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::LapisLazuli, 1, 16, 5},
    {2, ItemId::CoalLump, 22, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::Glowstone, 1, 12, 10},
    {3, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::Gunpowder, 3, 12, 15},
    {4, ItemId::EmeraldGem, 8, ItemId::LapisLazuli, 4, ItemId::DiamondGem, 1, 3, 30},
};

const OfferSpec armorerSpecs[] = {
    {0, ItemId::CoalLump, 15, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::IronIngot, 1, 12, 1},
    {1, ItemId::IronIngot, 4, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::IronShovel, 1, 12, 5},
    {2, ItemId::LapisLazuli, 10, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 6, ItemId::None, 0, ItemId::IronAxe, 1, 9, 10},
    {3, ItemId::EmeraldGem, 9, ItemId::None, 0, ItemId::DiamondShovel, 1, 6, 15},
    {4, ItemId::EmeraldGem, 14, ItemId::DiamondGem, 1, ItemId::DiamondAxe, 1, 3, 30},
};

const OfferSpec weaponsmithSpecs[] = {
    {0, ItemId::CoalLump, 15, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::IronSword, 1, 12, 1},
    {1, ItemId::IronIngot, 4, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::StoneSword, 1, 12, 5},
    {2, ItemId::Gravel, 24, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 6, ItemId::None, 0, ItemId::IronHoe, 1, 9, 10},
    {3, ItemId::EmeraldGem, 10, ItemId::None, 0, ItemId::GoldSword, 1, 6, 15},
    {4, ItemId::EmeraldGem, 13, ItemId::DiamondGem, 1, ItemId::DiamondSword, 1, 3, 30},
};

const OfferSpec toolsmithSpecs[] = {
    {0, ItemId::CoalLump, 15, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::StoneAxe, 1, 12, 1},
    {1, ItemId::IronIngot, 4, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::IronPickaxe, 1, 12, 5},
    {2, ItemId::Stone, 26, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 5, ItemId::None, 0, ItemId::IronAxe, 1, 9, 10},
    {3, ItemId::EmeraldGem, 8, ItemId::None, 0, ItemId::GoldPickaxe, 1, 6, 15},
    {4, ItemId::EmeraldGem, 12, ItemId::DiamondGem, 1, ItemId::DiamondPickaxe, 1, 3, 30},
};

const OfferSpec butcherSpecs[] = {
    {0, ItemId::RawPorkchop, 7, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::CookedPorkchop, 4, 16, 1},
    {1, ItemId::RawBeef, 10, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::CookedBeef, 3, 16, 5},
    {2, ItemId::RawMutton, 10, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::CookedMutton, 3, 12, 10},
    {3, ItemId::CoalLump, 15, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 15},
    {4, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::CookedBeef, 8, 12, 30},
};

const OfferSpec leatherworkerSpecs[] = {
    {0, ItemId::Leaves, 26, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::Planks, 8, 16, 1},
    {1, ItemId::RottenFlesh, 26, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::Ladder, 4, 16, 5},
    {2, ItemId::Snow, 24, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 4, ItemId::None, 0, ItemId::Ice, 6, 12, 10},
    {3, ItemId::EmeraldGem, 5, ItemId::None, 0, ItemId::Chest, 2, 12, 15},
    {4, ItemId::EmeraldGem, 6, ItemId::IronIngot, 2, ItemId::Furnace, 2, 9, 30},
};

const OfferSpec masonSpecs[] = {
    {0, ItemId::Stone, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 2},
    {0, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Cobblestone, 10, 16, 1},
    {1, ItemId::Sand, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 16, 5},
    {1, ItemId::EmeraldGem, 1, ItemId::None, 0, ItemId::Sandstone, 6, 16, 5},
    {2, ItemId::Gravel, 20, ItemId::None, 0, ItemId::EmeraldGem, 1, 12, 10},
    {2, ItemId::EmeraldGem, 2, ItemId::None, 0, ItemId::Obsidian, 1, 12, 10},
    {3, ItemId::EmeraldGem, 3, ItemId::None, 0, ItemId::Glowstone, 2, 12, 15},
    {4, ItemId::EmeraldGem, 5, ItemId::None, 0, ItemId::Sandstone, 12, 9, 30},
};

const OfferSpec* specsForProfession(VillagerProfession profession, int& outCount) {
    switch (profession) {
        case VillagerProfession::Farmer: outCount = 8; return farmerSpecs;
        case VillagerProfession::Fisherman: outCount = 8; return fishermanSpecs;
        case VillagerProfession::Shepherd: outCount = 8; return shepherdSpecs;
        case VillagerProfession::Fletcher: outCount = 8; return fletcherSpecs;
        case VillagerProfession::Librarian: outCount = 8; return librarianSpecs;
        case VillagerProfession::Cartographer: outCount = 8; return cartographerSpecs;
        case VillagerProfession::Cleric: outCount = 8; return clericSpecs;
        case VillagerProfession::Armorer: outCount = 8; return armorerSpecs;
        case VillagerProfession::Weaponsmith: outCount = 8; return weaponsmithSpecs;
        case VillagerProfession::Toolsmith: outCount = 8; return toolsmithSpecs;
        case VillagerProfession::Butcher: outCount = 8; return butcherSpecs;
        case VillagerProfession::Leatherworker: outCount = 8; return leatherworkerSpecs;
        case VillagerProfession::Mason: outCount = 8; return masonSpecs;
        default: break;
    }
    outCount = 0;
    return nullptr;
}

int containerCount(const Container& container, ItemId item) {
    int total = 0;
    for (int i = 0; i < container.size(); ++i) {
        const ItemSlot& slot = container.slot(i);
        if (!slot.empty() && slot.item == item) total += slot.count;
    }
    return total;
}

bool containerHasRoom(const Container& container, ItemId item, int count) {
    int capacity = 0;
    int limit = maxStackSize(item);
    for (int i = 0; i < container.size(); ++i) {
        const ItemSlot& slot = container.slot(i);
        if (slot.empty()) capacity += limit;
        else if (slot.item == item && slot.count < limit) capacity += limit - slot.count;
        if (capacity >= count) return true;
    }
    return capacity >= count;
}

}

const char* tradeTierName(TradeTier tier) {
    switch (tier) {
        case TradeTier::Novice: return "novice";
        case TradeTier::Apprentice: return "apprentice";
        case TradeTier::Journeyman: return "journeyman";
        case TradeTier::Expert: return "expert";
        case TradeTier::Master: return "master";
        case TradeTier::Count: break;
    }
    return "unknown";
}

TradeTier tierForLevel(int level) {
    int clamped = clampInt(level, 1, villagerMaxLevel);
    return static_cast<TradeTier>(clamped - 1);
}

int levelForTier(TradeTier tier) { return static_cast<int>(tier) + 1; }

int PriceModifier::totalPercent() const {
    int percent = reputationPercent + heroLevel * heroDiscountPercentPerLevel -
                  hitCount * hitSurchargePercent;
    return clampInt(percent, -300, 90);
}

PriceModifier priceModifierFor(const ReputationView& reputation, int heroLevel, int hitCount) {
    PriceModifier modifier;
    modifier.reputationPercent = reputation.discountPercent();
    modifier.heroLevel = heroLevel < 0 ? 0 : heroLevel;
    modifier.hitCount = hitCount < 0 ? 0 : hitCount;
    return modifier;
}

int villagerHitCount(const GossipLedger& gossip) {
    return gossip.valueOf(GossipKind::MinorNegative) / gossipMinorNegativeGain;
}

int offerPrice(const TradeOffer& offer, const PriceModifier& modifier) {
    int base = offer.basePrice();
    int demandDelta = static_cast<int>(
        std::lround(static_cast<double>(offer.demand) * offer.priceMultiplier * base));
    if (demandDelta < 0) demandDelta = 0;
    int raw = base + demandDelta;
    int shift = static_cast<int>(
        std::lround(static_cast<double>(raw) * modifier.totalPercent() / 100.0));
    int adjusted = raw - shift;
    return adjusted < 1 ? 1 : adjusted;
}

bool TradeOfferList::unlocked(int index, int level) const {
    if (index < 0 || index >= size()) return false;
    return static_cast<int>(offers[index].tier) < clampInt(level, 1, villagerMaxLevel);
}

int TradeOfferList::unlockedCount(int level) const {
    int count = 0;
    for (int i = 0; i < size(); ++i)
        if (unlocked(i, level)) ++count;
    return count;
}

int TradeOfferList::countForTier(TradeTier tier) const {
    int count = 0;
    for (const TradeOffer& offer : offers)
        if (offer.tier == tier) ++count;
    return count;
}

bool TradeOfferList::allExhausted(int level) const {
    int reachable = 0;
    for (int i = 0; i < size(); ++i) {
        if (!unlocked(i, level)) continue;
        ++reachable;
        if (!offers[i].exhausted()) return false;
    }
    return reachable > 0;
}

TradeOfferList buildTradeOffers(VillagerProfession profession, uint32_t seed) {
    TradeOfferList list;
    list.profession = profession;

    int specCount = 0;
    const OfferSpec* specs = specsForProfession(profession, specCount);
    if (specs == nullptr) return list;

    uint32_t rng = seed ^ (static_cast<uint32_t>(profession) * 0x9e3779b9u);
    for (int i = 0; i < specCount; ++i) {
        const OfferSpec& spec = specs[i];
        TradeOffer offer;
        offer.tier = static_cast<TradeTier>(spec.tier);
        offer.inputA = spec.inputA;
        offer.inputACount = spec.inputACount;
        offer.inputB = spec.inputB;
        offer.inputBCount = spec.inputBCount;
        offer.output = spec.output;
        offer.outputCount = spec.outputCount;
        offer.maxUses = spec.maxUses;
        offer.experienceReward = spec.experience;
        offer.priceMultiplier = spec.inputA == ItemId::EmeraldGem ? 0.05f : 0.02f;

        uint32_t draw = nextRandom(rng);
        if (offer.inputACount >= 6) {
            offer.inputACount += static_cast<int>(draw % 5u) - 2;
        } else if (offer.outputCount >= 3) {
            offer.outputCount += static_cast<int>(draw % 3u) - 1;
        }
        list.offers.push_back(offer);
    }
    return list;
}

bool restockOffers(TradeOfferList& offers) {
    bool changed = false;
    for (TradeOffer& offer : offers.offers) {
        if (offer.uses > 0) {
            offer.uses = 0;
            changed = true;
        }
        int faded = offer.demand - tradeDemandDecayPerRestock;
        if (faded < 0) faded = 0;
        if (faded != offer.demand) {
            offer.demand = faded;
            changed = true;
        }
    }
    return changed;
}

bool tickVillagerRestock(Villager& villager, TradeOfferList& offers, const TimeOfDay& clock) {
    long long day = clock.dayCount();
    if (villager.lastRestockDay != day) {
        villager.lastRestockDay = day;
        villager.restocksToday = 0;
    }
    if (!villager.alive || villager.restocksToday >= tradeRestocksPerDay) return false;
    if (villager.activity != VillagerActivity::Work ||
        villager.state != VillagerState::AtTarget) return false;

    bool anyUsed = false;
    for (const TradeOffer& offer : offers.offers)
        if (offer.uses > 0) anyUsed = true;
    if (!anyUsed) return false;

    restockOffers(offers);
    ++villager.restocksToday;
    return true;
}

const char* tradeResultReasonName(TradeResult::Reason reason) {
    switch (reason) {
        case TradeResult::Reason::Ok: return "ok";
        case TradeResult::Reason::NoSuchOffer: return "noSuchOffer";
        case TradeResult::Reason::VillagerUnavailable: return "villagerUnavailable";
        case TradeResult::Reason::LevelLocked: return "levelLocked";
        case TradeResult::Reason::OfferExhausted: return "offerExhausted";
        case TradeResult::Reason::MissingInputA: return "missingInputA";
        case TradeResult::Reason::MissingInputB: return "missingInputB";
        case TradeResult::Reason::NoRoom: return "noRoom";
        case TradeResult::Reason::Count: break;
    }
    return "unknown";
}

TradeSession::TradeSession(Villager& villager, TradeOfferList& offers, Container& customerInventory)
    : villager(villager), offers(offers), inventory(customerInventory) {}

void TradeSession::setPriceModifier(const PriceModifier& newModifier) { modifier = newModifier; }

void TradeSession::refreshPrices(const ReputationView& reputation, int heroLevel) {
    modifier = priceModifierFor(reputation, heroLevel, villagerHitCount(villager.gossip));
}

int TradeSession::priceOf(int index) const {
    if (index < 0 || index >= offers.size()) return 0;
    return offerPrice(offers.at(index), modifier);
}

bool TradeSession::available(int index) const {
    if (index < 0 || index >= offers.size()) return false;
    if (!villager.alive || !villager.employed()) return false;
    if (!offers.unlocked(index, villager.level)) return false;
    return !offers.at(index).exhausted();
}

bool TradeSession::canAfford(int index) const {
    if (!available(index)) return false;
    const TradeOffer& offer = offers.at(index);
    if (containerCount(inventory, offer.inputA) < priceOf(index)) return false;
    if (offer.hasSecondaryInput() && containerCount(inventory, offer.inputB) < offer.inputBCount)
        return false;
    return true;
}

TradeResult TradeSession::trade(int index) {
    TradeResult outcome;
    if (index < 0 || index >= offers.size()) {
        outcome.reason = TradeResult::Reason::NoSuchOffer;
        return outcome;
    }
    if (!villager.alive || !villager.employed()) {
        outcome.reason = TradeResult::Reason::VillagerUnavailable;
        return outcome;
    }
    if (!offers.unlocked(index, villager.level)) {
        outcome.reason = TradeResult::Reason::LevelLocked;
        return outcome;
    }

    TradeOffer& offer = offers.at(index);
    if (offer.exhausted()) {
        outcome.reason = TradeResult::Reason::OfferExhausted;
        return outcome;
    }

    int price = offerPrice(offer, modifier);
    if (containerCount(inventory, offer.inputA) < price) {
        outcome.reason = TradeResult::Reason::MissingInputA;
        return outcome;
    }
    if (offer.hasSecondaryInput() && containerCount(inventory, offer.inputB) < offer.inputBCount) {
        outcome.reason = TradeResult::Reason::MissingInputB;
        return outcome;
    }
    if (!containerHasRoom(inventory, offer.output, offer.outputCount)) {
        outcome.reason = TradeResult::Reason::NoRoom;
        return outcome;
    }

    inventory.extract(offer.inputA, price);
    if (offer.hasSecondaryInput()) inventory.extract(offer.inputB, offer.inputBCount);
    inventory.insert(offer.output, offer.outputCount);

    ++offer.uses;
    offer.demand = clampInt(offer.demand + tradeDemandPerUse, 0, tradeDemandCap);

    outcome.success = true;
    outcome.reason = TradeResult::Reason::Ok;
    outcome.pricePaid = price;
    outcome.secondaryPaid = offer.hasSecondaryInput() ? offer.inputBCount : 0;
    outcome.received = offer.output;
    outcome.receivedCount = offer.outputCount;
    outcome.experienceGained = offer.experienceReward;
    outcome.leveledUp = grantVillagerExperience(villager, offer.experienceReward);

    villager.gossip.add(GossipKind::Trading, gossipTradingGain);

    ++completed;
    if (offer.inputA == ItemId::EmeraldGem) spent += price;
    return outcome;
}

}
