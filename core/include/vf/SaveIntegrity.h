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

// Consistency control for a region save container (format in SaveMigration.h).
//
// The scanner never trusts a field before it has been bounded by the buffer it
// came from, so a save damaged in any way still produces a report rather than a
// crash. Every finding carries a severity, a location and a sentence a person
// can act on; the repair planner turns the recoverable ones into explicit
// actions, and applying them rewrites the container while leaving every healthy
// chunk byte for byte where it was.

enum class IssueSeverity : uint8_t {
    Info = 0,          // worth mentioning, costs nothing
    Warning = 1,       // stale or sloppy, still readable
    Recoverable = 2,   // damage the repair planner can act on
    Fatal = 3,         // the container cannot be interpreted at all
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

// Deterministic replacement terrain for a chunk that cannot be salvaged: an
// integer value-noise surface over a 16 block lattice, seeded by the world seed
// and the chunk coordinate only. The same seed always rebuilds the same blocks.
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

} // namespace vf
