#pragma once

#include <cstdint>
#include <vector>

namespace vf {

enum class GossipKind : uint8_t {
    MajorPositive = 0,
    MinorPositive = 1,
    Trading = 2,
    MinorNegative = 3,
    MajorNegative = 4,
    Count,
};

const char* gossipKindName(GossipKind kind);

constexpr int gossipMajorPositiveGain = 20;
constexpr int gossipMinorPositiveGain = 25;
constexpr int gossipTradingGain = 2;
constexpr int gossipMinorNegativeGain = 25;
constexpr int gossipMajorNegativeGain = 25;

constexpr int golemHostileReputation = -100;

struct GossipEntry {
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

class GossipLedger {
public:
    int add(GossipKind kind, int amount);

    int valueOf(GossipKind kind) const;

    int reputation() const;

    bool decayDays(int days);

    bool shareInto(GossipLedger& receiver) const;

    std::vector<GossipEntry> entries() const;

    int entryCount() const;
    bool empty() const;
    void forget(GossipKind kind);
    void clear();

private:
    int values[static_cast<int>(GossipKind::Count)] = {};
};

struct ReputationView {
    int personal = 0;
    int village = 0;
    int villagerCount = 0;
    int majorNegative = 0;

    int discountPercent() const;

    bool golemHostile() const { return village <= golemHostileReputation; }
    bool friendly() const { return personal > 0; }
};

ReputationView buildReputation(const GossipLedger& personal,
                               const std::vector<const GossipLedger*>& village);

}
