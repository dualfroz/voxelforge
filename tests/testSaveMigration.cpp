#include "testHarness.h"

#include "vf/Block.h"
#include "vf/Region.h"
#include "vf/SaveIntegrity.h"
#include "vf/SaveMigration.h"

#include <cstdint>
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

SaveChunkRecord buildRecord(const Vec3i& coord, int seed, SaveVersion version) {
    SaveChunkRecord record;
    record.coord = coord;
    record.blocks = texturedChunk(seed);
    if (versionHasLight(version))
        record.light = defaultChunkLight(record.blocks, saveDefaultSkyLight);
    return record;
}

SaveContainer buildContainer(SaveVersion version, int chunkTotal) {
    SaveContainer container;
    container.version = version;
    for (int i = 0; i < chunkTotal; ++i) {
        Vec3i coord{i % 4, i / 4, (i * 3) % 5};
        container.chunks.push_back(buildRecord(coord, i, version));
    }
    return container;
}

std::size_t distinctBlocks(const Chunk& chunk) {
    bool seen[256] = {false};
    std::size_t total = 0;
    for (Block block : chunk.raw()) {
        std::size_t slot = static_cast<std::size_t>(block);
        if (seen[slot]) continue;
        seen[slot] = true;
        ++total;
    }
    return total;
}

bool sameBlocks(const Chunk& a, const Chunk& b) {
    for (std::size_t i = 0; i < a.raw().size(); ++i)
        if (a.raw()[i] != b.raw()[i]) return false;
    return true;
}

}

VF_TEST(containerRoundTripsAtEveryFormatVersion) {
    for (uint32_t v = 1; v <= 4; ++v) {
        SaveVersion version = static_cast<SaveVersion>(v);
        SaveContainer original = buildContainer(version, 3);
        ByteBuffer buffer = writeSaveContainer(original);

        SaveContainer reloaded;
        CHECK(readSaveContainer(buffer.data(), buffer.size(), reloaded));
        CHECK(reloaded.version == version);
        CHECK_EQ(reloaded.chunks.size(), original.chunks.size());
        for (const SaveChunkRecord& record : original.chunks) {
            const SaveChunkRecord* back = reloaded.find(record.coord);
            CHECK(back != nullptr);
            if (back == nullptr) continue;
            CHECK(sameBlocks(record.blocks, back->blocks));
            CHECK(record.light == back->light);
        }
    }
}

VF_TEST(everyFreshContainerUsesTheCanonicalLayout) {
    for (uint32_t v = 1; v <= 4; ++v) {
        SaveContainer container = buildContainer(static_cast<SaveVersion>(v), 4);
        ByteBuffer buffer = writeSaveContainer(container);
        CHECK(isCanonicalSaveLayout(buffer.data(), buffer.size()));
        CHECK_EQ(buffer.size() % saveSectorBytes, static_cast<std::size_t>(0));
    }
}

VF_TEST(versionFeatureFlagsMatchTheDocumentedFormatTable) {
    CHECK(!versionHasPalette(SaveVersion::V1));
    CHECK(versionHasPalette(SaveVersion::V2));
    CHECK(!versionHasLight(SaveVersion::V2));
    CHECK(versionHasLight(SaveVersion::V3));
    CHECK(!versionHasChecksum(SaveVersion::V2));
    CHECK(versionHasChecksum(SaveVersion::V3));
    CHECK(!versionHasMetadata(SaveVersion::V3));
    CHECK(versionHasMetadata(SaveVersion::V4));
    CHECK(currentSaveVersion == SaveVersion::V4);
    CHECK(oldestSaveVersion == SaveVersion::V1);
    CHECK(std::string(saveVersionName(SaveVersion::V1)) == "v1");
    CHECK(std::string(saveVersionName(SaveVersion::V4)) == "v4");
    CHECK(isKnownSaveVersion(1u));
    CHECK(isKnownSaveVersion(4u));
    CHECK(!isKnownSaveVersion(0u));
    CHECK(!isKnownSaveVersion(5u));
}

