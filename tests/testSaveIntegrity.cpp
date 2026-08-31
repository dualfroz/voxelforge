#include "testHarness.h"

#include "vf/Block.h"
#include "vf/SaveIntegrity.h"
#include "vf/SaveMigration.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

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

Chunk uniformChunk(Block block) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) chunk.set(x, y, z, block);
    return chunk;
}

SaveChunkRecord buildRecord(const Vec3i& coord, const Chunk& blocks, SaveVersion version) {
    SaveChunkRecord record;
    record.coord = coord;
    record.blocks = blocks;
    if (versionHasLight(version)) record.light = defaultChunkLight(blocks, saveDefaultSkyLight);
    return record;
}

SaveContainer buildContainer(SaveVersion version, int chunkTotal) {
    SaveContainer container;
    container.version = version;
    for (int i = 0; i < chunkTotal; ++i)
        container.chunks.push_back(
            buildRecord(Vec3i{i, 0, i * 2}, texturedChunk(i), version));
    return container;
}

struct EntrySlot {
    std::size_t entryAt = 0;
    std::size_t payloadAt = 0;
    std::size_t payloadBytes = 0;
};

EntrySlot slotOf(const ByteBuffer& buffer, std::size_t index) {
    EntrySlot slot;
    SaveHeader header;
    if (!readSaveHeader(buffer.data(), buffer.size(), header)) return slot;
    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(buffer.data(), buffer.size(), header, entries)) return slot;
    if (index >= entries.size()) return slot;
    slot.entryAt = saveHeaderBytes + index * saveEntryBytes;
    slot.payloadAt = static_cast<std::size_t>(entries[index].sectorOffset) * header.sectorBytes;
    slot.payloadBytes = entries[index].payloadBytes;
    return slot;
}

void refreshTableChecksum(ByteBuffer& buffer) {
    SaveHeader header;
    if (!readSaveHeader(buffer.data(), buffer.size(), header)) return;
    if (!versionHasChecksum(static_cast<SaveVersion>(header.version))) return;
    uint32_t checksum = fnv1a32(buffer.data() + saveHeaderBytes, saveTableBytes(header));
    saveWriteU32At(buffer.data(), buffer.size(), 24, checksum);
}

bool sameBlocks(const Chunk& a, const Chunk& b) {
    for (std::size_t i = 0; i < a.raw().size(); ++i)
        if (a.raw()[i] != b.raw()[i]) return false;
    return true;
}

} // namespace

VF_TEST(scannerFindsNothingWrongWithAFreshContainer) {
    SaveContainer container = buildContainer(SaveVersion::V4, 4);
    ByteBuffer buffer = writeSaveContainer(container);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK(report.clean());
    CHECK(report.headerReadable);
    CHECK(!report.fatal());
    CHECK(!report.repairable());
    CHECK(report.version == SaveVersion::V4);
    CHECK_EQ(report.declaredChunks, static_cast<std::size_t>(4));
    CHECK_EQ(report.tableEntries, static_cast<std::size_t>(4));
    CHECK_EQ(report.readableChunks, static_cast<std::size_t>(4));
    CHECK_EQ(report.bufferBytes, buffer.size());
}

VF_TEST(aBufferTooShortForAHeaderIsFatal) {
    ByteBuffer stump(11, 0);
    SaveScanner scanner;
    IntegrityReport report = scanner.scan(stump);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::TruncatedBuffer);
    CHECK(report.issues[0].severity == IssueSeverity::Fatal);
    CHECK(!report.headerReadable);
    CHECK(report.fatal());
}

VF_TEST(aWrongMagicIsFatalAtTheStartOfTheBuffer) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 2));
    saveWriteU32At(buffer.data(), buffer.size(), 0, 0x4E4F5045u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::BadMagic);
    CHECK(report.issues[0].severity == IssueSeverity::Fatal);
    CHECK_EQ(report.issues[0].byteOffset, static_cast<std::size_t>(0));
    CHECK(!report.headerReadable);
}

VF_TEST(anUnknownFormatVersionIsFatalAtTheVersionField) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 2));
    saveWriteU32At(buffer.data(), buffer.size(), 4, 9u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::UnknownVersion);
    CHECK(report.issues[0].severity == IssueSeverity::Fatal);
    CHECK_EQ(report.issues[0].byteOffset, static_cast<std::size_t>(4));
    CHECK(report.issues[0].detail.find("v1..v4") != std::string::npos);
}

