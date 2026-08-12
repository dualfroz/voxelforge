#include "testHarness.h"
#include "vf/Gossip.h"

using namespace vf;

namespace {

GossipLedger ledgerWith(GossipKind kind, int occurrences, int gainPerOccurrence) {
    GossipLedger ledger;
    for (int i = 0; i < occurrences; ++i) ledger.add(kind, gainPerOccurrence);
    return ledger;
}

int shareRounds(const GossipLedger& source, GossipLedger& target, int rounds) {
    int changes = 0;
    for (int i = 0; i < rounds; ++i)
        if (source.shareInto(target)) ++changes;
    return changes;
}

} // namespace

VF_TEST(gossipSaturatesAtItsPerKindCapNoMatterHowManyEventsLand) {
    GossipLedger ledger = ledgerWith(GossipKind::MinorNegative, 40, gossipMinorNegativeGain);
    CHECK_EQ(ledger.valueOf(GossipKind::MinorNegative),
             gossipTuning(GossipKind::MinorNegative).maxValue);

    GossipLedger trading = ledgerWith(GossipKind::Trading, 100, gossipTradingGain);
    CHECK_EQ(trading.valueOf(GossipKind::Trading), gossipTuning(GossipKind::Trading).maxValue);

    CHECK_EQ(trading.add(GossipKind::Trading, gossipTradingGain), 0);
}

VF_TEST(reputationWeighsPositiveAndNegativeMemoriesAgainstEachOther) {
    GossipLedger ledger;
    ledger.add(GossipKind::Trading, gossipTradingGain);
    CHECK_EQ(ledger.reputation(), gossipTradingGain * gossipTuning(GossipKind::Trading).weight);

    ledger.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    int expected = gossipTradingGain * gossipTuning(GossipKind::Trading).weight +
                   gossipMinorNegativeGain * gossipTuning(GossipKind::MinorNegative).weight;
    CHECK_EQ(ledger.reputation(), expected);
    CHECK(ledger.reputation() < 0);

    int before = ledger.reputation();
    ledger.add(GossipKind::MajorPositive, gossipMajorPositiveGain);
    CHECK_EQ(ledger.reputation() - before,
             gossipMajorPositiveGain * gossipTuning(GossipKind::MajorPositive).weight);
    CHECK_EQ(ledger.entryCount(), 3);

    ledger.add(GossipKind::MajorPositive, gossipMajorPositiveGain);
    CHECK(ledger.reputation() > 0);
}

VF_TEST(hittingAVillagerAlreadyTurnsGolemsAndKillingOneIsWorseStill) {
    GossipLedger polite;
    polite.add(GossipKind::Trading, gossipTradingGain * 5);
    CHECK(!buildReputation(polite, {}).golemHostile());

    GossipLedger ledger;
    ledger.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    ReputationView afterHit = buildReputation(ledger, {});
    CHECK(afterHit.golemHostile());
    CHECK(afterHit.personal <= golemHostileReputation);
    CHECK_EQ(afterHit.majorNegative, 0);

    ledger.add(GossipKind::MajorNegative, gossipMajorNegativeGain);
    ReputationView afterKill = buildReputation(ledger, {});
    CHECK(afterKill.personal < afterHit.personal);
    CHECK_EQ(afterKill.majorNegative, gossipMajorNegativeGain);
}

VF_TEST(minorGossipFadesWithinDaysWhileAKillIsRememberedForWeeks) {
    GossipLedger ledger;
    ledger.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    ledger.add(GossipKind::MajorNegative, gossipMajorNegativeGain);

    ledger.decayDays(1);
    CHECK_EQ(ledger.valueOf(GossipKind::MinorNegative),
             gossipMinorNegativeGain - gossipTuning(GossipKind::MinorNegative).decayPerDay);
    CHECK(ledger.valueOf(GossipKind::MajorNegative) > 20);

    ledger.decayDays(1);
    CHECK_EQ(ledger.valueOf(GossipKind::MinorNegative), 0);
    CHECK(ledger.valueOf(GossipKind::MajorNegative) > 0);
    CHECK(buildReputation(ledger, {}).golemHostile());

    for (int day = 0; day < 12; ++day) ledger.decayDays(1);
    CHECK_EQ(ledger.valueOf(GossipKind::MajorNegative), 0);
    CHECK(ledger.empty());
    CHECK(!ledger.decayDays(1));
}

VF_TEST(gossipSharedBetweenNeighboursConvergesOnTheShareCap) {
    GossipLedger loud = ledgerWith(GossipKind::MinorNegative, 8, gossipMinorNegativeGain);
    CHECK_EQ(loud.valueOf(GossipKind::MinorNegative), 200);

    GossipLedger quiet;
    CHECK(loud.shareInto(quiet));
    CHECK_EQ(quiet.valueOf(GossipKind::MinorNegative),
             gossipTuning(GossipKind::MinorNegative).shareCap);

    shareRounds(loud, quiet, 20);
    CHECK_EQ(quiet.valueOf(GossipKind::MinorNegative),
             gossipTuning(GossipKind::MinorNegative).shareCap);
    CHECK(!loud.shareInto(quiet));
    CHECK(quiet.valueOf(GossipKind::MinorNegative) < loud.valueOf(GossipKind::MinorNegative));

    GossipLedger chatty = ledgerWith(GossipKind::Trading, 20, gossipTradingGain);
    CHECK_EQ(chatty.valueOf(GossipKind::Trading), gossipTuning(GossipKind::Trading).maxValue);
    GossipLedger listener;
    shareRounds(chatty, listener, 10);
    CHECK_EQ(listener.valueOf(GossipKind::Trading), gossipTuning(GossipKind::Trading).shareCap);
}