VF_TEST(migrationChainFromV1ToV4KeepsEveryBlockIdentical) {
    SaveContainer original = buildContainer(SaveVersion::V1, 5);
    ByteBuffer legacy = writeSaveContainer(original);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrateToCurrent(legacy);
    CHECK(result.ok);
    CHECK(result.failure.empty());
    CHECK(result.from == SaveVersion::V1);
    CHECK(result.to == SaveVersion::V4);
    CHECK_EQ(result.chunksMigrated, static_cast<std::size_t>(5));

    SaveContainer lifted;
    CHECK(readSaveContainer(result.output.data(), result.output.size(), lifted));
    CHECK(lifted.version == SaveVersion::V4);
    CHECK_EQ(lifted.chunks.size(), original.chunks.size());
    for (const SaveChunkRecord& before : original.chunks) {
        const SaveChunkRecord* after = lifted.find(before.coord);
        CHECK(after != nullptr);
        if (after == nullptr) continue;
        CHECK(sameBlocks(before.blocks, after->blocks));
    }
}

VF_TEST(migrationAppliesOneStepPerVersionHop) {
    SaveContainer original = buildContainer(SaveVersion::V1, 2);
    ByteBuffer legacy = writeSaveContainer(original);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrateToCurrent(legacy);
    CHECK(result.ok);
    CHECK_EQ(result.applied.size(), static_cast<std::size_t>(3));
    CHECK(result.applied[0].from == SaveVersion::V1);
    CHECK(result.applied[0].to == SaveVersion::V2);
    CHECK(result.applied[1].from == SaveVersion::V2);
    CHECK(result.applied[1].to == SaveVersion::V3);
    CHECK(result.applied[2].from == SaveVersion::V3);
    CHECK(result.applied[2].to == SaveVersion::V4);
    for (const MigrationStep& step : result.applied) CHECK(!step.summary.empty());

    MigrationPlan plan = SaveMigrator::planFor(SaveVersion::V2, SaveVersion::V4);
    CHECK(plan.possible);
    CHECK(plan.refusal.empty());
    CHECK_EQ(plan.steps.size(), static_cast<std::size_t>(2));
}

VF_TEST(everyIntermediateVersionIsItselfAValidContainer) {
    SaveContainer original = buildContainer(SaveVersion::V1, 3);
    ByteBuffer legacy = writeSaveContainer(original);
    SaveMigrator migrator;
    SaveScanner scanner;

    for (uint32_t target = 2; target <= 4; ++target) {
        MigrationResult result = migrator.migrate(legacy.data(), legacy.size(),
                                                  static_cast<SaveVersion>(target));
        CHECK(result.ok);
        IntegrityReport report = scanner.scan(result.output);
        CHECK(report.clean());
        CHECK(report.version == static_cast<SaveVersion>(target));
        CHECK_EQ(report.readableChunks, static_cast<std::size_t>(3));
        CHECK(isCanonicalSaveLayout(result.output.data(), result.output.size()));
    }
}

VF_TEST(migrationDownwardIsRefusedWithAStatedReason) {
    SaveContainer modern = buildContainer(SaveVersion::V4, 2);
    ByteBuffer buffer = writeSaveContainer(modern);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrate(buffer.data(), buffer.size(), SaveVersion::V2);
    CHECK(!result.ok);
    CHECK(result.output.empty());
    CHECK(result.failure.find("downgrade") != std::string::npos);
    CHECK(result.failure.find("v4 -> v2") != std::string::npos);

    MigrationPlan plan = SaveMigrator::planFor(SaveVersion::V4, SaveVersion::V1);
    CHECK(!plan.possible);
    CHECK(plan.steps.empty());
    CHECK(!plan.refusal.empty());
}