VF_TEST(aStaleRecordChecksumIsRecoverableAtTheEndOfTheRecord) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 1);
    std::size_t checksumAt = slot.payloadAt + slot.payloadBytes - 4;
    buffer[checksumAt] = static_cast<uint8_t>(buffer[checksumAt] ^ 0x01u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::ChecksumMismatch);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, checksumAt);
    CHECK_EQ(report.issues[0].entryIndex, 1);
    CHECK(report.issues[0].hasChunkCoord);
    CHECK(report.repairable());
}

VF_TEST(aTruncatedBufferIsRecoverableAtTheCutOffPayload) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot last = slotOf(buffer, 2);
    buffer.resize(buffer.size() - saveSectorBytes);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::TruncatedBuffer);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, last.payloadAt);
    CHECK_EQ(report.issues[0].entryIndex, 2);
    CHECK_EQ(report.readableChunks, static_cast<std::size_t>(2));
}

VF_TEST(aHeaderPromisingMoreChunksThanFitIsRecoverable) {
    SaveContainer empty;
    empty.version = SaveVersion::V4;
    empty.sectorBytes = 64;
    ByteBuffer buffer = writeSaveContainer(empty);
    CHECK_EQ(buffer.size(), static_cast<std::size_t>(64));
    saveWriteU32At(buffer.data(), buffer.size(), 16, 3u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.countOf(IntegrityIssueKind::ChunkCountOverrun), static_cast<std::size_t>(1));
    const IntegrityIssue* issue = report.firstOf(IntegrityIssueKind::ChunkCountOverrun);
    CHECK(issue != nullptr);
    if (issue == nullptr) return;
    CHECK(issue->severity == IssueSeverity::Recoverable);
    CHECK_EQ(issue->byteOffset, static_cast<std::size_t>(16));
    CHECK_EQ(report.declaredChunks, static_cast<std::size_t>(3));
    CHECK_EQ(report.tableEntries, static_cast<std::size_t>(1));
}

VF_TEST(aChunkFiledUnderTheWrongRegionIsRecoverable) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 1);
    saveWriteU32At(buffer.data(), buffer.size(), slot.entryAt, static_cast<uint32_t>(100));
    refreshTableChecksum(buffer);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::ChunkOutsideRegion);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, slot.entryAt);
    CHECK_EQ(report.issues[0].entryIndex, 1);
    CHECK_EQ(report.issues[0].chunkCoord.x, 100);
    CHECK_EQ(report.readableChunks, static_cast<std::size_t>(2));
}

VF_TEST(aChunkAboveTheWorldCeilingIsRecoverable) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 0);
    saveWriteU32At(buffer.data(), buffer.size(), slot.entryAt + 4, static_cast<uint32_t>(40));
    refreshTableChecksum(buffer);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::ChunkHeightOutOfRange);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, slot.entryAt + 4);
    CHECK_EQ(report.issues[0].entryIndex, 0);
    CHECK(report.issues[0].detail.find("-4..19") != std::string::npos);
}

VF_TEST(anEntryPointingAtNothingIsAnOrphan) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 1);
    saveWriteU32At(buffer.data(), buffer.size(), slot.entryAt + 16, 0u);
    refreshTableChecksum(buffer);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::OrphanEntry);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, slot.entryAt + 12);
    CHECK_EQ(report.issues[0].entryIndex, 1);
    CHECK_EQ(report.readableChunks, static_cast<std::size_t>(2));
}

