#include "vf/WorldBackup.h"

#include <algorithm>

namespace vf {

namespace {

struct LiveChunk {
    Vec3i coord;
    uint32_t checksum = 0;
    ByteBuffer payload;
};

std::size_t findLive(const std::vector<LiveChunk>& live, const Vec3i& coord) {
    for (std::size_t i = 0; i < live.size(); ++i)
        if (live[i].coord == coord) return i;
    return live.size();
}

void insertLive(std::vector<LiveChunk>& live, const Vec3i& coord, uint32_t checksum,
                const ByteBuffer& payload) {
    std::size_t at = findLive(live, coord);
    if (at != live.size()) {
        live[at].checksum = checksum;
        live[at].payload = payload;
        return;
    }
    LiveChunk fresh;
    fresh.coord = coord;
    fresh.checksum = checksum;
    fresh.payload = payload;
    live.push_back(std::move(fresh));
}

void eraseLive(std::vector<LiveChunk>& live, const Vec3i& coord) {
    std::size_t at = findLive(live, coord);
    if (at != live.size()) live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
}

// Pulls every chunk of a container out as (coordinate, checksum, payload
// bytes), in canonical order.
bool sliceContainer(const ByteBuffer& save, SaveHeader& header, std::vector<LiveChunk>& out) {
    if (!readSaveHeader(save.data(), save.size(), header)) return false;
    if (header.magic != saveMagic || !isKnownSaveVersion(header.version)) return false;
    if (header.sectorBytes == 0) return false;
    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(save.data(), save.size(), header, entries)) return false;

    out.clear();
    out.reserve(entries.size());
    for (const SaveTableEntry& entry : entries) {
        std::size_t at = static_cast<std::size_t>(entry.sectorOffset) * header.sectorBytes;
        if (at + entry.payloadBytes > save.size()) return false;
        LiveChunk chunk;
        chunk.coord = entry.coord;
        chunk.payload.assign(save.begin() + static_cast<std::ptrdiff_t>(at),
                             save.begin() + static_cast<std::ptrdiff_t>(at + entry.payloadBytes));
        chunk.checksum = fnv1a32(chunk.payload.data(), chunk.payload.size());
        out.push_back(std::move(chunk));
    }
    return true;
}

std::string idText(uint32_t id) { return "#" + std::to_string(id); }

} // namespace

const char* backupKindName(BackupKind kind) {
    switch (kind) {
        case BackupKind::Full: return "full";
        case BackupKind::Incremental: return "incremental";
    }
    return "unknown";
}

std::size_t BackupEntry::storedBytes() const {
    std::size_t total = 0;
    for (const BackupChunk& chunk : stored) total += chunk.payload.size();
    return total;
}

const BackupChunk* BackupEntry::findStored(const Vec3i& coord) const {
    for (const BackupChunk& chunk : stored)
        if (chunk.coord == coord) return &chunk;
    return nullptr;
}

const BackupChunk* BackupEntry::findState(const Vec3i& coord) const {
    for (const BackupChunk& chunk : state)
        if (chunk.coord == coord) return &chunk;
    return nullptr;
}

const BackupEntry* BackupIndex::find(uint32_t id) const {
    for (const BackupEntry& entry : entries)
        if (entry.id == id) return &entry;
    return nullptr;
}

BackupEntry* BackupIndex::find(uint32_t id) {
    for (BackupEntry& entry : entries)
        if (entry.id == id) return &entry;
    return nullptr;
}

uint32_t BackupIndex::latestId() const {
    uint32_t best = 0;
    for (const BackupEntry& entry : entries)
        if (entry.id > best) best = entry.id;
    return best;
}

std::size_t BackupIndex::storedBytes() const {
    std::size_t total = 0;
    for (const BackupEntry& entry : entries) total += entry.storedBytes();
    return total;
}

std::vector<uint32_t> BackupIndex::chainTo(uint32_t id, bool& complete,
                                           std::string& failure) const {
    std::vector<uint32_t> walk;
    complete = false;
    failure.clear();

    const BackupEntry* entry = find(id);
    if (entry == nullptr) {
        failure = "backup " + idText(id) + " is not in the index";
        return walk;
    }

    std::size_t guard = entries.size() + 1;
    while (guard-- > 0) {
        walk.push_back(entry->id);
        if (entry->kind == BackupKind::Full) {
            std::reverse(walk.begin(), walk.end());
            complete = true;
            return walk;
        }
        const BackupEntry* parent = find(entry->parentId);
        if (parent == nullptr) {
            failure = "chain broken: incremental " + idText(entry->id) + " expects parent " +
                      idText(entry->parentId) + ", which is not in the index";
            walk.clear();
            return walk;
        }
        entry = parent;
    }
    failure = "chain broken: the parent links from " + idText(id) + " never reach a full backup";
    walk.clear();
    return walk;
}

