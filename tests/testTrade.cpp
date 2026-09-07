#include "testHarness.h"
#include "vf/Trade.h"

using namespace vf;

namespace {

constexpr int groundTop = 4;
constexpr int walkY = groundTop + 1;
const Vec3i villageCenter{0, walkY, 0};

// Index of the fixed "4 emeralds for one comparator" librarian offer: the one
// entry whose counts the seed jitter leaves alone, so prices are exact.
constexpr int comparatorOffer = 5;

World makeShopWorld() {
    World world;
    for (int x = -12; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z)
            for (int y = 0; y <= groundTop; ++y)
                world.setBlock(x, y, z, Block::Stone);
    world.setBlock(-4, walkY, 0, Block::Lectern);
    world.setBlock(4, walkY, 0, Block::Bed);
    return world;
}

Villager makeLibrarian(int level) {
    Villager villager = makeVillager(1, {-1.5f, static_cast<float>(walkY), 0.5f}, 17u);
    villager.profession = VillagerProfession::Librarian;
    villager.level = level;
    villager.experience = villagerExperienceForLevel(level);
    return villager;
}

Container makeWallet(int emeralds) {
    Container inventory(27);
    inventory.insert(ItemId::EmeraldGem, emeralds);
    return inventory;
}

int stockOf(const Container& inventory, ItemId item) {
    int total = 0;
    for (int i = 0; i < inventory.size(); ++i) {
        const ItemSlot& slot = inventory.slot(i);
        if (!slot.empty() && slot.item == item) total += slot.count;
    }
    return total;
}

int firstOfferWithTier(const TradeOfferList& list, TradeTier tier) {
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i).tier == tier) return i;
    return -1;
}

} // namespace

VF_TEST(offerTableIsIdenticalForTheSameProfessionAndSeedAndMovesWithTheSeed) {
    TradeOfferList a = buildTradeOffers(VillagerProfession::Librarian, 99u);
    TradeOfferList b = buildTradeOffers(VillagerProfession::Librarian, 99u);
    CHECK_EQ(a.size(), b.size());
    bool identical = true;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).inputACount != b.at(i).inputACount) identical = false;
        if (a.at(i).outputCount != b.at(i).outputCount) identical = false;
        if (a.at(i).output != b.at(i).output) identical = false;
    }
    CHECK(identical);

    TradeOfferList other = buildTradeOffers(VillagerProfession::Librarian, 4242u);
    bool differs = false;
    for (int i = 0; i < a.size(); ++i)
        if (a.at(i).inputACount != other.at(i).inputACount ||
            a.at(i).outputCount != other.at(i).outputCount) differs = true;
    CHECK(differs);

    for (int i = 0; i < a.size(); ++i) {
        CHECK(a.at(i).inputACount >= 1);
        CHECK(a.at(i).outputCount >= 1);
        CHECK(a.at(i).output != ItemId::None);
        CHECK(a.at(i).inputA != ItemId::None);
    }
}

VF_TEST(everyTradingProfessionCoversAllFiveTiersWhileNitwitsOfferNothing) {
    for (int raw = 2; raw < static_cast<int>(VillagerProfession::Count); ++raw) {
        VillagerProfession profession = static_cast<VillagerProfession>(raw);
        TradeOfferList list = buildTradeOffers(profession, 7u);
        CHECK_EQ(list.size(), 8);
        CHECK(list.profession == profession);
        for (int tier = 0; tier < static_cast<int>(TradeTier::Count); ++tier)
            CHECK(list.countForTier(static_cast<TradeTier>(tier)) >= 1);
        CHECK_EQ(list.unlockedCount(1), list.countForTier(TradeTier::Novice));
        CHECK_EQ(list.unlockedCount(villagerMaxLevel), list.size());
    }

    CHECK_EQ(buildTradeOffers(VillagerProfession::Nitwit, 7u).size(), 0);
    CHECK_EQ(buildTradeOffers(VillagerProfession::Unemployed, 7u).size(), 0);
}

VF_TEST(lockedTiersRefuseTheTradeUntilTheVillagerHasLevelledUp) {
    Villager villager = makeLibrarian(1);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(200);
    TradeSession session(villager, offers, wallet);

    int masterOffer = firstOfferWithTier(offers, TradeTier::Master);
    CHECK(masterOffer >= 0);
    CHECK(!session.available(masterOffer));
    TradeResult locked = session.trade(masterOffer);
    CHECK(!locked.success);
    CHECK(locked.reason == TradeResult::Reason::LevelLocked);
    CHECK_EQ(stockOf(wallet, ItemId::EmeraldGem), 200);

    villager.level = villagerMaxLevel;
    CHECK(session.available(masterOffer));
    TradeResult unlocked = session.trade(masterOffer);
    CHECK(unlocked.success);
    CHECK(unlocked.received == ItemId::DiamondGem);
    CHECK_EQ(stockOf(wallet, ItemId::DiamondGem), unlocked.receivedCount);
}

