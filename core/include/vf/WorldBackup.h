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

enum class BackupKind : uint8_t {
    Full = 0,
    Incremental = 1,
};

const char* backupKindName(BackupKind kind);

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

class BackupIndex {
public:
    std::vector<BackupEntry> entries;

    const BackupEntry* find(uint32_t id) const;
    BackupEntry* find(uint32_t id);
    uint32_t latestId() const;
    std::size_t count() const { return entries.size(); }
    std::size_t storedBytes() const;

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

    uint32_t captureMigrated(BackupKind kind, const ByteBuffer& save, uint64_t tick);

    RestoreResult restore(uint32_t id) const;
    RestoreResult restoreLatest() const;

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

}
