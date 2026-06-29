#pragma once

#include <cstdint>
#include <vector>

namespace vf {

// What a village remembers about one player. Every kind accumulates its own
// saturating counter; the signed sum of those counters weighted per kind is the
// reputation that trade prices and iron golem hostility read.
enum class GossipKind : uint8_t {
    MajorPositive = 0,   // saved the village from a raid
    MinorPositive = 1,   // helped a villager
    Trading = 2,         // completed a trade
    MinorNegative = 3,   // hit a villager
    MajorNegative = 4,   // killed a villager
    Count,
};

const char* gossipKindName(GossipKind kind);

// Value granted by one occurrence of each kind.
constexpr int gossipMajorPositiveGain = 20;
constexpr int gossipMinorPositiveGain = 25;
constexpr int gossipTradingGain = 2;
constexpr int gossipMinorNegativeGain = 25;
constexpr int gossipMajorNegativeGain = 25;

// Reputation at or below which iron golems turn on the player.
constexpr int golemHostileReputation = -100;

struct GossipEntry {
    // Per-kind rules: how much a point of this kind is worth, where it
    // saturates, how fast it fades per in-game day, and how far it travels when
    // two villagers meet. A zero sharePercent means the kind never spreads.
    struct Tuning {
        int weight = 1;
        int maxValue = 100;
        int decayPerDay = 10;
        int shareCap = 0;
        int sharePercent = 0;

        bool shareable() const { return sharePercent > 0 && shareCap > 0; }
    };

    GossipKind kind = GossipKind::Trading;
    int value = 0;
};

const GossipEntry::Tuning& gossipTuning(GossipKind kind);

// One villager's memory of the player. Values only ever saturate at their per
// kind cap, so no single event source can dominate the sum without bound.
class GossipLedger {
public:
    // Adds `amount` points of `kind`, clamped into [0, maxValue]. Returns the
    // amount actually stored.
    int add(GossipKind kind, int amount);

    int valueOf(GossipKind kind) const;

    // Signed weighted sum across every kind.
    int reputation() const;

    // Fades every kind by its own per-day rate. Returns true when anything moved.
    bool decayDays(int days);

    // Spreads shareable kinds into `receiver`. The receiver can never be pushed
    // past the kind's share cap by gossip alone, so repeated meetings converge
    // instead of growing without limit. Returns true when the receiver changed.
    bool shareInto(GossipLedger& receiver) const;

    // Non-zero entries in GossipKind order.
    std::vector<GossipEntry> entries() const;

    int entryCount() const;
    bool empty() const;
    void forget(GossipKind kind);
    void clear();

private:
    int values[static_cast<int>(GossipKind::Count)] = {};
};

// Reputation resolved for one villager against the backdrop of its village:
// `personal` drives that villager's prices, `village` drives golem hostility.
struct ReputationView {
    int personal = 0;
    int village = 0;
    int villagerCount = 0;
    int majorNegative = 0;

    // Positive lowers prices, negative raises them. Clamped to keep a single
    // extreme ledger from making goods free.
    int discountPercent() const;

    bool golemHostile() const { return village <= golemHostileReputation; }
    bool friendly() const { return personal > 0; }
};

ReputationView buildReputation(const GossipLedger& personal,
                               const std::vector<const GossipLedger*>& village);

} // namespace vf