VF_TEST(repeatedTradesRaiseThePriceThroughRisingDemand) {
    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);

    CHECK(offers.at(comparatorOffer).tier == TradeTier::Journeyman);
    CHECK(offers.at(comparatorOffer).output == ItemId::Comparator);
    int basePrice = offers.at(comparatorOffer).basePrice();
    CHECK_EQ(session.priceOf(comparatorOffer), basePrice);

    int firstPrice = session.priceOf(comparatorOffer);
    int trades = 0;
    int lastPrice = firstPrice;
    while (session.available(comparatorOffer)) {
        int price = session.priceOf(comparatorOffer);
        CHECK(price >= lastPrice);
        lastPrice = price;
        TradeResult outcome = session.trade(comparatorOffer);
        CHECK(outcome.success);
        CHECK_EQ(outcome.pricePaid, price);
        ++trades;
    }

    CHECK_EQ(trades, offers.at(comparatorOffer).maxUses);
    CHECK_EQ(offers.at(comparatorOffer).demand, trades * tradeDemandPerUse);
    CHECK(lastPrice > firstPrice);
    CHECK_EQ(stockOf(wallet, ItemId::Comparator), trades);
    CHECK(session.trade(comparatorOffer).reason == TradeResult::Reason::OfferExhausted);
}

VF_TEST(restockRefillsUsesAndLetsDemandAndPriceFallBack) {
    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);

    for (int i = 0; i < 10; ++i) CHECK(session.trade(comparatorOffer).success);
    int busyPrice = session.priceOf(comparatorOffer);
    int busyDemand = offers.at(comparatorOffer).demand;
    CHECK_EQ(offers.at(comparatorOffer).uses, 10);

    CHECK(restockOffers(offers));
    CHECK_EQ(offers.at(comparatorOffer).uses, 0);
    CHECK_EQ(offers.at(comparatorOffer).demand, busyDemand - tradeDemandDecayPerRestock);
    CHECK(session.priceOf(comparatorOffer) <= busyPrice);
    CHECK(session.available(comparatorOffer));

    for (int i = 0; i < 20; ++i) restockOffers(offers);
    CHECK_EQ(offers.at(comparatorOffer).demand, 0);
    CHECK_EQ(session.priceOf(comparatorOffer), offers.at(comparatorOffer).basePrice());
    CHECK(!restockOffers(offers));
}

VF_TEST(theVillagerRestocksAtMostTwiceADayAndOnlyAtItsOwnWorkstation) {
    World world = makeShopWorld();
    PoiRegistry pois;
    pois.scan(world, villageCenter, 10, 4);

    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);
    VillagerSchedule schedule;
    TimeOfDay clock(static_cast<WorldTick>(0.35 * 24000.0));

    CHECK(session.trade(comparatorOffer).success);
    CHECK(!tickVillagerRestock(villager, offers, clock));

    for (int tick = 0; tick < 400; ++tick)
        tickVillager(world, villager, pois, clock, schedule, villageCenter, false, 0.05f);
    CHECK(villager.activity == VillagerActivity::Work);
    CHECK(villager.state == VillagerState::AtTarget);
    CHECK_EQ(pois.at(villager.workstationIndex).block, Block::Lectern);

    CHECK(tickVillagerRestock(villager, offers, clock));
    CHECK_EQ(villager.restocksToday, 1);
    CHECK(!tickVillagerRestock(villager, offers, clock));

    CHECK(session.trade(comparatorOffer).success);
    CHECK(tickVillagerRestock(villager, offers, clock));
    CHECK_EQ(villager.restocksToday, tradeRestocksPerDay);

    CHECK(session.trade(comparatorOffer).success);
    CHECK(!tickVillagerRestock(villager, offers, clock));

    clock.setTick(static_cast<WorldTick>(24000 + 0.35 * 24000.0));
    CHECK(tickVillagerRestock(villager, offers, clock));
    CHECK_EQ(villager.restocksToday, 1);
    CHECK_EQ(villager.lastRestockDay, 1);
}

