#include "testHarness.h"

#include "vf/Block.h"
#include "vf/RegionCompact.h"
#include "vf/SaveIntegrity.h"
#include "vf/SaveMigration.h"

#include <cstdint>
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

Chunk busyChunk(int seed) {
    Chunk chunk;
    int cell = 0;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) {
                int value = (cell + seed) % static_cast<int>(Block::Count);
                chunk.set(x, y, z, static_cast<Block>(value));
                ++cell;
            }
    return chunk;
}

SaveChunkRecord buildRecord(const Vec3i& coord, const Chunk& blocks) {
    SaveChunkRecord record;
    record.coord = coord;
    record.blocks = blocks;
    record.light = defaultChunkLight(blocks, saveDefaultSkyLight);
    return record;
}

SaveContainer uniformContainer(int chunkTotal) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    for (int i = 0; i < chunkTotal; ++i)
        container.chunks.push_back(buildRecord(Vec3i{i, 0, 0}, uniformChunk(Block::Stone)));
    return container;
}

std::vector<Vec3i> coordsOf(const ByteBuffer& buffer) {
    std::vector<Vec3i> coords;
    SaveHeader header;
    if (!readSaveHeader(buffer.data(), buffer.size(), header)) return coords;
    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(buffer.data(), buffer.size(), header, entries)) return coords;
    for (const SaveTableEntry& entry : entries) coords.push_back(entry.coord);
    return coords;
}

// A layout written by hand so the fragmentation arithmetic is known before the
// code runs: 13 sectors of 64 bytes, 2 header sectors, runs of 4 and 3 sectors
// at sector 2 and sector 10, so 4 sectors are holes and 1 sector inside the
// second run is reserved for nothing.
ByteBuffer handBuiltLayout() {
    const uint32_t sectorBytes = 64;
    ByteBuffer buffer(13 * static_cast<std::size_t>(sectorBytes), 0);
    saveWriteU32At(buffer.data(), buffer.size(), 0, saveMagic);
    saveWriteU32At(buffer.data(), buffer.size(), 4, static_cast<uint32_t>(SaveVersion::V4));
    saveWriteU32At(buffer.data(), buffer.size(), 8, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), 12, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), 16, 2u);
    saveWriteU32At(buffer.data(), buffer.size(), 20, sectorBytes);

    std::size_t at = saveHeaderBytes;
    saveWriteU32At(buffer.data(), buffer.size(), at + 0, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 4, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 8, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 12, 2u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 16, 4u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 20, 200u);

    at += saveEntryBytes;
    saveWriteU32At(buffer.data(), buffer.size(), at + 0, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 4, 0u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 8, 1u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 12, 10u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 16, 3u);
    saveWriteU32At(buffer.data(), buffer.size(), at + 20, 100u);

    uint32_t checksum = fnv1a32(buffer.data() + saveHeaderBytes, 2 * saveEntryBytes);
    saveWriteU32At(buffer.data(), buffer.size(), 24, checksum);
    return buffer;
}

} // namespace

VF_TEST(fragmentationOfAHandBuiltLayoutMatchesTheArithmetic) {
    ByteBuffer buffer = handBuiltLayout();
    RegionCompactor compactor;

    CHECK_EQ(compactor.fragmentationPercent(buffer), 38);

    std::vector<ChunkFragment> fragments = compactor.survey(buffer);
    CHECK_EQ(fragments.size(), static_cast<std::size_t>(2));
    if (fragments.size() != 2) return;

    CHECK_EQ(fragments[0].sectorOffset, static_cast<uint32_t>(2));
    CHECK_EQ(fragments[0].sectorCount, static_cast<uint32_t>(4));
    CHECK_EQ(fragments[0].payloadBytes, static_cast<uint32_t>(200));
    CHECK_EQ(fragments[0].neededSectors, static_cast<uint32_t>(4));
    CHECK_EQ(fragments[0].excessSectors, static_cast<uint32_t>(0));
    CHECK_EQ(fragments[0].slackBytes, static_cast<uint32_t>(56));
    CHECK(fragments[0].reachable);

    CHECK_EQ(fragments[1].sectorOffset, static_cast<uint32_t>(10));
    CHECK_EQ(fragments[1].sectorCount, static_cast<uint32_t>(3));
    CHECK_EQ(fragments[1].neededSectors, static_cast<uint32_t>(2));
    CHECK_EQ(fragments[1].excessSectors, static_cast<uint32_t>(1));
    CHECK_EQ(fragments[1].slackBytes, static_cast<uint32_t>(92));
    CHECK(fragments[1].reachable);
}