VF_TEST(migrationToTheSameVersionCopiesTheBufferVerbatim) {
    SaveContainer modern = buildContainer(SaveVersion::V4, 3);
    ByteBuffer buffer = writeSaveContainer(modern);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrate(buffer.data(), buffer.size(), SaveVersion::V4);
    CHECK(result.ok);
    CHECK(result.applied.empty());
    CHECK(result.output == buffer);
}

VF_TEST(fieldsAddedByAMigrationGetTheirDocumentedDefaults) {
    SaveContainer original = buildContainer(SaveVersion::V2, 2);
    ByteBuffer buffer = writeSaveContainer(original);

    SaveMigrator migrator;
    MigrationResult toV3 = migrator.migrate(buffer.data(), buffer.size(), SaveVersion::V3);
    CHECK(toV3.ok);
    SaveContainer lit;
    CHECK(readSaveContainer(toV3.output.data(), toV3.output.size(), lit));
    for (const SaveChunkRecord& record : lit.chunks) {
        CHECK_EQ(record.light.size(), static_cast<std::size_t>(chunkVolume));
        CHECK(record.light == defaultChunkLight(record.blocks, saveDefaultSkyLight));
    }

    MigrationResult toV4 = migrator.migrate(buffer.data(), buffer.size(), SaveVersion::V4);
    CHECK(toV4.ok);
    SaveContainer stamped;
    CHECK(readSaveContainer(toV4.output.data(), toV4.output.size(), stamped));
    for (const SaveChunkRecord& record : stamped.chunks) {
        CHECK_EQ(record.meta.inhabitedTicks, static_cast<uint64_t>(0));
        CHECK_EQ(record.meta.lastSavedTick, static_cast<uint64_t>(0));
        CHECK_EQ(record.meta.generationStage, saveStageComplete);
    }
}

VF_TEST(migrationCountsTheFieldsEachStepCreated) {
    SaveContainer original = buildContainer(SaveVersion::V1, 3);
    ByteBuffer legacy = writeSaveContainer(original);

    std::size_t expectedPaletteEntries = 0;
    for (const SaveChunkRecord& record : original.chunks)
        expectedPaletteEntries += distinctBlocks(record.blocks);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrateToCurrent(legacy);
    CHECK(result.ok);
    CHECK_EQ(result.paletteEntriesCreated, expectedPaletteEntries);
    CHECK_EQ(result.lightCellsInitialized, static_cast<std::size_t>(3 * chunkVolume));
    CHECK_EQ(result.metadataRecordsCreated, static_cast<std::size_t>(3));
}

VF_TEST(migratorHonoursItsConfiguredDefaults) {
    SaveContainer original = buildContainer(SaveVersion::V2, 2);
    ByteBuffer buffer = writeSaveContainer(original);

    SaveMigrator migrator;
    migrator.skyLightForNewPlanes = 7;
    migrator.metadataDefaults.inhabitedTicks = 1234;
    migrator.metadataDefaults.lastSavedTick = 9999;
    migrator.metadataDefaults.generationStage = 1;

    MigrationResult result = migrator.migrateToCurrent(buffer);
    CHECK(result.ok);
    SaveContainer lifted;
    CHECK(readSaveContainer(result.output.data(), result.output.size(), lifted));
    for (const SaveChunkRecord& record : lifted.chunks) {
        CHECK(record.light == defaultChunkLight(record.blocks, 7));
        CHECK_EQ(record.meta.inhabitedTicks, static_cast<uint64_t>(1234));
        CHECK_EQ(record.meta.lastSavedTick, static_cast<uint64_t>(9999));
        CHECK_EQ(record.meta.generationStage, static_cast<uint8_t>(1));
    }
}

