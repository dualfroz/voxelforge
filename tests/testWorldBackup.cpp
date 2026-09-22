#include "testHarness.h"

#include "vf/Block.h"
#include "vf/RegionCompact.h"
#include "vf/SaveIntegrity.h"
#include "vf/SaveMigration.h"
#include "vf/WorldBackup.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

Chunk uniformChunk(Block block) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) chunk.set(x, y, z, block);
    return chunk;
}

Chunk texturedChunk(int seed) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) {
                int value = 1 + (x * 7 + y * 5 + z * 3 + seed) % 12;
                chunk.set(x, y, z, static_cast<Block>(value));
            }
    return chunk;
}

SaveChunkRecord buildRecord(const Vec3i& coord, const Chunk& blocks, SaveVersion version) {
    SaveChunkRecord record;
    record.coord = coord;
    record.blocks = blocks;
    if (versionHasLight(version)) record.light = defaultChunkLight(blocks, saveDefaultSkyLight);
    return record;
}

SaveContainer baseContainer() {
    SaveContainer container;
    container.version = SaveVersion::V4;
    container.chunks.push_back(buildRecord({0, 0, 0}, uniformChunk(Block::Stone), SaveVersion::V4));
    container.chunks.push_back(buildRecord({1, 0, 0}, uniformChunk(Block::Dirt), SaveVersion::V4));
    container.chunks.push_back(buildRecord({2, 0, 0}, uniformChunk(Block::Sand), SaveVersion::V4));
    return container;
}

void replaceChunk(SaveContainer& container, const Vec3i& coord, const Chunk& blocks) {
    for (SaveChunkRecord& record : container.chunks) {
        if (!(record.coord == coord)) continue;
        record.blocks = blocks;
        record.light = defaultChunkLight(blocks, saveDefaultSkyLight);
        return;
    }
    container.chunks.push_back(buildRecord(coord, blocks, container.version));
}

void dropChunk(SaveContainer& container, const Vec3i& coord) {
    for (std::size_t i = 0; i < container.chunks.size(); ++i) {
        if (!(container.chunks[i].coord == coord)) continue;
        container.chunks.erase(container.chunks.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

}

VF_TEST(aFullBackupRestoresTheExactBytesItCaptured) {
    ByteBuffer save = writeSaveContainer(baseContainer());
    BackupStore store;
    uint32_t id = store.capture(BackupKind::Full, save, 0);
    CHECK(id != 0u);
    CHECK(store.lastFailure().empty());

    RestoreResult restored = store.restore(id);
    CHECK(restored.ok);
    CHECK(restored.failure.empty());
    CHECK(restored.save == save);
    CHECK_EQ(restored.chunksRestored, static_cast<std::size_t>(3));
    CHECK_EQ(restored.chunksFromFull, static_cast<std::size_t>(3));
    CHECK_EQ(restored.chunksFromIncrements, static_cast<std::size_t>(0));
    CHECK_EQ(restored.incrementsApplied, static_cast<std::size_t>(0));
    CHECK_EQ(restored.chain.size(), static_cast<std::size_t>(1));
}

VF_TEST(everyPointOfABackupChainRestoresByteForByte) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);

    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);

    replaceChunk(container, {2, 0, 0}, texturedChunk(4));
    replaceChunk(container, {3, 0, 0}, uniformChunk(Block::Gravel));
    ByteBuffer save2 = writeSaveContainer(container);

    dropChunk(container, {0, 0, 0});
    ByteBuffer save3 = writeSaveContainer(container);

    BackupStore store;
    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    uint32_t id2 = store.capture(BackupKind::Incremental, save2, 2000);
    uint32_t id3 = store.capture(BackupKind::Incremental, save3, 3000);
    CHECK(id0 != 0u);
    CHECK(id1 != 0u);
    CHECK(id2 != 0u);
    CHECK(id3 != 0u);

    CHECK(store.restore(id0).save == save0);
    CHECK(store.restore(id1).save == save1);
    CHECK(store.restore(id2).save == save2);
    CHECK(store.restore(id3).save == save3);

    RestoreResult latest = store.restoreLatest();
    CHECK(latest.ok);
    CHECK(latest.save == save3);
    CHECK_EQ(latest.chain.size(), static_cast<std::size_t>(4));
    CHECK_EQ(latest.incrementsApplied, static_cast<std::size_t>(3));
    CHECK_EQ(latest.chunksRestored, static_cast<std::size_t>(3));
    CHECK_EQ(latest.chunksRemoved, static_cast<std::size_t>(1));
}