VF_TEST(tradingWithoutEnoughEmeraldsFailsAndLeavesTheInventoryAlone) {
    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(2);
    TradeSession session(villager, offers, wallet);

    CHECK(session.available(comparatorOffer));
    CHECK(!session.canAfford(comparatorOffer));
    TradeResult outcome = session.trade(comparatorOffer);
    CHECK(!outcome.success);
    CHECK(outcome.reason == TradeResult::Reason::MissingInputA);
    CHECK_EQ(stockOf(wallet, ItemId::EmeraldGem), 2);
    CHECK_EQ(stockOf(wallet, ItemId::Comparator), 0);
    CHECK_EQ(offers.at(comparatorOffer).uses, 0);
    CHECK_EQ(villager.experience, villagerExperienceForLevel(3));

    CHECK(session.trade(-1).reason == TradeResult::Reason::NoSuchOffer);
    CHECK(session.trade(offers.size()).reason == TradeResult::Reason::NoSuchOffer);
    CHECK(!session.available(99));
    CHECK_EQ(session.priceOf(99), 0);
}

VF_TEST(aDeadOrUnemployedVillagerRefusesToTradeAtAll) {
    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);
    CHECK(session.trade(comparatorOffer).success);

    villager.alive = false;
    TradeResult dead = session.trade(comparatorOffer);
    CHECK(!dead.success);
    CHECK(dead.reason == TradeResult::Reason::VillagerUnavailable);

    villager.alive = true;
    villager.profession = VillagerProfession::Unemployed;
    CHECK(session.trade(comparatorOffer).reason == TradeResult::Reason::VillagerUnavailable);
    CHECK_EQ(session.tradesCompleted(), 1);
}

VF_TEST(heroOfTheVillageCutsPricesWhileHittingAVillagerRaisesThem) {
    Villager villager = makeLibrarian(3);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);

    int listPrice = session.priceOf(comparatorOffer);
    CHECK_EQ(listPrice, offers.at(comparatorOffer).basePrice());

    ReputationView neutral = buildReputation(villager.gossip, {});
    session.setPriceModifier(priceModifierFor(neutral, 1, 0));
    int heroPrice = session.priceOf(comparatorOffer);
    CHECK(heroPrice < listPrice);
    CHECK_EQ(session.priceModifier().totalPercent(), heroDiscountPercentPerLevel);

    villager.gossip.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    CHECK_EQ(villagerHitCount(villager.gossip), 1);
    session.refreshPrices(buildReputation(villager.gossip, {}), 0);
    int angryPrice = session.priceOf(comparatorOffer);
    CHECK(angryPrice > listPrice);
    CHECK(session.priceModifier().totalPercent() < 0);

    session.refreshPrices(buildReputation(villager.gossip, {}), 1);
    CHECK(session.priceOf(comparatorOffer) < angryPrice);
}

VF_TEST(priceNeverDropsBelowOneNoMatterHowLargeTheDiscount) {
    TradeOffer offer;
    offer.inputA = ItemId::EmeraldGem;
    offer.inputACount = 2;
    offer.output = ItemId::Bread;
    offer.outputCount = 4;

    PriceModifier generous;
    generous.heroLevel = 5;
    CHECK_EQ(generous.totalPercent(), 90);
    CHECK_EQ(offerPrice(offer, generous), 1);

    PriceModifier absurd;
    absurd.reputationPercent = 60;
    absurd.heroLevel = 5;
    CHECK_EQ(absurd.totalPercent(), 90);
    CHECK_EQ(offerPrice(offer, absurd), 1);

    PriceModifier hostile;
    hostile.hitCount = 8;
    CHECK_EQ(hostile.totalPercent(), -160);
    CHECK_EQ(offerPrice(offer, hostile), 5);

    offer.demand = 40;
    CHECK(offerPrice(offer, PriceModifier{}) > 2);
    CHECK_EQ(offerPrice(offer, generous), 1);
}

VF_TEST(aCompletedTradeMovesBothSidesAndFeedsTheVillagerExperience) {
    Villager villager = makeLibrarian(1);
    villager.experience = 0;
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(40);
    TradeSession session(villager, offers, wallet);

    int lapisOffer = 1;
    CHECK(offers.at(lapisOffer).output == ItemId::LapisLazuli);
    int price = session.priceOf(lapisOffer);
    TradeResult outcome = session.trade(lapisOffer);

    CHECK(outcome.success);
    CHECK(outcome.reason == TradeResult::Reason::Ok);
    CHECK_EQ(outcome.pricePaid, price);
    CHECK_EQ(outcome.secondaryPaid, 0);
    CHECK_EQ(stockOf(wallet, ItemId::EmeraldGem), 40 - price);
    CHECK_EQ(stockOf(wallet, ItemId::LapisLazuli), outcome.receivedCount);
    CHECK_EQ(villager.experience, outcome.experienceGained);
    CHECK_EQ(villager.gossip.valueOf(GossipKind::Trading), gossipTradingGain);
    CHECK_EQ(session.tradesCompleted(), 1);
    CHECK_EQ(session.emeraldsSpent(), price);

    bool sawLevelUp = false;
    for (int i = 0; i < 12 && session.available(lapisOffer); ++i)
        if (session.trade(lapisOffer).leveledUp) sawLevelUp = true;
    CHECK(sawLevelUp);
    CHECK_EQ(villager.level, 2);
    CHECK(villager.gossip.valueOf(GossipKind::Trading) > gossipTradingGain);
}