VF_TEST(migratorCanCompressTheFinalHopAndStillReadItBack) {
    SaveContainer original = buildContainer(SaveVersion::V1, 3);
    ByteBuffer legacy = writeSaveContainer(original);

    SaveMigrator plain;
    MigrationResult loose = plain.migrateToCurrent(legacy);
    CHECK(loose.ok);

    SaveMigrator packing;
    packing.compressMigratedPayloads = true;
    MigrationResult tight = packing.migrateToCurrent(legacy);
    CHECK(tight.ok);
    CHECK(tight.output.size() < loose.output.size());

    SaveContainer reloaded;
    CHECK(readSaveContainer(tight.output.data(), tight.output.size(), reloaded));
    CHECK(reloaded.compressPayloads);
    for (const SaveChunkRecord& before : original.chunks) {
        const SaveChunkRecord* after = reloaded.find(before.coord);
        CHECK(after != nullptr);
        if (after != nullptr) CHECK(sameBlocks(before.blocks, after->blocks));
    }

    SaveScanner scanner;
    CHECK(scanner.scan(tight.output).clean());
}

VF_TEST(migrationRefusesAHeaderItCannotIdentify) {
    SaveContainer original = buildContainer(SaveVersion::V1, 2);
    ByteBuffer buffer = writeSaveContainer(original);
    saveWriteU32At(buffer.data(), buffer.size(), 4, 7u);

    SaveMigrator migrator;
    MigrationResult result = migrator.migrateToCurrent(buffer);
    CHECK(!result.ok);
    CHECK(result.output.empty());
    CHECK(result.failure.find("header") != std::string::npos);

    ByteBuffer stump(20, 0);
    MigrationResult tiny = migrator.migrateToCurrent(stump);
    CHECK(!tiny.ok);
}

VF_TEST(peekVersionReadsTheHeaderWithoutTheBody) {
    SaveContainer original = buildContainer(SaveVersion::V3, 2);
    ByteBuffer buffer = writeSaveContainer(original);
    ByteBuffer headerOnly(buffer.begin(), buffer.begin() + saveHeaderBytes);

    SaveVersion version = SaveVersion::V1;
    CHECK(SaveMigrator::peekVersion(headerOnly.data(), headerOnly.size(), version));
    CHECK(version == SaveVersion::V3);

    ByteBuffer shorter(buffer.begin(), buffer.begin() + 12);
    CHECK(!SaveMigrator::peekVersion(shorter.data(), shorter.size(), version));
}

VF_TEST(defaultLightIsBlockedByOpaqueBlocksAndLitByEmitters) {
    Chunk chunk;
    chunk.set(0, 0, 0, Block::Air);
    chunk.set(1, 0, 0, Block::Stone);
    chunk.set(2, 0, 0, Block::Glowstone);

    std::vector<uint8_t> plane = defaultChunkLight(chunk, saveDefaultSkyLight);
    CHECK_EQ(plane.size(), static_cast<std::size_t>(chunkVolume));

    uint8_t openCell = plane[static_cast<std::size_t>(localIndex(0, 0, 0))];
    uint8_t stoneCell = plane[static_cast<std::size_t>(localIndex(1, 0, 0))];
    uint8_t lampCell = plane[static_cast<std::size_t>(localIndex(2, 0, 0))];

    CHECK_EQ(static_cast<int>(openCell >> 4), 15);
    CHECK_EQ(static_cast<int>(openCell & 0x0F), 0);
    CHECK_EQ(static_cast<int>(stoneCell >> 4), 0);
    CHECK_EQ(static_cast<int>(lampCell & 0x0F), blockLightEmission(Block::Glowstone));

    std::vector<uint8_t> dim = defaultChunkLight(chunk, 4);
    CHECK_EQ(static_cast<int>(dim[static_cast<std::size_t>(localIndex(0, 0, 0))] >> 4), 4);
}

