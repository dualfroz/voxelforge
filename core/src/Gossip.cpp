#include "vf/Gossip.h"

namespace vf {

namespace {

int kindIndex(GossipKind kind) { return static_cast<int>(kind); }

int clampInt(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

const GossipEntry::Tuning tuningTable[static_cast<int>(GossipKind::Count)] = {
    // weight, maxValue, decayPerDay, shareCap, sharePercent
    {5, 100, 10, 0, 0},
    {5, 200, 20, 100, 50},
    {1, 25, 2, 20, 50},
    {-5, 200, 20, 100, 50},
    {-5, 100, 2, 0, 0},
};

} // namespace

const char* gossipKindName(GossipKind kind) {
    switch (kind) {
        case GossipKind::MajorPositive: return "majorPositive";
        case GossipKind::MinorPositive: return "minorPositive";
        case GossipKind::Trading: return "trading";
        case GossipKind::MinorNegative: return "minorNegative";
        case GossipKind::MajorNegative: return "majorNegative";
        case GossipKind::Count: break;
    }
    return "unknown";
}

const GossipEntry::Tuning& gossipTuning(GossipKind kind) {
    return tuningTable[kindIndex(kind)];
}

int GossipLedger::add(GossipKind kind, int amount) {
    if (amount == 0) return 0;
    const GossipEntry::Tuning& tuning = gossipTuning(kind);
    int& slot = values[kindIndex(kind)];
    int before = slot;
    slot = clampInt(slot + amount, 0, tuning.maxValue);
    return slot - before;
}

int GossipLedger::valueOf(GossipKind kind) const { return values[kindIndex(kind)]; }

int GossipLedger::reputation() const {
    int total = 0;
    for (int i = 0; i < static_cast<int>(GossipKind::Count); ++i)
        total += values[i] * tuningTable[i].weight;
    return total;
}

bool GossipLedger::decayDays(int days) {
    if (days <= 0) return false;
    bool changed = false;
    for (int i = 0; i < static_cast<int>(GossipKind::Count); ++i) {
        if (values[i] == 0) continue;
        int faded = clampInt(values[i] - tuningTable[i].decayPerDay * days, 0, tuningTable[i].maxValue);
        if (faded != values[i]) {
            values[i] = faded;
            changed = true;
        }
    }
    return changed;
}

bool GossipLedger::shareInto(GossipLedger& receiver) const {
    bool changed = false;
    for (int i = 0; i < static_cast<int>(GossipKind::Count); ++i) {
        const GossipEntry::Tuning& tuning = tuningTable[i];
        if (!tuning.shareable() || values[i] <= 0) continue;
        int& target = receiver.values[i];
        if (target >= tuning.shareCap) continue;
        int transferred = values[i] * tuning.sharePercent / 100;
        if (transferred <= 0) continue;
        int raised = clampInt(target + transferred, 0, tuning.shareCap);
        if (raised != target) {
            target = raised;
            changed = true;
        }
    }
    return changed;
}

std::vector<GossipEntry> GossipLedger::entries() const {
    std::vector<GossipEntry> found;
    for (int i = 0; i < static_cast<int>(GossipKind::Count); ++i) {
        if (values[i] == 0) continue;
        found.push_back({static_cast<GossipKind>(i), values[i]});
    }
    return found;
}

int GossipLedger::entryCount() const {
    int count = 0;
    for (int value : values)
        if (value != 0) ++count;
    return count;
}

bool GossipLedger::empty() const { return entryCount() == 0; }

void GossipLedger::forget(GossipKind kind) { values[kindIndex(kind)] = 0; }

void GossipLedger::clear() {
    for (int& value : values) value = 0;
}

int ReputationView::discountPercent() const {
    return clampInt(personal / 10, -60, 60);
}

ReputationView buildReputation(const GossipLedger& personal,
                               const std::vector<const GossipLedger*>& village) {
    ReputationView view;
    view.personal = personal.reputation();
    view.majorNegative = personal.valueOf(GossipKind::MajorNegative);
    view.villagerCount = static_cast<int>(village.size());

    if (village.empty()) {
        view.village = view.personal;
        return view;
    }

    int sum = 0;
    for (const GossipLedger* ledger : village) {
        sum += ledger->reputation();
        int major = ledger->valueOf(GossipKind::MajorNegative);
        if (major > view.majorNegative) view.majorNegative = major;
    }
    view.village = sum / view.villagerCount;
    return view;
}

} // namespace vf