VF_TEST(anIncrementalKeepsOnlyTheChunksThatMoved) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);
    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);
    replaceChunk(container, {2, 0, 0}, texturedChunk(4));
    replaceChunk(container, {3, 0, 0}, uniformChunk(Block::Gravel));
    ByteBuffer save2 = writeSaveContainer(container);
    dropChunk(container, {0, 0, 0});
    ByteBuffer save3 = writeSaveContainer(container);

    BackupStore store;
    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    uint32_t id2 = store.capture(BackupKind::Incremental, save2, 2000);
    uint32_t id3 = store.capture(BackupKind::Incremental, save3, 3000);

    const BackupEntry* full = store.index().find(id0);
    const BackupEntry* first = store.index().find(id1);
    const BackupEntry* second = store.index().find(id2);
    const BackupEntry* third = store.index().find(id3);
    CHECK(full != nullptr);
    CHECK(first != nullptr);
    CHECK(second != nullptr);
    CHECK(third != nullptr);
    if (full == nullptr || first == nullptr || second == nullptr || third == nullptr) return;

    CHECK(full->kind == BackupKind::Full);
    CHECK_EQ(full->stored.size(), static_cast<std::size_t>(3));
    CHECK_EQ(full->state.size(), static_cast<std::size_t>(3));
    CHECK_EQ(full->parentId, static_cast<uint32_t>(0));

    CHECK(first->kind == BackupKind::Incremental);
    CHECK_EQ(first->parentId, id0);
    CHECK_EQ(first->stored.size(), static_cast<std::size_t>(1));
    CHECK(first->findStored({1, 0, 0}) != nullptr);
    CHECK(first->findStored({0, 0, 0}) == nullptr);
    CHECK(first->removed.empty());

    CHECK_EQ(second->stored.size(), static_cast<std::size_t>(2));
    CHECK(second->findStored({2, 0, 0}) != nullptr);
    CHECK(second->findStored({3, 0, 0}) != nullptr);
    CHECK_EQ(second->state.size(), static_cast<std::size_t>(4));

    CHECK(third->stored.empty());
    CHECK_EQ(third->removed.size(), static_cast<std::size_t>(1));
    CHECK(third->removed[0] == Vec3i(0, 0, 0));

    CHECK(first->storedBytes() < full->storedBytes());
    CHECK_EQ(third->storedBytes(), static_cast<std::size_t>(0));
    CHECK_EQ(store.storedBytes(),
             full->storedBytes() + first->storedBytes() + second->storedBytes());
}

VF_TEST(aChainMissingALinkIsReportedInsteadOfRestored) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);
    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);
    replaceChunk(container, {2, 0, 0}, texturedChunk(4));
    ByteBuffer save2 = writeSaveContainer(container);

    BackupStore store;
    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    uint32_t id2 = store.capture(BackupKind::Incremental, save2, 2000);
    CHECK(store.restore(id2).ok);

    CHECK(store.drop(id1));
    CHECK(!store.drop(id1));
    CHECK_EQ(store.index().count(), static_cast<std::size_t>(2));

    RestoreResult broken = store.restore(id2);
    CHECK(!broken.ok);
    CHECK(broken.save.empty());
    CHECK(broken.failure.find("chain broken") != std::string::npos);
    CHECK(broken.failure.find("#" + std::to_string(id1)) != std::string::npos);

    bool complete = true;
    std::string walkFailure;
    std::vector<uint32_t> walk = store.index().chainTo(id2, complete, walkFailure);
    CHECK(!complete);
    CHECK(walk.empty());
    CHECK(!walkFailure.empty());

    RestoreResult stillGood = store.restore(id0);
    CHECK(stillGood.ok);
    CHECK(stillGood.save == save0);

    RestoreResult unknown = store.restore(9999);
    CHECK(!unknown.ok);
    CHECK(unknown.failure.find("not in the index") != std::string::npos);
}

VF_TEST(chainWalkListsTheFullBackupFirstAndTheAskedForEntryLast) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);
    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);
    replaceChunk(container, {2, 0, 0}, texturedChunk(4));
    ByteBuffer save2 = writeSaveContainer(container);

    BackupStore store;
    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    uint32_t id2 = store.capture(BackupKind::Incremental, save2, 2000);

    bool complete = false;
    std::string walkFailure;
    std::vector<uint32_t> walk = store.index().chainTo(id2, complete, walkFailure);
    CHECK(complete);
    CHECK(walkFailure.empty());
    CHECK_EQ(walk.size(), static_cast<std::size_t>(3));
    if (walk.size() != 3) return;
    CHECK_EQ(walk[0], id0);
    CHECK_EQ(walk[1], id1);
    CHECK_EQ(walk[2], id2);
    CHECK_EQ(store.index().latestId(), id2);
}

