#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Chunk.h"
#include "Codec.h"
#include "Math.h"
#include "SaveMigration.h"

namespace vf {

enum class IssueSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Recoverable = 2,
    Fatal = 3,
};

enum class IntegrityIssueKind : uint8_t {
    BadMagic,
    UnknownVersion,
    MalformedHeader,
    TruncatedBuffer,
    ChunkCountOverrun,
    OrphanEntry,
    OverlappingEntries,
    ChunkOutsideRegion,
    ChunkHeightOutOfRange,
    MalformedRecord,
    PaletteIndexOutOfRange,
    ChecksumMismatch,
    LightingInconsistent,
    TrailingBytes,
};

const char* issueSeverityName(IssueSeverity severity);
const char* integrityIssueName(IntegrityIssueKind kind);

struct IntegrityIssue {
    IntegrityIssueKind kind = IntegrityIssueKind::BadMagic;
    IssueSeverity severity = IssueSeverity::Fatal;
    int regionX = 0;
    int regionZ = 0;
    int entryIndex = -1;
    bool hasChunkCoord = false;
    Vec3i chunkCoord;
    std::size_t byteOffset = 0;
    std::string detail;
};

struct IntegrityReport {
    bool headerReadable = false;
    SaveVersion version = currentSaveVersion;
    int regionX = 0;
    int regionZ = 0;
    std::size_t bufferBytes = 0;
    std::size_t declaredChunks = 0;
    std::size_t tableEntries = 0;
    std::size_t readableChunks = 0;
    std::vector<IntegrityIssue> issues;

    bool clean() const { return issues.empty(); }
    bool fatal() const;
    bool repairable() const;
    std::size_t countOf(IntegrityIssueKind kind) const;
    std::size_t countAtLeast(IssueSeverity severity) const;
    const IntegrityIssue* firstOf(IntegrityIssueKind kind) const;
};

enum class RepairActionKind : uint8_t {
    DropChunk,
    TruncateBuffer,
    RecomputeChecksum,
    RegenerateChunk,
    RemoveOrphanEntry,
};

const char* repairActionName(RepairActionKind kind);

struct RepairAction {
    RepairActionKind kind = RepairActionKind::DropChunk;
    int entryIndex = -1;
    bool hasChunkCoord = false;
    Vec3i chunkCoord;
    std::size_t truncateTo = 0;
    std::string reason;
};

struct RepairPlan {
    bool possible = false;
    std::string refusal;
    uint64_t worldSeed = 0;
    std::vector<RepairAction> actions;

    const RepairAction* forEntry(int entryIndex) const;
    std::size_t countOf(RepairActionKind kind) const;
};

struct RepairResult {
    bool ok = false;
    ByteBuffer output;
    std::vector<RepairAction> applied;
    std::size_t chunksPreserved = 0;
    std::size_t chunksDropped = 0;
    std::size_t chunksRegenerated = 0;
    std::size_t checksumsRewritten = 0;
    std::size_t orphanEntriesRemoved = 0;
    std::size_t bytesLost = 0;
    std::string failure;
};

Chunk regenerateChunkFromSeed(uint64_t seed, const Vec3i& coord);
int regeneratedSurfaceHeight(uint64_t seed, int worldX, int worldZ);

class SaveScanner {
public:
    uint64_t worldSeed = 0;
    bool allowRegeneration = true;
    bool checkPaletteIndices = true;
    bool checkLighting = true;

    IntegrityReport scan(const uint8_t* data, std::size_t size) const;
    IntegrityReport scan(const ByteBuffer& buffer) const;

    RepairPlan planRepair(const uint8_t* data, std::size_t size,
                          const IntegrityReport& report) const;
    RepairResult repair(const uint8_t* data, std::size_t size, const RepairPlan& plan) const;
    RepairResult scanAndRepair(const ByteBuffer& buffer) const;
};

}