VF_TEST(twoEntriesSharingSectorsAreReportedAsOverlapping) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot first = slotOf(buffer, 0);
    EntrySlot second = slotOf(buffer, 1);
    uint32_t offset = 0, count = 0, bytes = 0;
    CHECK(saveReadU32At(buffer.data(), buffer.size(), first.entryAt + 12, offset));
    CHECK(saveReadU32At(buffer.data(), buffer.size(), first.entryAt + 16, count));
    CHECK(saveReadU32At(buffer.data(), buffer.size(), first.entryAt + 20, bytes));
    saveWriteU32At(buffer.data(), buffer.size(), second.entryAt + 12, offset);
    saveWriteU32At(buffer.data(), buffer.size(), second.entryAt + 16, count);
    saveWriteU32At(buffer.data(), buffer.size(), second.entryAt + 20, bytes);
    refreshTableChecksum(buffer);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.countOf(IntegrityIssueKind::OverlappingEntries), static_cast<std::size_t>(1));
    const IntegrityIssue* issue = report.firstOf(IntegrityIssueKind::OverlappingEntries);
    CHECK(issue != nullptr);
    if (issue == nullptr) return;
    CHECK(issue->severity == IssueSeverity::Recoverable);
    CHECK_EQ(issue->entryIndex, 1);
    CHECK_EQ(issue->byteOffset, second.entryAt + 12);
}

VF_TEST(aBlockIndexOutsideThePaletteIsRecoverable) {
    SaveContainer container;
    container.version = SaveVersion::V2;
    Chunk blocks = uniformChunk(Block::Stone);
    blocks.set(1, 0, 0, Block::Grass);
    container.chunks.push_back(buildRecord(Vec3i{0, 0, 0}, blocks, SaveVersion::V2));
    container.chunks.push_back(
        buildRecord(Vec3i{1, 0, 0}, uniformChunk(Block::Dirt), SaveVersion::V2));
    ByteBuffer buffer = writeSaveContainer(container);

    EntrySlot slot = slotOf(buffer, 0);
    std::size_t indicesAt = slot.payloadAt + 4 + 2 + 4;
    buffer[indicesAt + 5] = 200;

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::PaletteIndexOutOfRange);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, slot.payloadAt);
    CHECK_EQ(report.issues[0].entryIndex, 0);
    CHECK(report.issues[0].detail.find("200") != std::string::npos);
}

VF_TEST(aLightPlaneOfTheWrongLengthIsRecoverable) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    SaveChunkRecord record =
        buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Stone), SaveVersion::V4);
    record.light.resize(100);
    container.chunks.push_back(std::move(record));
    ByteBuffer buffer = writeSaveContainer(container);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::LightingInconsistent);
    CHECK(report.issues[0].severity == IssueSeverity::Recoverable);
    CHECK_EQ(report.issues[0].byteOffset, slotOf(buffer, 0).payloadAt);
    CHECK(report.issues[0].detail.find("100") != std::string::npos);
}

VF_TEST(lightThatDisagreesWithItsBlocksIsOnlyAWarning) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    SaveChunkRecord record =
        buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Stone), SaveVersion::V4);
    record.light[0] = 0xF0;
    record.light[1] = 0xF0;
    container.chunks.push_back(std::move(record));
    ByteBuffer buffer = writeSaveContainer(container);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::LightingInconsistent);
    CHECK(report.issues[0].severity == IssueSeverity::Warning);
    CHECK(report.issues[0].detail.find("2 light cells") != std::string::npos);
    CHECK(!report.repairable());
    CHECK_EQ(report.countAtLeast(IssueSeverity::Recoverable), static_cast<std::size_t>(0));
    CHECK_EQ(report.countAtLeast(IssueSeverity::Warning), static_cast<std::size_t>(1));
}

VF_TEST(bytesPastTheLastSectorRunAreOnlyInformational) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 2));
    std::size_t before = buffer.size();
    buffer.resize(before + 300, 0);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.issues.size(), static_cast<std::size_t>(1));
    CHECK(report.issues[0].kind == IntegrityIssueKind::TrailingBytes);
    CHECK(report.issues[0].severity == IssueSeverity::Info);
    CHECK_EQ(report.issues[0].byteOffset, before);
    CHECK(!report.repairable());
    CHECK(!report.fatal());
}

VF_TEST(aRecordThatDoesNotDecodeIsRecoverable) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 1);
    saveWriteU32At(buffer.data(), buffer.size(), slot.payloadAt + 8, 0u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    CHECK_EQ(report.countOf(IntegrityIssueKind::MalformedRecord), static_cast<std::size_t>(1));
    const IntegrityIssue* issue = report.firstOf(IntegrityIssueKind::MalformedRecord);
    CHECK(issue != nullptr);
    if (issue == nullptr) return;
    CHECK(issue->severity == IssueSeverity::Recoverable);
    CHECK_EQ(issue->byteOffset, slot.payloadAt);
    CHECK_EQ(issue->entryIndex, 1);
}