VF_TEST(sharingNeverLowersWhatAWitnessSawFirstHand) {
    GossipLedger witness = ledgerWith(GossipKind::MinorNegative, 8, gossipMinorNegativeGain);
    GossipLedger hearsay;
    hearsay.add(GossipKind::MinorNegative, 50);

    CHECK(witness.shareInto(hearsay));
    CHECK_EQ(hearsay.valueOf(GossipKind::MinorNegative),
             gossipTuning(GossipKind::MinorNegative).shareCap);

    GossipLedger stubborn = ledgerWith(GossipKind::MinorNegative, 8, gossipMinorNegativeGain);
    CHECK(!hearsay.shareInto(stubborn));
    CHECK_EQ(stubborn.valueOf(GossipKind::MinorNegative), 200);
}

VF_TEST(whatOneVillagerSawAloneNeverSpreadsToTheRestOfTheVillage) {
    GossipLedger firstHand;
    firstHand.add(GossipKind::MajorNegative, gossipMajorNegativeGain);
    firstHand.add(GossipKind::MajorPositive, gossipMajorPositiveGain);

    GossipLedger neighbour;
    CHECK(!firstHand.shareInto(neighbour));
    CHECK_EQ(neighbour.valueOf(GossipKind::MajorNegative), 0);
    CHECK_EQ(neighbour.valueOf(GossipKind::MajorPositive), 0);

    CHECK(!gossipTuning(GossipKind::MajorNegative).shareable());
    CHECK(!gossipTuning(GossipKind::MajorPositive).shareable());
    CHECK(gossipTuning(GossipKind::Trading).shareable());
}

VF_TEST(reputationViewKeepsPersonalOpinionApartFromTheVillageAverage) {
    GossipLedger friendly;
    friendly.add(GossipKind::MajorPositive, gossipMajorPositiveGain);

    GossipLedger hostileA;
    hostileA.add(GossipKind::MajorNegative, gossipMajorNegativeGain);
    GossipLedger hostileB;
    hostileB.add(GossipKind::MajorNegative, gossipMajorNegativeGain);

    std::vector<const GossipLedger*> mixed{&friendly, &hostileA, &hostileB};
    ReputationView view = buildReputation(friendly, mixed);

    CHECK_EQ(view.villagerCount, 3);
    CHECK_EQ(view.personal, 100);
    CHECK_EQ(view.village, -50);
    CHECK(view.friendly());
    CHECK(!view.golemHostile());
    CHECK_EQ(view.majorNegative, gossipMajorNegativeGain);

    std::vector<const GossipLedger*> allHostile{&hostileA, &hostileB};
    ReputationView angry = buildReputation(friendly, allHostile);
    CHECK_EQ(angry.personal, 100);
    CHECK(angry.golemHostile());
    CHECK(angry.friendly());
}

VF_TEST(discountPercentTracksReputationAndStaysInsideItsClamp) {
    GossipLedger neutral;
    CHECK_EQ(buildReputation(neutral, {}).discountPercent(), 0);

    GossipLedger liked;
    for (int i = 0; i < 20; ++i) liked.add(GossipKind::MinorPositive, gossipMinorPositiveGain);
    ReputationView likedView = buildReputation(liked, {});
    CHECK_EQ(likedView.personal, 1000);
    CHECK_EQ(likedView.discountPercent(), 60);

    GossipLedger hated;
    for (int i = 0; i < 20; ++i) hated.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    CHECK_EQ(buildReputation(hated, {}).discountPercent(), -60);

    GossipLedger mild;
    mild.add(GossipKind::Trading, 20);
    CHECK_EQ(buildReputation(mild, {}).discountPercent(), 2);
}

VF_TEST(forgettingOneKindLeavesTheRestOfTheMemoryIntact) {
    GossipLedger ledger;
    ledger.add(GossipKind::Trading, gossipTradingGain * 3);
    ledger.add(GossipKind::MinorNegative, gossipMinorNegativeGain);
    CHECK_EQ(ledger.entryCount(), 2);

    ledger.forget(GossipKind::MinorNegative);
    CHECK_EQ(ledger.entryCount(), 1);
    CHECK_EQ(ledger.valueOf(GossipKind::Trading), gossipTradingGain * 3);

    std::vector<GossipEntry> remaining = ledger.entries();
    CHECK_EQ(static_cast<int>(remaining.size()), 1);
    CHECK(remaining[0].kind == GossipKind::Trading);

    ledger.clear();
    CHECK(ledger.empty());
}