uint32_t BackupStore::capture(BackupKind kind, const ByteBuffer& save, uint64_t tick) {
    failure.clear();

    IntegrityReport report = scanner.scan(save.data(), save.size());
    if (!report.headerReadable) {
        failure = "save is not a readable container, nothing to back up";
        return 0;
    }
    if (report.countAtLeast(IssueSeverity::Recoverable) != 0) {
        failure = "save reports " + std::to_string(report.countAtLeast(IssueSeverity::Recoverable)) +
                  " unresolved integrity findings; repair it before backing it up";
        return 0;
    }
    if (!isCanonicalSaveLayout(save.data(), save.size())) {
        failure =
            "save layout is fragmented; compact it first, a payload level backup cannot reproduce "
            "hole placement byte for byte";
        return 0;
    }

    SaveHeader header;
    std::vector<LiveChunk> current;
    if (!sliceContainer(save, header, current)) {
        failure = "save table does not address its own payloads";
        return 0;
    }

    BackupEntry entry;
    entry.kind = kind;
    entry.tick = tick;
    entry.version = static_cast<SaveVersion>(header.version);
    entry.regionX = header.regionX;
    entry.regionZ = header.regionZ;
    entry.sectorBytes = header.sectorBytes;

    const BackupEntry* parent = nullptr;
    if (kind == BackupKind::Incremental) {
        uint32_t parentId = chain.latestId();
        parent = chain.find(parentId);
        if (parent == nullptr) {
            failure = "no full backup to build on; capture a full backup first";
            return 0;
        }
        bool complete = false;
        std::string walkFailure;
        chain.chainTo(parentId, complete, walkFailure);
        if (!complete) {
            failure = "cannot extend a broken chain: " + walkFailure;
            return 0;
        }
        if (parent->version != entry.version || parent->regionX != entry.regionX ||
            parent->regionZ != entry.regionZ) {
            failure = "chain covers region (" + std::to_string(parent->regionX) + ", " +
                      std::to_string(parent->regionZ) + ") at " + saveVersionName(parent->version) +
                      ", the save is region (" + std::to_string(entry.regionX) + ", " +
                      std::to_string(entry.regionZ) + ") at " + saveVersionName(entry.version);
            return 0;
        }
        if (tick < parent->tick) {
            failure = "backup tick " + std::to_string(tick) + " runs behind parent " +
                      idText(parent->id) + " at tick " + std::to_string(parent->tick);
            return 0;
        }
        entry.parentId = parentId;
    }

    for (const LiveChunk& chunk : current) {
        BackupChunk stamp;
        stamp.coord = chunk.coord;
        stamp.checksum = chunk.checksum;
        entry.state.push_back(stamp);

        bool changed = true;
        if (parent != nullptr) {
            const BackupChunk* before = parent->findState(chunk.coord);
            changed = before == nullptr || before->checksum != chunk.checksum;
        }
        if (!changed) continue;
        BackupChunk written;
        written.coord = chunk.coord;
        written.checksum = chunk.checksum;
        written.payload = chunk.payload;
        entry.stored.push_back(std::move(written));
    }

    if (parent != nullptr) {
        for (const BackupChunk& before : parent->state) {
            bool stillHere = false;
            for (const LiveChunk& chunk : current)
                if (chunk.coord == before.coord) stillHere = true;
            if (!stillHere) entry.removed.push_back(before.coord);
        }
    }

    entry.id = nextId++;
    chain.entries.push_back(std::move(entry));
    return chain.entries.back().id;
}

uint32_t BackupStore::captureMigrated(BackupKind kind, const ByteBuffer& save, uint64_t tick) {
    failure.clear();
    SaveMigrator migrator;
    MigrationResult migrated = migrator.migrateToCurrent(save);
    if (!migrated.ok) {
        failure = "migration to " + std::string(saveVersionName(currentSaveVersion)) +
                  " failed: " + migrated.failure;
        return 0;
    }
    return capture(kind, migrated.output, tick);
}