VF_TEST(worldImportKeepsOnlyTheChunksOfTheAskedForRegion) {
    World world;
    world.ensureChunk({1, 0, 1}) = texturedChunk(1);
    world.ensureChunk({2, 3, 4}) = texturedChunk(2);
    world.ensureChunk({20, 0, 0}) = texturedChunk(3);
    world.ensureChunk({5, 0, 5});

    SaveContainer container = saveContainerFromWorld(world, RegionCoord{0, 0}, currentSaveVersion);
    CHECK_EQ(container.chunks.size(), static_cast<std::size_t>(2));
    CHECK(container.find({1, 0, 1}) != nullptr);
    CHECK(container.find({2, 3, 4}) != nullptr);
    CHECK(container.find({20, 0, 0}) == nullptr);
    CHECK(container.find({5, 0, 5}) == nullptr);

    SaveContainer neighbour = saveContainerFromWorld(world, RegionCoord{1, 0}, currentSaveVersion);
    CHECK_EQ(neighbour.chunks.size(), static_cast<std::size_t>(1));
    CHECK(neighbour.find({20, 0, 0}) != nullptr);
}

VF_TEST(applyingAContainerBackToAWorldRestoresTheBlocks) {
    World source;
    source.ensureChunk({1, 0, 1}) = texturedChunk(11);
    source.ensureChunk({2, 1, 3}) = texturedChunk(12);

    SaveContainer container = saveContainerFromWorld(source, RegionCoord{0, 0}, currentSaveVersion);
    ByteBuffer buffer = writeSaveContainer(container);

    SaveContainer reloaded;
    CHECK(readSaveContainer(buffer.data(), buffer.size(), reloaded));
    World rebuilt;
    applySaveContainerToWorld(reloaded, rebuilt);

    CHECK_EQ(rebuilt.chunkCount(), static_cast<std::size_t>(2));
    CHECK(sameBlocks(*source.chunkAt({1, 0, 1}), *rebuilt.chunkAt({1, 0, 1})));
    CHECK(sameBlocks(*source.chunkAt({2, 1, 3}), *rebuilt.chunkAt({2, 1, 3})));
}

VF_TEST(payloadBytesCanBePulledOutOfAContainerVerbatim) {
    SaveContainer container = buildContainer(SaveVersion::V4, 3);
    ByteBuffer buffer = writeSaveContainer(container);

    ByteBuffer payload;
    CHECK(readChunkPayloadBytes(buffer.data(), buffer.size(), Vec3i{1, 0, 3}, payload));
    CHECK(!payload.empty());

    SaveChunkRecord decoded;
    CHECK(decodeChunkPayload(SaveVersion::V4, payload.data(), payload.size(), decoded));
    const SaveChunkRecord* original = container.find({1, 0, 3});
    CHECK(original != nullptr);
    if (original != nullptr) CHECK(sameBlocks(original->blocks, decoded.blocks));

    ByteBuffer missing;
    CHECK(!readChunkPayloadBytes(buffer.data(), buffer.size(), Vec3i{9, 9, 9}, missing));
}

VF_TEST(recordChecksumCoversTheBodyAndNotItsFraming) {
    SaveContainer container = buildContainer(SaveVersion::V4, 1);
    const SaveChunkRecord& record = container.chunks[0];

    ByteBuffer loose = encodeChunkPayload(SaveVersion::V4, record, false);
    ByteBuffer tight = encodeChunkPayload(SaveVersion::V4, record, true);
    CHECK(tight.size() < loose.size());

    uint32_t looseStored = 0, looseComputed = 0, tightStored = 0, tightComputed = 0;
    CHECK(verifyChunkPayloadChecksum(SaveVersion::V4, loose.data(), loose.size(), looseStored,
                                     looseComputed));
    CHECK(verifyChunkPayloadChecksum(SaveVersion::V4, tight.data(), tight.size(), tightStored,
                                     tightComputed));
    CHECK_EQ(looseStored, looseComputed);
    CHECK_EQ(tightStored, tightComputed);
    CHECK_EQ(looseStored, tightStored);

    uint32_t stored = 0, computed = 0;
    ByteBuffer ancient = encodeChunkPayload(SaveVersion::V1, record, false);
    CHECK(!verifyChunkPayloadChecksum(SaveVersion::V1, ancient.data(), ancient.size(), stored,
                                      computed));
}