VF_TEST(repairLeavesEveryHealthyChunkByteIdentical) {
    SaveContainer container = buildContainer(SaveVersion::V4, 4);
    ByteBuffer healthy = writeSaveContainer(container);

    ByteBuffer damaged = healthy;
    EntrySlot slot = slotOf(damaged, 2);
    saveWriteU32At(damaged.data(), damaged.size(), slot.payloadAt + 8, 0u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(damaged);
    RepairPlan plan = scanner.planRepair(damaged.data(), damaged.size(), report);
    CHECK(plan.possible);
    CHECK_EQ(plan.countOf(RepairActionKind::DropChunk), static_cast<std::size_t>(1));

    RepairResult repaired = scanner.repair(damaged.data(), damaged.size(), plan);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.chunksDropped, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.chunksPreserved, static_cast<std::size_t>(3));
    CHECK_EQ(repaired.bytesLost, slot.payloadBytes);

    const Vec3i survivors[3] = {{0, 0, 0}, {1, 0, 2}, {3, 0, 6}};
    for (const Vec3i& coord : survivors) {
        ByteBuffer before;
        ByteBuffer after;
        CHECK(readChunkPayloadBytes(healthy.data(), healthy.size(), coord, before));
        CHECK(readChunkPayloadBytes(repaired.output.data(), repaired.output.size(), coord, after));
        CHECK(before == after);
    }
    ByteBuffer gone;
    CHECK(!readChunkPayloadBytes(repaired.output.data(), repaired.output.size(), Vec3i{2, 0, 4},
                                 gone));
    CHECK(scanner.scan(repaired.output).clean());
    CHECK(isCanonicalSaveLayout(repaired.output.data(), repaired.output.size()));
}

VF_TEST(repairRewritesAStaleChecksumBackToTheOriginalBytes) {
    ByteBuffer healthy = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    ByteBuffer damaged = healthy;
    EntrySlot slot = slotOf(damaged, 1);
    std::size_t checksumAt = slot.payloadAt + slot.payloadBytes - 4;
    damaged[checksumAt] = static_cast<uint8_t>(damaged[checksumAt] ^ 0xFFu);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(damaged);
    RepairPlan plan = scanner.planRepair(damaged.data(), damaged.size(), report);
    CHECK(plan.possible);
    CHECK_EQ(plan.countOf(RepairActionKind::RecomputeChecksum), static_cast<std::size_t>(1));

    RepairResult repaired = scanner.repair(damaged.data(), damaged.size(), plan);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.checksumsRewritten, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.chunksDropped, static_cast<std::size_t>(0));
    CHECK_EQ(repaired.chunksPreserved, static_cast<std::size_t>(3));
    CHECK_EQ(repaired.bytesLost, static_cast<std::size_t>(0));
    CHECK(repaired.output == healthy);
}

VF_TEST(repairRebuildsAChunkWhoseIndicesLeftThePalette) {
    SaveContainer container;
    container.version = SaveVersion::V2;
    Chunk blocks = uniformChunk(Block::Stone);
    blocks.set(1, 0, 0, Block::Grass);
    container.chunks.push_back(buildRecord(Vec3i{1, 2, 1}, blocks, SaveVersion::V2));
    container.chunks.push_back(
        buildRecord(Vec3i{2, 2, 1}, uniformChunk(Block::Dirt), SaveVersion::V2));
    ByteBuffer buffer = writeSaveContainer(container);

    EntrySlot slot = slotOf(buffer, 0);
    buffer[slot.payloadAt + 10 + 5] = 200;

    SaveScanner scanner;
    scanner.worldSeed = 0x5EED1234u;
    IntegrityReport report = scanner.scan(buffer);
    RepairPlan plan = scanner.planRepair(buffer.data(), buffer.size(), report);
    CHECK(plan.possible);
    CHECK_EQ(plan.countOf(RepairActionKind::RegenerateChunk), static_cast<std::size_t>(1));
    CHECK_EQ(plan.worldSeed, static_cast<uint64_t>(0x5EED1234u));

    RepairResult repaired = scanner.repair(buffer.data(), buffer.size(), plan);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.chunksRegenerated, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.chunksPreserved, static_cast<std::size_t>(1));
    CHECK(repaired.bytesLost > 0u);

    SaveContainer rebuilt;
    CHECK(readSaveContainer(repaired.output.data(), repaired.output.size(), rebuilt));
    CHECK_EQ(rebuilt.chunks.size(), static_cast<std::size_t>(2));
    const SaveChunkRecord* restored = rebuilt.find({1, 2, 1});
    CHECK(restored != nullptr);
    if (restored != nullptr)
        CHECK(sameBlocks(restored->blocks,
                         regenerateChunkFromSeed(0x5EED1234u, Vec3i{1, 2, 1})));
    CHECK(scanner.scan(repaired.output).clean());
}

