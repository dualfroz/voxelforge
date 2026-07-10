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

// The region save container "VXSV" and the migration chain that lifts an old
// container to the current version.
//
// One container holds the chunks of a single region tile (regionSize columns
// per side, see Region.h) and is laid out like a real region file: a fixed
// header, an entry table addressing sector runs, then the chunk payloads. The
// sector indirection is what makes a chunk rewritable without touching its
// neighbours - and what makes the file fragment, which RegionCompact repairs.
//
// Header, 28 bytes, big-endian throughout (same order as Codec's ByteWriter):
//     0  u32 magic          saveMagic
//     4  u32 version        1..4
//     8  i32 regionX
//    12  i32 regionZ
//    16  u32 chunkCount     entries in the table
//    20  u32 sectorBytes    bytes per sector
//    24  u32 tableChecksum  fnv1a32 of the table, v3+ only, 0 before that
//
// Entry, 24 bytes, one per chunk:
//     0  i32 chunkX
//     4  i32 chunkY
//     8  i32 chunkZ
//    12  u32 sectorOffset   from the start of the buffer
//    16  u32 sectorCount
//    20  u32 payloadBytes   bytes actually used inside the run
//
// Payload record, by version:
//    v1  u32 blockBytes, u8 blocks[chunkVolume]
//    v2  u32 paletteCount, u8 palette[], u32 indexBytes, u8 indices[chunkVolume]
//    v3  v2 body, then u32 lightBytes, u8 light[chunkVolume], u32 checksum
//    v4  u32 flags, u32 storedBodyBytes, body, u32 checksum
//        body = u32 paletteCount, u8 palette[], u32 bitWidth, u32 wordCount,
//               u64 words[], u32 lightBytes, u8 light[], u64 inhabitedTicks,
//               u64 lastSavedTick, u8 generationStage
//
// The v4 body is bit-packed through Anvil's BitPacker and may be framed by the
// Codec compressor; the checksum always covers the uncompressed body, so it
// stays comparable across repacking.

enum class SaveVersion : uint32_t {
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
};

constexpr SaveVersion currentSaveVersion = SaveVersion::V4;
constexpr SaveVersion oldestSaveVersion = SaveVersion::V1;

constexpr uint32_t saveMagic = 0x56585356u;  // "VXSV"
constexpr std::size_t saveHeaderBytes = 28;
constexpr std::size_t saveEntryBytes = 24;
constexpr uint32_t saveSectorBytes = 256;

// The vertical span a chunk coordinate may occupy, in chunk units. Anything
// outside is a corrupt coordinate, not a tall world.
constexpr int saveChunkYMin = -4;
constexpr int saveChunkYMax = 19;

constexpr uint32_t saveRecordEmpty = 1u;       // v4 flag: tombstone, holds no blocks
constexpr uint32_t saveRecordCompressed = 2u;  // v4 flag: body framed by compress()

constexpr uint8_t saveStageComplete = 3;
constexpr uint8_t saveDefaultSkyLight = 15;

// Per-chunk bookkeeping introduced by v4. Chunks lifted from v1..v3 get these
// defaults: no recorded playtime, saved at tick 0, fully generated.
struct SaveChunkMeta {
    uint64_t inhabitedTicks = 0;
    uint64_t lastSavedTick = 0;
    uint8_t generationStage = saveStageComplete;
};

// One chunk as the container sees it: where it lives, its blocks, its light
// plane and its metadata. `light` is empty for v1/v2 containers and holds
// chunkVolume bytes of (sky << 4) | block from v3 on.
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

// What a payload record turned out to contain, filled while decoding so the
// scanner never has to parse the same bytes twice.
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

// Big-endian field access at an absolute byte offset, bounds checked. The
// scanner and the compactor both need random access into a buffer they may not
// be able to parse from the front.
bool saveReadU32At(const uint8_t* data, std::size_t size, std::size_t at, uint32_t& out);
bool saveWriteU32At(uint8_t* data, std::size_t size, std::size_t at, uint32_t value);

bool isKnownSaveVersion(uint32_t version);
bool versionHasPalette(SaveVersion version);
bool versionHasLight(SaveVersion version);
bool versionHasChecksum(SaveVersion version);
bool versionHasMetadata(SaveVersion version);
const char* saveVersionName(SaveVersion version);

// Reads the header without trusting any of it beyond the buffer length.
bool readSaveHeader(const uint8_t* data, std::size_t size, SaveHeader& out);
std::size_t saveTableBytes(const SaveHeader& header);
std::size_t saveHeaderSectors(const SaveHeader& header);
std::size_t saveDataStart(const SaveHeader& header);

// Reads as many entries as the buffer actually holds; `out` may end up shorter
// than header.chunkCount, which is itself a finding for the scanner.
bool readSaveTable(const uint8_t* data, std::size_t size, const SaveHeader& header,
                   std::vector<SaveTableEntry>& out);

// The light plane a chunk gets when a version that has none is lifted: sky is
// blocked by anything opaque, block light is the emission of the block itself.
std::vector<uint8_t> defaultChunkLight(const Chunk& chunk, uint8_t skyLevel);
bool chunkIsAllAir(const Chunk& chunk);

ByteBuffer encodeChunkPayload(SaveVersion version, const SaveChunkRecord& record,
                              bool compressBody);

// Structural decode. A wrong checksum does NOT fail the decode - the scanner
// reports it separately, and repair needs the blocks out of a record whose
// checksum went stale.
bool decodeChunkPayload(SaveVersion version, const uint8_t* data, std::size_t size,
                        SaveChunkRecord& out, SavePayloadInfo* info = nullptr);

// Pulls the stored checksum and the one the bytes actually hash to. False for
// v1/v2, which carry none.
bool verifyChunkPayloadChecksum(SaveVersion version, const uint8_t* data, std::size_t size,
                                uint32_t& stored, uint32_t& computed);

// Lays out a container from payloads that are already encoded: header, entry
// table, then every payload back to back on sector boundaries. `coords` must
// already be in ascending coordinate order - this is the one place that decides
// what a canonical buffer looks like, and the writer, the compactor and the
// backup restore all go through it so their output is comparable byte for byte.
ByteBuffer packSaveContainer(SaveVersion version, int regionX, int regionZ, uint32_t sectorBytes,
                             const std::vector<Vec3i>& coords,
                             const std::vector<ByteBuffer>& payloads,
                             std::vector<uint32_t>* sectorOffsets = nullptr);

bool saveCoordLess(const Vec3i& a, const Vec3i& b);

// Writes a canonical container: entries sorted by coordinate, payloads packed
// back to back on sector boundaries, no holes, buffer ending on a sector edge.
// Deterministic - the same container always produces the same bytes.
ByteBuffer writeSaveContainer(const SaveContainer& container);
bool readSaveContainer(const uint8_t* data, std::size_t size, SaveContainer& out);

// Copies one chunk's payload record out verbatim, for byte-level comparison.
bool readChunkPayloadBytes(const uint8_t* data, std::size_t size, const Vec3i& coord,
                           ByteBuffer& out);

// True when the layout matches what writeSaveContainer would produce. Backups
// require it, because they store payloads rather than layout.
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

// Lifts a container one version at a time. Every step is a real transcode: the
// intermediate buffer is a valid container of the intermediate version, so a
// v1 save passes through readable v2 and v3 states on its way to v4.
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

} // namespace vf
