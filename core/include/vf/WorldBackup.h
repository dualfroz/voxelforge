#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Codec.h"
#include "Math.h"
#include "SaveIntegrity.h"
#include "SaveMigration.h"

namespace vf {

// In-memory backup chain for a region save container.
//
// A full backup keeps every chunk payload. An incremental keeps only the
// payloads whose checksum moved since the entry before it, plus the list of
// chunks that disappeared, so a long series of small edits costs a handful of
// records rather than a copy of the world. Every entry also records the
// checksum of every chunk that was live at that moment, which is what makes a
// restore verifiable rather than hopeful.
//
// Time is always a tick handed in by the caller. Nothing here reads a clock, so
// the same sequence of captures and prunes gives the same chain every run.
//
// Restores go back through the shared canonical packer, so a restored buffer is
// byte for byte the buffer that was captured. That only holds for a canonical
// layout, which is why capture refuses a fragmented container and points at the
// compactor instead.

enum class BackupKind : uint8_t {
    Full = 0,
    Incremental = 1,
};

const char* backupKindName(BackupKind kind);

// One chunk inside an entry. In `stored` the payload is present and the
// checksum describes it; in `state` the payload is left empty and only the
// checksum is carried, because the state list covers the whole world.
struct BackupChunk {
    Vec3i coord;
    uint32_t checksum = 0;
    ByteBuffer payload;
};

struct BackupEntry {
    uint32_t id = 0;
    uint32_t parentId = 0;
    BackupKind kind = BackupKind::Full;
    uint64_t tick = 0;
    SaveVersion version = currentSaveVersion;
    int regionX = 0;
    int regionZ = 0;
    uint32_t sectorBytes = saveSectorBytes;

    std::vector<BackupChunk> stored;
    std::vector<Vec3i> removed;
    std::vector<BackupChunk> state;

    std::size_t storedBytes() const;
    const BackupChunk* findStored(const Vec3i& coord) const;
    const BackupChunk* findState(const Vec3i& coord) const;
};

// The chain itself: one full backup followed by the incrementals hanging off
// it. Walking a chain is the only supported way to reach an entry, and a walk
// that cannot reach a full backup is reported instead of guessed around.
class BackupIndex {
public:
    std::vector<BackupEntry> entries;

    const BackupEntry* find(uint32_t id) const;
    BackupEntry* find(uint32_t id);
    uint32_t latestId() const;
    std::size_t count() const { return entries.size(); }
    std::size_t storedBytes() const;

    // Ancestors first, `id` last. On a break `complete` is false and `failure`
    // says which link is missing.
    std::vector<uint32_t> chainTo(uint32_t id, bool& complete, std::string& failure) const;
};

struct RestoreResult {
    bool ok = false;
    ByteBuffer save;
    uint32_t fromId = 0;
    std::vector<uint32_t> chain;
    std::size_t chunksRestored = 0;
    std::size_t chunksFromFull = 0;
    std::size_t chunksFromIncrements = 0;
    std::size_t chunksRemoved = 0;
    std::size_t incrementsApplied = 0;
    std::string failure;
};

// Keep the newest `keepLatest` entries, plus the newest entry of each of the
// last `dailyBuckets` days and `weeklyBuckets` weeks, plus every ancestor those
// entries need. Days and weeks are measured in ticks against the tick passed to
// prune, never against a system clock.
struct RetentionPolicy {
    std::size_t keepLatest = 3;
    std::size_t dailyBuckets = 1;
    std::size_t weeklyBuckets = 1;
    uint64_t dayTicks = 24000;
    uint64_t weekTicks = 168000;
};

class BackupStore {
public:
    RetentionPolicy retention;
    SaveScanner scanner;

    uint32_t capture(BackupKind kind, const ByteBuffer& save, uint64_t tick);

    // Lifts an older container to the current version first, so a chain never
    // mixes formats. Returns 0 and sets lastFailure if the migration refuses.
    uint32_t captureMigrated(BackupKind kind, const ByteBuffer& save, uint64_t tick);

    RestoreResult restore(uint32_t id) const;
    RestoreResult restoreLatest() const;

    // Deletes an entry without touching the rest, the way a stray file removal
    // would. Chains that depended on it become detectably broken.
    bool drop(uint32_t id);

    std::vector<uint32_t> prune(uint64_t nowTick);

    const BackupIndex& index() const { return chain; }
    const std::string& lastFailure() const { return failure; }
    std::size_t storedBytes() const { return chain.storedBytes(); }

private:
    BackupIndex chain;
    uint32_t nextId = 1;
    std::string failure;
};

} // namespace vf