VF_TEST(captureRefusesAFragmentedContainerAndPointsAtTheCompactor) {
    ByteBuffer save = writeSaveContainer(baseContainer());
    CHECK(updateChunkInPlace(save, buildRecord({1, 0, 0}, texturedChunk(8), SaveVersion::V4),
                             false));
    CHECK(!isCanonicalSaveLayout(save.data(), save.size()));

    BackupStore store;
    CHECK_EQ(store.capture(BackupKind::Full, save, 0), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("compact") != std::string::npos);
    CHECK_EQ(store.index().count(), static_cast<std::size_t>(0));

    RegionCompactor compactor;
    ByteBuffer tidy;
    CompactionStats stats;
    CHECK(compactor.compact(save, tidy, stats));

    uint32_t id = store.capture(BackupKind::Full, tidy, 0);
    CHECK(id != 0u);
    CHECK(store.restore(id).save == tidy);
}

VF_TEST(captureRefusesASaveTheScannerCallsDamaged) {
    ByteBuffer save = writeSaveContainer(baseContainer());
    SaveHeader header;
    CHECK(readSaveHeader(save.data(), save.size(), header));
    std::vector<SaveTableEntry> entries;
    CHECK(readSaveTable(save.data(), save.size(), header, entries));
    std::size_t payloadAt = static_cast<std::size_t>(entries[1].sectorOffset) * header.sectorBytes;
    saveWriteU32At(save.data(), save.size(), payloadAt + 8, 0u);

    BackupStore store;
    CHECK_EQ(store.capture(BackupKind::Full, save, 0), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("integrity") != std::string::npos);

    ByteBuffer rubbish(30, 0);
    CHECK_EQ(store.capture(BackupKind::Full, rubbish, 0), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("readable container") != std::string::npos);
}

VF_TEST(anIncrementalWithoutAFullBehindItIsRefused) {
    ByteBuffer save = writeSaveContainer(baseContainer());
    BackupStore store;
    CHECK_EQ(store.capture(BackupKind::Incremental, save, 0), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("no full backup") != std::string::npos);

    CHECK(store.capture(BackupKind::Full, save, 100) != 0u);
    CHECK_EQ(store.capture(BackupKind::Incremental, save, 50), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("runs behind") != std::string::npos);

    SaveContainer elsewhere = baseContainer();
    elsewhere.regionX = 4;
    for (SaveChunkRecord& record : elsewhere.chunks) record.coord.x += 64;
    ByteBuffer other = writeSaveContainer(elsewhere);
    CHECK_EQ(store.capture(BackupKind::Incremental, other, 200), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("chain covers region") != std::string::npos);

    RestoreResult empty = BackupStore{}.restoreLatest();
    CHECK(!empty.ok);
    CHECK(empty.failure.find("empty") != std::string::npos);
}

VF_TEST(captureMigratedLiftsAnOldSaveBeforeStoringIt) {
    SaveContainer legacy;
    legacy.version = SaveVersion::V1;
    legacy.chunks.push_back(buildRecord({0, 0, 0}, texturedChunk(1), SaveVersion::V1));
    legacy.chunks.push_back(buildRecord({1, 0, 0}, texturedChunk(2), SaveVersion::V1));
    ByteBuffer ancient = writeSaveContainer(legacy);

    BackupStore store;
    uint32_t id = store.captureMigrated(BackupKind::Full, ancient, 0);
    CHECK(id != 0u);
    const BackupEntry* entry = store.index().find(id);
    CHECK(entry != nullptr);
    if (entry == nullptr) return;
    CHECK(entry->version == currentSaveVersion);

    RestoreResult restored = store.restore(id);
    CHECK(restored.ok);
    SaveContainer reloaded;
    CHECK(readSaveContainer(restored.save.data(), restored.save.size(), reloaded));
    CHECK(reloaded.version == SaveVersion::V4);
    for (const SaveChunkRecord& before : legacy.chunks) {
        const SaveChunkRecord* after = reloaded.find(before.coord);
        CHECK(after != nullptr);
        if (after == nullptr) continue;
        for (std::size_t i = 0; i < before.blocks.raw().size(); ++i)
            CHECK(before.blocks.raw()[i] == after->blocks.raw()[i]);
    }

    ByteBuffer modern = writeSaveContainer(baseContainer());
    saveWriteU32At(modern.data(), modern.size(), 4, 9u);
    CHECK_EQ(store.captureMigrated(BackupKind::Full, modern, 10), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("migration") != std::string::npos);
}