VF_TEST(aFreshContainerIsCanonicalAndUnfragmented) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(4));
    RegionCompactor compactor;
    CHECK_EQ(compactor.fragmentationPercent(buffer), 0);
    CHECK(isCanonicalSaveLayout(buffer.data(), buffer.size()));

    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK(stats.ok);
    CHECK(out == buffer);
    CHECK_EQ(stats.chunksMoved, static_cast<std::size_t>(0));
    CHECK_EQ(stats.chunksDropped, static_cast<std::size_t>(0));
    CHECK_EQ(stats.chunksKept, static_cast<std::size_t>(4));
    CHECK_EQ(stats.bytesReclaimed, static_cast<std::size_t>(0));
    CHECK_EQ(stats.holeSectors, static_cast<uint32_t>(0));
    CHECK_EQ(stats.excessSectors, static_cast<uint32_t>(0));
    CHECK_EQ(stats.fragmentationBefore, 0);
    CHECK_EQ(stats.fragmentationAfter, 0);
    CHECK_EQ(stats.operations, static_cast<std::size_t>(5));
}

VF_TEST(aChunkThatOutgrowsItsRunMovesToTheEndAndLeavesAHole) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(3));
    RegionCompactor compactor;
    std::vector<ChunkFragment> before = compactor.survey(buffer);
    uint32_t abandoned = before[1].sectorCount;
    std::size_t sizeBefore = buffer.size();

    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{1, 0, 0}, busyChunk(3)), false));
    CHECK(buffer.size() > sizeBefore);

    std::vector<ChunkFragment> after = compactor.survey(buffer);
    CHECK_EQ(after.size(), static_cast<std::size_t>(3));
    CHECK_EQ(after[1].sectorOffset, static_cast<uint32_t>(sizeBefore / saveSectorBytes));
    CHECK(after[1].sectorCount > abandoned);
    CHECK(compactor.fragmentationPercent(buffer) > 0);

    SaveScanner scanner;
    CHECK(scanner.scan(buffer).clean());
    CHECK(!isCanonicalSaveLayout(buffer.data(), buffer.size()));

    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK_EQ(stats.holeSectors, abandoned);
    CHECK(stats.bytesReclaimed >= abandoned * saveSectorBytes);
    CHECK_EQ(stats.fragmentationAfter, 0);
    CHECK(isCanonicalSaveLayout(out.data(), out.size()));
}

VF_TEST(aChunkThatShrinksKeepsUnusedSectorsInsideItsOwnRun) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    container.chunks.push_back(buildRecord(Vec3i{0, 0, 0}, busyChunk(1)));
    container.chunks.push_back(buildRecord(Vec3i{1, 0, 0}, uniformChunk(Block::Stone)));
    ByteBuffer buffer = writeSaveContainer(container);
    std::size_t sizeBefore = buffer.size();

    RegionCompactor compactor;
    uint32_t runBefore = compactor.survey(buffer)[0].sectorCount;
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Dirt)),
                             false));
    CHECK_EQ(buffer.size(), sizeBefore);

    std::vector<ChunkFragment> after = compactor.survey(buffer);
    CHECK_EQ(after[0].sectorCount, runBefore);
    CHECK(after[0].neededSectors < runBefore);
    CHECK_EQ(after[0].excessSectors, runBefore - after[0].neededSectors);
    CHECK(compactor.fragmentationPercent(buffer) > 0);

    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK_EQ(stats.holeSectors, static_cast<uint32_t>(0));
    CHECK_EQ(stats.excessSectors, runBefore - after[0].neededSectors);
    CHECK(out.size() < buffer.size());
    CHECK_EQ(stats.fragmentationAfter, 0);
}

VF_TEST(compactionRewritesEveryChunkByteIdentically) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    for (int i = 0; i < 5; ++i)
        container.chunks.push_back(buildRecord(Vec3i{i, 1, i}, busyChunk(i * 17)));
    ByteBuffer buffer = writeSaveContainer(container);

    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{2, 1, 2}, uniformChunk(Block::Sand)),
                             false));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{0, 1, 0}, busyChunk(5)), false));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{4, 1, 4}, uniformChunk(Block::Gravel)),
                             false));

    std::vector<Vec3i> coords = coordsOf(buffer);
    CHECK_EQ(coords.size(), static_cast<std::size_t>(5));
    std::vector<ByteBuffer> before;
    for (const Vec3i& coord : coords) {
        ByteBuffer payload;
        CHECK(readChunkPayloadBytes(buffer.data(), buffer.size(), coord, payload));
        before.push_back(payload);
    }

    RegionCompactor compactor;
    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK(stats.ok);
    CHECK_EQ(stats.chunksKept, static_cast<std::size_t>(5));
    CHECK_EQ(stats.chunksDropped, static_cast<std::size_t>(0));
    CHECK_EQ(stats.chunksRepacked, static_cast<std::size_t>(0));

    for (std::size_t i = 0; i < coords.size(); ++i) {
        ByteBuffer after;
        CHECK(readChunkPayloadBytes(out.data(), out.size(), coords[i], after));
        CHECK(after == before[i]);
    }
    CHECK(isCanonicalSaveLayout(out.data(), out.size()));
    SaveScanner scanner;
    CHECK(scanner.scan(out).clean());
}