VF_TEST(repairDropsInsteadOfRebuildingWhenRegenerationIsTurnedOff) {
    SaveContainer container;
    container.version = SaveVersion::V2;
    Chunk blocks = uniformChunk(Block::Stone);
    blocks.set(1, 0, 0, Block::Grass);
    container.chunks.push_back(buildRecord(Vec3i{1, 2, 1}, blocks, SaveVersion::V2));
    container.chunks.push_back(
        buildRecord(Vec3i{2, 2, 1}, uniformChunk(Block::Dirt), SaveVersion::V2));
    ByteBuffer buffer = writeSaveContainer(container);
    buffer[slotOf(buffer, 0).payloadAt + 10 + 5] = 200;

    SaveScanner scanner;
    scanner.allowRegeneration = false;
    IntegrityReport report = scanner.scan(buffer);
    RepairPlan plan = scanner.planRepair(buffer.data(), buffer.size(), report);
    CHECK_EQ(plan.countOf(RepairActionKind::RegenerateChunk), static_cast<std::size_t>(0));
    CHECK_EQ(plan.countOf(RepairActionKind::DropChunk), static_cast<std::size_t>(1));

    RepairResult repaired = scanner.repair(buffer.data(), buffer.size(), plan);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.chunksRegenerated, static_cast<std::size_t>(0));
    CHECK_EQ(repaired.chunksDropped, static_cast<std::size_t>(1));
}

VF_TEST(repairRemovesAnOrphanEntryAndKeepsTheRest) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    EntrySlot slot = slotOf(buffer, 1);
    saveWriteU32At(buffer.data(), buffer.size(), slot.entryAt + 16, 0u);
    refreshTableChecksum(buffer);

    SaveScanner scanner;
    RepairResult repaired = scanner.scanAndRepair(buffer);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.orphanEntriesRemoved, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.chunksPreserved, static_cast<std::size_t>(2));

    SaveContainer rebuilt;
    CHECK(readSaveContainer(repaired.output.data(), repaired.output.size(), rebuilt));
    CHECK_EQ(rebuilt.chunks.size(), static_cast<std::size_t>(2));
    CHECK(scanner.scan(repaired.output).clean());
}

VF_TEST(repairIsRefusedWhenTheHeaderItselfIsFatal) {
    ByteBuffer buffer = writeSaveContainer(buildContainer(SaveVersion::V4, 2));
    saveWriteU32At(buffer.data(), buffer.size(), 0, 0u);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(buffer);
    RepairPlan plan = scanner.planRepair(buffer.data(), buffer.size(), report);
    CHECK(!plan.possible);
    CHECK(plan.actions.empty());
    CHECK(plan.refusal.find("backup") != std::string::npos);

    RepairResult repaired = scanner.repair(buffer.data(), buffer.size(), plan);
    CHECK(!repaired.ok);
    CHECK(repaired.output.empty());
    CHECK_EQ(repaired.failure, plan.refusal);
}

VF_TEST(repairPlanIsRefusedForABufferItWasNotBuiltFrom) {
    ByteBuffer first = writeSaveContainer(buildContainer(SaveVersion::V4, 3));
    ByteBuffer second = writeSaveContainer(buildContainer(SaveVersion::V4, 2));

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(first);
    RepairPlan plan = scanner.planRepair(second.data(), second.size(), report);
    CHECK(!plan.possible);
    CHECK(plan.refusal.find("different buffer") != std::string::npos);
}

