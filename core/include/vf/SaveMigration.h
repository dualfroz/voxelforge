#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Chunk.h"
#include "Codec.h"
#include "Math.h"
#include "Region.h"
#include "World.h"

namespace vf {

enum class SaveVersion : uint32_t {
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
};

constexpr SaveVersion currentSaveVersion = SaveVersion::V4;
constexpr SaveVersion oldestSaveVersion = SaveVersion::V1;

constexpr uint32_t saveMagic = 0x56585356u;
constexpr std::size_t saveHeaderBytes = 28;
constexpr std::size_t saveEntryBytes = 24;
constexpr uint32_t saveSectorBytes = 256;

constexpr int saveChunkYMin = -4;
constexpr int saveChunkYMax = 19;

constexpr uint32_t saveRecordEmpty = 1u;
constexpr uint32_t saveRecordCompressed = 2u;

constexpr uint8_t saveStageComplete = 3;
constexpr uint8_t saveDefaultSkyLight = 15;

struct SaveChunkMeta {
    uint64_t inhabitedTicks = 0;
    uint64_t lastSavedTick = 0;
    uint8_t generationStage = saveStageComplete;
};

struct SaveChunkRecord {
    Vec3i coord;
    bool empty = false;
    Chunk blocks;
    std::vector<uint8_t> light;
    SaveChunkMeta meta;
};

struct SaveContainer {
    SaveVersion version = currentSaveVersion;
    int regionX = 0;
    int regionZ = 0;
    uint32_t sectorBytes = saveSectorBytes;
    bool compressPayloads = false;
    std::vector<SaveChunkRecord> chunks;

    const SaveChunkRecord* find(const Vec3i& coord) const;
};

struct SavePayloadInfo {
    uint32_t paletteCount = 0;
    uint32_t maxIndex = 0;
    uint32_t bitWidth = 0;
    uint32_t lightBytes = 0;
    bool compressed = false;
    bool empty = false;
    std::size_t rawBodyBytes = 0;
};

struct SaveHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    int32_t regionX = 0;
    int32_t regionZ = 0;
    uint32_t chunkCount = 0;
    uint32_t sectorBytes = 0;
    uint32_t tableChecksum = 0;
};

struct SaveTableEntry {
    Vec3i coord;
    uint32_t sectorOffset = 0;
    uint32_t sectorCount = 0;
    uint32_t payloadBytes = 0;
};

bool saveReadU32At(const uint8_t* data, std::size_t size, std::size_t at, uint32_t& out);
bool saveWriteU32At(uint8_t* data, std::size_t size, std::size_t at, uint32_t value);

bool isKnownSaveVersion(uint32_t version);
bool versionHasPalette(SaveVersion version);
bool versionHasLight(SaveVersion version);
bool versionHasChecksum(SaveVersion version);
bool versionHasMetadata(SaveVersion version);
const char* saveVersionName(SaveVersion version);

bool readSaveHeader(const uint8_t* data, std::size_t size, SaveHeader& out);
std::size_t saveTableBytes(const SaveHeader& header);
std::size_t saveHeaderSectors(const SaveHeader& header);
std::size_t saveDataStart(const SaveHeader& header);

bool readSaveTable(const uint8_t* data, std::size_t size, const SaveHeader& header,
                   std::vector<SaveTableEntry>& out);

std::vector<uint8_t> defaultChunkLight(const Chunk& chunk, uint8_t skyLevel);
bool chunkIsAllAir(const Chunk& chunk);

ByteBuffer encodeChunkPayload(SaveVersion version, const SaveChunkRecord& record,
                              bool compressBody);

bool decodeChunkPayload(SaveVersion version, const uint8_t* data, std::size_t size,
                        SaveChunkRecord& out, SavePayloadInfo* info = nullptr);

bool verifyChunkPayloadChecksum(SaveVersion version, const uint8_t* data, std::size_t size,
                                uint32_t& stored, uint32_t& computed);

ByteBuffer packSaveContainer(SaveVersion version, int regionX, int regionZ, uint32_t sectorBytes,
                             const std::vector<Vec3i>& coords,
                             const std::vector<ByteBuffer>& payloads,
                             std::vector<uint32_t>* sectorOffsets = nullptr);

bool saveCoordLess(const Vec3i& a, const Vec3i& b);

ByteBuffer writeSaveContainer(const SaveContainer& container);
bool readSaveContainer(const uint8_t* data, std::size_t size, SaveContainer& out);

bool readChunkPayloadBytes(const uint8_t* data, std::size_t size, const Vec3i& coord,
                           ByteBuffer& out);

bool isCanonicalSaveLayout(const uint8_t* data, std::size_t size);

SaveContainer saveContainerFromWorld(const World& world, RegionCoord region, SaveVersion version);
void applySaveContainerToWorld(const SaveContainer& container, World& world);

struct MigrationStep {
    SaveVersion from = SaveVersion::V1;
    SaveVersion to = SaveVersion::V2;
    std::string summary;
};

struct MigrationPlan {
    SaveVersion from = currentSaveVersion;
    SaveVersion to = currentSaveVersion;
    std::vector<MigrationStep> steps;
    bool possible = false;
    std::string refusal;
};

struct MigrationResult {
    bool ok = false;
    SaveVersion from = currentSaveVersion;
    SaveVersion to = currentSaveVersion;
    std::vector<MigrationStep> applied;
    std::size_t chunksMigrated = 0;
    std::size_t paletteEntriesCreated = 0;
    std::size_t lightCellsInitialized = 0;
    std::size_t metadataRecordsCreated = 0;
    std::string failure;
    ByteBuffer output;
};

class SaveMigrator {
public:
    uint8_t skyLightForNewPlanes = saveDefaultSkyLight;
    SaveChunkMeta metadataDefaults;
    bool compressMigratedPayloads = false;

    static bool peekVersion(const uint8_t* data, std::size_t size, SaveVersion& out);
    static MigrationPlan planFor(SaveVersion from, SaveVersion to);

    MigrationResult migrate(const uint8_t* data, std::size_t size, SaveVersion target) const;
    MigrationResult migrateToCurrent(const ByteBuffer& buffer) const;

private:
    bool applyStep(const ByteBuffer& in, const MigrationStep& step, MigrationResult& result,
                   ByteBuffer& out) const;
};

}