RestoreResult BackupStore::restore(uint32_t id) const {
    RestoreResult result;
    result.fromId = id;

    bool complete = false;
    std::string walkFailure;
    std::vector<uint32_t> walk = chain.chainTo(id, complete, walkFailure);
    if (!complete) {
        result.failure = walkFailure;
        return result;
    }
    result.chain = walk;

    std::vector<LiveChunk> live;
    const BackupEntry* target = chain.find(id);
    for (std::size_t step = 0; step < walk.size(); ++step) {
        const BackupEntry* entry = chain.find(walk[step]);
        for (const Vec3i& coord : entry->removed) {
            eraseLive(live, coord);
            ++result.chunksRemoved;
        }
        for (const BackupChunk& chunk : entry->stored) {
            insertLive(live, chunk.coord, chunk.checksum, chunk.payload);
            if (entry->kind == BackupKind::Full) ++result.chunksFromFull;
            else ++result.chunksFromIncrements;
        }
        if (entry->kind == BackupKind::Incremental) ++result.incrementsApplied;
    }

    if (live.size() != target->state.size()) {
        result.failure = "restored " + std::to_string(live.size()) + " chunks but backup " +
                         idText(id) + " recorded " + std::to_string(target->state.size());
        return result;
    }
    for (const BackupChunk& stamp : target->state) {
        std::size_t at = findLive(live, stamp.coord);
        if (at == live.size() || live[at].checksum != stamp.checksum) {
            result.failure = "restored chunk (" + std::to_string(stamp.coord.x) + ", " +
                             std::to_string(stamp.coord.y) + ", " + std::to_string(stamp.coord.z) +
                             ") does not match the checksum backup " + idText(id) + " recorded";
            return result;
        }
    }

    std::sort(live.begin(), live.end(), [](const LiveChunk& a, const LiveChunk& b) {
        return saveCoordLess(a.coord, b.coord);
    });
    std::vector<Vec3i> coords;
    std::vector<ByteBuffer> payloads;
    coords.reserve(live.size());
    payloads.reserve(live.size());
    for (LiveChunk& chunk : live) {
        coords.push_back(chunk.coord);
        payloads.push_back(std::move(chunk.payload));
    }

    result.save = packSaveContainer(target->version, target->regionX, target->regionZ,
                                    target->sectorBytes, coords, payloads, nullptr);
    result.chunksRestored = coords.size();
    result.ok = true;
    return result;
}

RestoreResult BackupStore::restoreLatest() const {
    uint32_t latest = chain.latestId();
    if (latest == 0) {
        RestoreResult result;
        result.failure = "the backup index is empty";
        return result;
    }
    return restore(latest);
}

bool BackupStore::drop(uint32_t id) {
    for (std::size_t i = 0; i < chain.entries.size(); ++i) {
        if (chain.entries[i].id != id) continue;
        chain.entries.erase(chain.entries.begin() + static_cast<std::ptrdiff_t>(i));
        return true;
    }
    return false;
}

std::vector<uint32_t> BackupStore::prune(uint64_t nowTick) {
    failure.clear();
    std::vector<uint32_t> keep;
    auto markKept = [&](uint32_t id) {
        for (uint32_t held : keep)
            if (held == id) return;
        keep.push_back(id);
    };

    std::vector<uint32_t> byAge;
    for (const BackupEntry& entry : chain.entries) byAge.push_back(entry.id);
    std::sort(byAge.begin(), byAge.end(), [](uint32_t a, uint32_t b) { return a > b; });
    for (std::size_t i = 0; i < byAge.size() && i < retention.keepLatest; ++i) markKept(byAge[i]);

    auto keepNewestPerBucket = [&](uint64_t bucketTicks, std::size_t bucketCount) {
        if (bucketTicks == 0 || bucketCount == 0) return;
        uint64_t currentBucket = nowTick / bucketTicks;
        for (std::size_t offset = 0; offset < bucketCount; ++offset) {
            if (currentBucket < offset) break;
            uint64_t wanted = currentBucket - offset;
            uint32_t best = 0;
            uint64_t bestTick = 0;
            for (const BackupEntry& entry : chain.entries) {
                if (entry.tick / bucketTicks != wanted) continue;
                if (best == 0 || entry.tick > bestTick ||
                    (entry.tick == bestTick && entry.id > best)) {
                    best = entry.id;
                    bestTick = entry.tick;
                }
            }
            if (best != 0) markKept(best);
        }
    };
    keepNewestPerBucket(retention.dayTicks, retention.dailyBuckets);
    keepNewestPerBucket(retention.weekTicks, retention.weeklyBuckets);

    std::size_t scanned = 0;
    while (scanned < keep.size()) {
        const BackupEntry* entry = chain.find(keep[scanned]);
        ++scanned;
        if (entry == nullptr || entry->kind == BackupKind::Full) continue;
        markKept(entry->parentId);
    }

    std::vector<uint32_t> removed;
    std::vector<BackupEntry> survivors;
    for (BackupEntry& entry : chain.entries) {
        bool kept = false;
        for (uint32_t held : keep)
            if (held == entry.id) kept = true;
        if (kept) survivors.push_back(std::move(entry));
        else removed.push_back(entry.id);
    }
    chain.entries = std::move(survivors);
    std::sort(removed.begin(), removed.end());
    return removed;
}

} // namespace vf