VF_TEST(regeneratedTerrainIsDeterministicAndFollowsTheSeed) {
    Chunk first = regenerateChunkFromSeed(99u, Vec3i{1, 2, 3});
    Chunk again = regenerateChunkFromSeed(99u, Vec3i{1, 2, 3});
    Chunk elsewhere = regenerateChunkFromSeed(99u, Vec3i{4, 2, 3});
    Chunk otherSeed = regenerateChunkFromSeed(100u, Vec3i{1, 2, 3});

    CHECK(sameBlocks(first, again));
    CHECK(!sameBlocks(first, elsewhere));
    CHECK(!sameBlocks(first, otherSeed));

    CHECK_EQ(regeneratedSurfaceHeight(99u, 0, 0), regeneratedSurfaceHeight(99u, 0, 0));
    CHECK(regeneratedSurfaceHeight(99u, 0, 0) >= 32);
    CHECK(regeneratedSurfaceHeight(99u, 0, 0) <= 64);
    CHECK(regeneratedSurfaceHeight(99u, 7, 9) >= 32);
    CHECK(regeneratedSurfaceHeight(99u, 7, 9) <= 64);
}

VF_TEST(severityAndActionNamesStayReadable) {
    CHECK(std::string(issueSeverityName(IssueSeverity::Info)) == "info");
    CHECK(std::string(issueSeverityName(IssueSeverity::Warning)) == "warning");
    CHECK(std::string(issueSeverityName(IssueSeverity::Recoverable)) == "recoverable");
    CHECK(std::string(issueSeverityName(IssueSeverity::Fatal)) == "fatal");
    CHECK(IssueSeverity::Info < IssueSeverity::Warning);
    CHECK(IssueSeverity::Warning < IssueSeverity::Recoverable);
    CHECK(IssueSeverity::Recoverable < IssueSeverity::Fatal);

    CHECK(std::string(integrityIssueName(IntegrityIssueKind::BadMagic)) == "badMagic");
    CHECK(std::string(integrityIssueName(IntegrityIssueKind::LightingInconsistent)) ==
          "lightingInconsistent");
    CHECK(std::string(repairActionName(RepairActionKind::RegenerateChunk)) == "regenerateChunk");
    CHECK(std::string(repairActionName(RepairActionKind::RemoveOrphanEntry)) ==
          "removeOrphanEntry");
}

VF_TEST(aContainerDamagedInSeveralPlacesIsSalvagedInOnePass) {
    SaveContainer container = buildContainer(SaveVersion::V4, 5);
    ByteBuffer healthy = writeSaveContainer(container);
    ByteBuffer damaged = healthy;

    EntrySlot broken = slotOf(damaged, 0);
    saveWriteU32At(damaged.data(), damaged.size(), broken.payloadAt + 8, 0u);

    EntrySlot orphan = slotOf(damaged, 2);
    saveWriteU32At(damaged.data(), damaged.size(), orphan.entryAt + 16, 0u);

    EntrySlot stale = slotOf(damaged, 3);
    std::size_t checksumAt = stale.payloadAt + stale.payloadBytes - 4;
    damaged[checksumAt] = static_cast<uint8_t>(damaged[checksumAt] ^ 0x0Fu);
    refreshTableChecksum(damaged);

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(damaged);
    CHECK_EQ(report.countOf(IntegrityIssueKind::MalformedRecord), static_cast<std::size_t>(1));
    CHECK_EQ(report.countOf(IntegrityIssueKind::OrphanEntry), static_cast<std::size_t>(1));
    CHECK_EQ(report.countOf(IntegrityIssueKind::ChecksumMismatch), static_cast<std::size_t>(1));

    RepairResult repaired = scanner.scanAndRepair(damaged);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.chunksDropped, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.orphanEntriesRemoved, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.checksumsRewritten, static_cast<std::size_t>(1));
    CHECK_EQ(repaired.chunksPreserved, static_cast<std::size_t>(3));
    CHECK(scanner.scan(repaired.output).clean());

    const Vec3i survivors[3] = {{1, 0, 2}, {3, 0, 6}, {4, 0, 8}};
    for (const Vec3i& coord : survivors) {
        ByteBuffer before;
        ByteBuffer after;
        CHECK(readChunkPayloadBytes(healthy.data(), healthy.size(), coord, before));
        CHECK(readChunkPayloadBytes(repaired.output.data(), repaired.output.size(), coord, after));
        CHECK(before == after);
    }
}