VF_TEST(secondaryInputIsRequiredAndConsumedByTheTrade) {
    Villager villager = makeVillager(2, {0.5f, static_cast<float>(walkY), 0.5f}, 5u);
    villager.profession = VillagerProfession::Cartographer;
    villager.level = 2;
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Cartographer, 3u);
    Container wallet = makeWallet(60);
    TradeSession session(villager, offers, wallet);

    int chestOffer = 3;
    CHECK(offers.at(chestOffer).output == ItemId::Chest);
    CHECK(offers.at(chestOffer).hasSecondaryInput());
    CHECK(offers.at(chestOffer).inputB == ItemId::GoldIngot);

    CHECK(!session.canAfford(chestOffer));
    CHECK(session.trade(chestOffer).reason == TradeResult::Reason::MissingInputB);

    wallet.insert(ItemId::GoldIngot, 3);
    CHECK(session.canAfford(chestOffer));
    int price = session.priceOf(chestOffer);
    TradeResult outcome = session.trade(chestOffer);
    CHECK(outcome.success);
    CHECK_EQ(outcome.secondaryPaid, offers.at(chestOffer).inputBCount);
    CHECK_EQ(stockOf(wallet, ItemId::GoldIngot), 3 - offers.at(chestOffer).inputBCount);
    CHECK_EQ(stockOf(wallet, ItemId::EmeraldGem), 60 - price);
    CHECK_EQ(stockOf(wallet, ItemId::Chest), outcome.receivedCount);
}

VF_TEST(sellingToTheVillagerTakesGoodsAndPaysOutEmeralds) {
    Villager villager = makeLibrarian(1);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet(27);
    int planksOffer = 0;
    CHECK(offers.at(planksOffer).inputA == ItemId::Planks);
    CHECK(offers.at(planksOffer).output == ItemId::EmeraldGem);

    TradeSession session(villager, offers, wallet);
    CHECK(session.trade(planksOffer).reason == TradeResult::Reason::MissingInputA);

    int price = session.priceOf(planksOffer);
    wallet.insert(ItemId::Planks, price * 2);
    TradeResult outcome = session.trade(planksOffer);
    CHECK(outcome.success);
    CHECK_EQ(stockOf(wallet, ItemId::Planks), price);
    CHECK_EQ(stockOf(wallet, ItemId::EmeraldGem), outcome.receivedCount);
    CHECK_EQ(session.emeraldsSpent(), 0);
}

VF_TEST(tradingLeavesGossipAndAStrongReputationCutsThePrice) {
    Villager villager = makeLibrarian(villagerMaxLevel);
    TradeOfferList offers = buildTradeOffers(VillagerProfession::Librarian, 1u);
    Container wallet = makeWallet(400);
    TradeSession session(villager, offers, wallet);

    int diamondOffer = 7;
    CHECK(offers.at(diamondOffer).tier == TradeTier::Master);
    CHECK(offers.at(diamondOffer).output == ItemId::DiamondGem);

    for (int i = 0; i < 2; ++i) CHECK(session.trade(diamondOffer).success);
    CHECK_EQ(villager.gossip.valueOf(GossipKind::Trading), gossipTradingGain * 2);

    restockOffers(offers);
    CHECK_EQ(offers.at(diamondOffer).demand, 0);
    session.refreshPrices(buildReputation(villager.gossip, {}), 0);
    int politePrice = session.priceOf(diamondOffer);
    CHECK_EQ(politePrice, offers.at(diamondOffer).basePrice());

    villager.gossip.add(GossipKind::MajorPositive, gossipMajorPositiveGain);
    ReputationView heroic = buildReputation(villager.gossip, {});
    CHECK_EQ(heroic.discountPercent(), 10);
    session.refreshPrices(heroic, 0);
    CHECK(session.priceOf(diamondOffer) < politePrice);

    session.refreshPrices(heroic, 3);
    CHECK_EQ(session.priceModifier().totalPercent(), 90);
    CHECK_EQ(session.priceOf(diamondOffer), 1);
}