VF_TEST(compactionReportsTheSectorsItGaveBack) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(4));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{1, 0, 0}, busyChunk(2)), false));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{3, 0, 0}, busyChunk(4)), false));

    RegionCompactor compactor;
    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK_EQ(stats.sectorBytes, saveSectorBytes);
    CHECK_EQ(stats.bytesBefore, buffer.size());
    CHECK_EQ(stats.bytesAfter, out.size());
    CHECK_EQ(stats.bytesReclaimed, buffer.size() - out.size());
    CHECK_EQ(stats.sectorsBefore, static_cast<uint32_t>(buffer.size() / saveSectorBytes));
    CHECK_EQ(stats.sectorsAfter, static_cast<uint32_t>(out.size() / saveSectorBytes));
    CHECK(stats.holeSectors > 0u);
    CHECK(stats.chunksMoved > 0u);
    CHECK_EQ(stats.operations, 4u + stats.chunksMoved + 1u);
    CHECK(stats.fragmentationBefore > 0);
    CHECK_EQ(stats.fragmentationAfter, 0);
}

VF_TEST(compactionDropsChunksThatHoldNothing) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    container.chunks.push_back(buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Stone)));
    container.chunks.push_back(buildRecord(Vec3i{1, 0, 0}, uniformChunk(Block::Air)));
    container.chunks.push_back(buildRecord(Vec3i{2, 0, 0}, uniformChunk(Block::Dirt)));
    ByteBuffer buffer = writeSaveContainer(container);

    RegionCompactor compactor;
    std::vector<ChunkFragment> fragments = compactor.survey(buffer);
    CHECK(!fragments[0].empty);
    CHECK(fragments[1].empty);
    CHECK(!fragments[2].empty);

    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK_EQ(stats.chunksDropped, static_cast<std::size_t>(1));
    CHECK_EQ(stats.chunksKept, static_cast<std::size_t>(2));
    CHECK_EQ(coordsOf(out).size(), static_cast<std::size_t>(2));
    ByteBuffer gone;
    CHECK(!readChunkPayloadBytes(out.data(), out.size(), Vec3i{1, 0, 0}, gone));

    RegionCompactor keeper;
    keeper.dropEmptyChunks = false;
    ByteBuffer kept;
    CompactionStats keptStats;
    CHECK(keeper.compact(buffer, kept, keptStats));
    CHECK_EQ(keptStats.chunksDropped, static_cast<std::size_t>(0));
    CHECK_EQ(keptStats.chunksKept, static_cast<std::size_t>(3));
    CHECK(kept == buffer);
}

VF_TEST(repackingWithCompressionShrinksTheFileAndStillReadsBack) {
    SaveContainer container;
    container.version = SaveVersion::V4;
    container.chunks.push_back(buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Stone)));
    container.chunks.push_back(buildRecord(Vec3i{1, 0, 0}, uniformChunk(Block::Dirt)));
    container.chunks.push_back(buildRecord(Vec3i{2, 0, 0}, busyChunk(9)));
    ByteBuffer buffer = writeSaveContainer(container);

    RegionCompactor compactor;
    compactor.repackWithStrongerCompression = true;
    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(buffer, out, stats));
    CHECK(stats.chunksRepacked > 0u);
    CHECK(out.size() < buffer.size());
    CHECK(isCanonicalSaveLayout(out.data(), out.size()));

    SaveContainer reloaded;
    CHECK(readSaveContainer(out.data(), out.size(), reloaded));
    CHECK_EQ(reloaded.chunks.size(), static_cast<std::size_t>(3));
    for (const SaveChunkRecord& original : container.chunks) {
        const SaveChunkRecord* back = reloaded.find(original.coord);
        CHECK(back != nullptr);
        if (back == nullptr) continue;
        for (std::size_t i = 0; i < original.blocks.raw().size(); ++i)
            CHECK(original.blocks.raw()[i] == back->blocks.raw()[i]);
        CHECK(original.light == back->light);
        CHECK_EQ(original.meta.generationStage, back->meta.generationStage);
    }
    SaveScanner scanner;
    CHECK(scanner.scan(out).clean());
}

