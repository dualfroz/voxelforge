#pragma once

#include <cstdint>
#include <vector>
#include "Container.h"
#include "Gossip.h"
#include "Item.h"
#include "TimeOfDay.h"
#include "Villager.h"

namespace vf {

// Trade unlock band. A villager of level N has tiers 0..N-1 unlocked.
enum class TradeTier : uint8_t {
    Novice = 0,
    Apprentice,
    Journeyman,
    Expert,
    Master,
    Count,
};

const char* tradeTierName(TradeTier tier);
TradeTier tierForLevel(int level);
int levelForTier(TradeTier tier);

constexpr int heroDiscountPercentPerLevel = 30;
constexpr int hitSurchargePercent = 20;

// Everything outside the offer itself that bends its price: how the villager
// feels about the customer, the hero of the village effect, and how many
// unforgiven hits the customer landed on villagers.
struct PriceModifier {
    int reputationPercent = 0;
    int heroLevel = 0;
    int hitCount = 0;

    // Net discount in percent. Positive makes goods cheaper.
    int totalPercent() const;
};

PriceModifier priceModifierFor(const ReputationView& reputation, int heroLevel, int hitCount);
int villagerHitCount(const GossipLedger& gossip);

constexpr int tradeDemandCap = 64;
constexpr int tradeDemandPerUse = 1;
constexpr int tradeDemandDecayPerRestock = 2;
constexpr int tradeRestocksPerDay = 2;

// One entry in a villager's trade list: up to two inputs for one output, a use
// budget that a restock refills, and a demand counter that raises the price the
// more the offer is taken.
struct TradeOffer {
    ItemId inputA = ItemId::None;
    int inputACount = 1;
    ItemId inputB = ItemId::None;
    int inputBCount = 0;
    ItemId output = ItemId::None;
    int outputCount = 1;
    int maxUses = 12;
    int uses = 0;
    int demand = 0;
    int experienceReward = villagerExperiencePerTrade;
    float priceMultiplier = 0.05f;
    TradeTier tier = TradeTier::Novice;

    bool exhausted() const { return uses >= maxUses; }
    int basePrice() const { return inputACount; }
    bool hasSecondaryInput() const { return inputB != ItemId::None && inputBCount > 0; }
};

// Base price plus demand, then the modifier percentage. Never drops below one.
int offerPrice(const TradeOffer& offer, const PriceModifier& modifier);

// A villager's whole trade table across all five tiers. Which entries are
// reachable depends on the villager's current level.
struct TradeOfferList {
    std::vector<TradeOffer> offers;
    VillagerProfession profession = VillagerProfession::Unemployed;

    int size() const { return static_cast<int>(offers.size()); }
    const TradeOffer& at(int index) const { return offers[index]; }
    TradeOffer& at(int index) { return offers[index]; }
    bool unlocked(int index, int level) const;
    int unlockedCount(int level) const;
    int countForTier(TradeTier tier) const;
    bool allExhausted(int level) const;
};

// Deterministic per profession and seed: the same pair always yields the same
// table, with per-offer count jitter drawn from the seed.
TradeOfferList buildTradeOffers(VillagerProfession profession, uint32_t seed);

// Refills use budgets and lets demand fade. Returns true when anything changed.
bool restockOffers(TradeOfferList& offers);

// Restocks at most twice per day, and only while the villager is standing at
// its own workstation. Returns true on the tick a restock fires.
bool tickVillagerRestock(Villager& villager, TradeOfferList& offers, const TimeOfDay& clock);

struct TradeResult {
    enum class Reason : uint8_t {
        Ok = 0,
        NoSuchOffer,
        VillagerUnavailable,
        LevelLocked,
        OfferExhausted,
        MissingInputA,
        MissingInputB,
        NoRoom,
        Count,
    };

    Reason reason = Reason::Ok;
    bool success = false;
    int pricePaid = 0;
    int secondaryPaid = 0;
    ItemId received = ItemId::None;
    int receivedCount = 0;
    int experienceGained = 0;
    bool leveledUp = false;
};

const char* tradeResultReasonName(TradeResult::Reason reason);

// Binds a villager, its offer table and the customer's inventory for the
// duration of a trading screen. Every completed trade moves items, raises
// demand, feeds the villager experience and leaves a Trading gossip entry.
class TradeSession {
public:
    TradeSession(Villager& villager, TradeOfferList& offers, Container& customerInventory);

    void setPriceModifier(const PriceModifier& modifier);
    void refreshPrices(const ReputationView& reputation, int heroLevel);
    const PriceModifier& priceModifier() const { return modifier; }

    int priceOf(int index) const;
    bool available(int index) const;
    bool canAfford(int index) const;

    TradeResult trade(int index);

    int tradesCompleted() const { return completed; }
    int emeraldsSpent() const { return spent; }

private:
    Villager& villager;
    TradeOfferList& offers;
    Container& inventory;
    PriceModifier modifier;
    int completed = 0;
    int spent = 0;
};

} // namespace vf