VF_TEST(retentionKeepsTheLatestAndPrunesTheChainsNobodyNeeds) {
    SaveContainer container = baseContainer();
    BackupStore store;
    store.retention.keepLatest = 2;
    store.retention.dailyBuckets = 1;
    store.retention.weeklyBuckets = 1;

    std::vector<uint32_t> ids;
    std::vector<ByteBuffer> saves;
    const uint64_t ticks[6] = {0, 1000, 24000, 25000, 48000, 49000};
    const BackupKind kinds[6] = {BackupKind::Full,        BackupKind::Incremental,
                                 BackupKind::Full,        BackupKind::Incremental,
                                 BackupKind::Full,        BackupKind::Incremental};
    for (int step = 0; step < 6; ++step) {
        replaceChunk(container, {1, 0, 0}, texturedChunk(step));
        ByteBuffer save = writeSaveContainer(container);
        uint32_t id = store.capture(kinds[step], save, ticks[step]);
        CHECK(id != 0u);
        ids.push_back(id);
        saves.push_back(save);
    }
    CHECK_EQ(store.index().count(), static_cast<std::size_t>(6));

    std::vector<uint32_t> removed = store.prune(49000);
    CHECK_EQ(removed.size(), static_cast<std::size_t>(4));
    if (removed.size() == 4) {
        CHECK_EQ(removed[0], ids[0]);
        CHECK_EQ(removed[1], ids[1]);
        CHECK_EQ(removed[2], ids[2]);
        CHECK_EQ(removed[3], ids[3]);
    }
    CHECK_EQ(store.index().count(), static_cast<std::size_t>(2));
    CHECK(store.index().find(ids[4]) != nullptr);
    CHECK(store.index().find(ids[5]) != nullptr);

    RestoreResult survivor = store.restore(ids[5]);
    CHECK(survivor.ok);
    CHECK(survivor.save == saves[5]);
    CHECK(!store.restore(ids[1]).ok);
}

VF_TEST(retentionAlsoKeepsOneBackupPerRecentDay) {
    ByteBuffer save = writeSaveContainer(baseContainer());
    BackupStore store;
    store.retention.keepLatest = 1;
    store.retention.dailyBuckets = 2;
    store.retention.weeklyBuckets = 0;

    uint32_t oldest = store.capture(BackupKind::Full, save, 0);
    uint32_t sameDay = store.capture(BackupKind::Full, save, 100);
    uint32_t nextDay = store.capture(BackupKind::Full, save, 30000);
    CHECK(oldest != 0u);
    CHECK(sameDay != 0u);
    CHECK(nextDay != 0u);

    std::vector<uint32_t> removed = store.prune(30000);
    CHECK_EQ(removed.size(), static_cast<std::size_t>(1));
    if (!removed.empty()) CHECK_EQ(removed[0], oldest);
    CHECK(store.index().find(sameDay) != nullptr);
    CHECK(store.index().find(nextDay) != nullptr);
}

VF_TEST(retentionNeverPrunesAnAncestorAKeptBackupStillNeeds) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);
    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);
    replaceChunk(container, {2, 0, 0}, texturedChunk(4));
    ByteBuffer save2 = writeSaveContainer(container);

    BackupStore store;
    store.retention.keepLatest = 1;
    store.retention.dailyBuckets = 0;
    store.retention.weeklyBuckets = 0;

    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    uint32_t id2 = store.capture(BackupKind::Incremental, save2, 2000);

    std::vector<uint32_t> removed = store.prune(2000);
    CHECK(removed.empty());
    CHECK_EQ(store.index().count(), static_cast<std::size_t>(3));
    CHECK(store.index().find(id0) != nullptr);
    CHECK(store.index().find(id1) != nullptr);
    CHECK(store.restore(id2).save == save2);
}

VF_TEST(aBrokenChainCannotBeExtendedWithANewIncremental) {
    SaveContainer container = baseContainer();
    ByteBuffer save0 = writeSaveContainer(container);
    replaceChunk(container, {1, 0, 0}, texturedChunk(3));
    ByteBuffer save1 = writeSaveContainer(container);

    BackupStore store;
    uint32_t id0 = store.capture(BackupKind::Full, save0, 0);
    uint32_t id1 = store.capture(BackupKind::Incremental, save1, 1000);
    CHECK(id1 != 0u);
    CHECK(store.drop(id0));

    replaceChunk(container, {2, 0, 0}, texturedChunk(5));
    ByteBuffer save2 = writeSaveContainer(container);
    CHECK_EQ(store.capture(BackupKind::Incremental, save2, 2000), static_cast<uint32_t>(0));
    CHECK(store.lastFailure().find("broken chain") != std::string::npos);
}

VF_TEST(backupKindNamesStayReadable) {
    CHECK(std::string(backupKindName(BackupKind::Full)) == "full");
    CHECK(std::string(backupKindName(BackupKind::Incremental)) == "incremental");
}