VF_TEST(compactionRefusesAContainerTheScannerCallsDamaged) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(3));
    SaveHeader header;
    CHECK(readSaveHeader(buffer.data(), buffer.size(), header));
    std::vector<SaveTableEntry> entries;
    CHECK(readSaveTable(buffer.data(), buffer.size(), header, entries));
    std::size_t payloadAt = static_cast<std::size_t>(entries[1].sectorOffset) * header.sectorBytes;
    saveWriteU32At(buffer.data(), buffer.size(), payloadAt + 8, 0u);

    RegionCompactor compactor;
    ByteBuffer out;
    CompactionStats stats;
    CHECK(!compactor.compact(buffer, out, stats));
    CHECK(!stats.ok);
    CHECK(stats.failure.find("integrity") != std::string::npos);
    CHECK(out.empty());
}

VF_TEST(compactionAndRepairCooperateOnADamagedFragmentedContainer) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(4));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{2, 0, 0}, busyChunk(6)), false));

    SaveHeader header;
    CHECK(readSaveHeader(buffer.data(), buffer.size(), header));
    std::vector<SaveTableEntry> entries;
    CHECK(readSaveTable(buffer.data(), buffer.size(), header, entries));
    std::size_t payloadAt = static_cast<std::size_t>(entries[0].sectorOffset) * header.sectorBytes;
    saveWriteU32At(buffer.data(), buffer.size(), payloadAt + 8, 0u);

    RegionCompactor compactor;
    ByteBuffer refused;
    CompactionStats refusedStats;
    CHECK(!compactor.compact(buffer, refused, refusedStats));

    SaveScanner scanner;
    RepairResult repaired = scanner.scanAndRepair(buffer);
    CHECK(repaired.ok);
    CHECK_EQ(repaired.chunksDropped, static_cast<std::size_t>(1));

    ByteBuffer out;
    CompactionStats stats;
    CHECK(compactor.compact(repaired.output, out, stats));
    CHECK(stats.ok);
    CHECK_EQ(stats.fragmentationAfter, 0);
    CHECK(isCanonicalSaveLayout(out.data(), out.size()));
}

VF_TEST(inPlaceUpdateRefusesAChunkThatIsNotInTheTable) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(2));
    ByteBuffer untouched = buffer;
    CHECK(!updateChunkInPlace(buffer, buildRecord(Vec3i{9, 0, 0}, uniformChunk(Block::Sand)),
                              false));
    CHECK(buffer == untouched);

    ByteBuffer rubbish(40, 0);
    CHECK(!updateChunkInPlace(rubbish, buildRecord(Vec3i{0, 0, 0}, uniformChunk(Block::Sand)),
                              false));
}

VF_TEST(inPlaceUpdateKeepsTheContainerReadableAndChecksummed) {
    ByteBuffer buffer = writeSaveContainer(uniformContainer(3));
    CHECK(updateChunkInPlace(buffer, buildRecord(Vec3i{1, 0, 0}, busyChunk(21)), true));

    SaveScanner scanner;
    CHECK(scanner.scan(buffer).clean());

    SaveContainer reloaded;
    CHECK(readSaveContainer(buffer.data(), buffer.size(), reloaded));
    const SaveChunkRecord* updated = reloaded.find({1, 0, 0});
    CHECK(updated != nullptr);
    if (updated == nullptr) return;
    Chunk expected = busyChunk(21);
    for (std::size_t i = 0; i < expected.raw().size(); ++i)
        CHECK(expected.raw()[i] == updated->blocks.raw()[i]);
}

VF_TEST(surveyAndFragmentationRejectBuffersThatAreNotContainers) {
    RegionCompactor compactor;
    ByteBuffer rubbish(500, 0x7F);
    CHECK(compactor.survey(rubbish).empty());
    CHECK_EQ(compactor.fragmentationPercent(rubbish), -1);

    ByteBuffer stump(4, 0);
    CHECK(compactor.survey(stump).empty());
    CHECK_EQ(compactor.fragmentationPercent(stump), -1);

    ByteBuffer out;
    CompactionStats stats;
    CHECK(!compactor.compact(rubbish, out, stats));
    CHECK(stats.failure.find("readable save container") != std::string::npos);
}
