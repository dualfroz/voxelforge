#include "vf/SaveIntegrity.h"

#include <algorithm>
#include <array>

#include "vf/Block.h"
#include "vf/Region.h"

namespace vf {

namespace {

std::string decimal(long long value) { return std::to_string(value); }

std::string coordText(const Vec3i& coord) {
    return "(" + decimal(coord.x) + ", " + decimal(coord.y) + ", " + decimal(coord.z) + ")";
}

uint64_t mixBits(uint64_t value) {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

uint64_t latticeHash(uint64_t seed, int gx, int gz) {
    uint64_t h = seed;
    h = mixBits(h ^ (static_cast<uint64_t>(static_cast<uint32_t>(gx)) * 0x2545F4914F6CDD1Dull));
    h = mixBits(h ^ (static_cast<uint64_t>(static_cast<uint32_t>(gz)) * 0x9E3779B97F4A7C15ull));
    return h;
}

int latticeHeight(uint64_t seed, int gx, int gz) {
    return 32 + static_cast<int>(latticeHash(seed, gx, gz) % 33ull);
}

struct SectorSpan {
    uint32_t first = 0;
    uint32_t last = 0;
    int entryIndex = -1;
};

IntegrityIssue makeIssue(IntegrityIssueKind kind, IssueSeverity severity, std::size_t byteOffset,
                         std::string detail) {
    IntegrityIssue issue;
    issue.kind = kind;
    issue.severity = severity;
    issue.byteOffset = byteOffset;
    issue.detail = std::move(detail);
    return issue;
}

bool actionOutranks(RepairActionKind candidate, RepairActionKind existing) {
    auto rank = [](RepairActionKind kind) {
        switch (kind) {
            case RepairActionKind::RecomputeChecksum: return 0;
            case RepairActionKind::RegenerateChunk: return 1;
            case RepairActionKind::RemoveOrphanEntry: return 2;
            case RepairActionKind::DropChunk: return 3;
            case RepairActionKind::TruncateBuffer: return 4;
        }
        return 0;
    };
    return rank(candidate) > rank(existing);
}

} // namespace

const char* issueSeverityName(IssueSeverity severity) {
    switch (severity) {
        case IssueSeverity::Info: return "info";
        case IssueSeverity::Warning: return "warning";
        case IssueSeverity::Recoverable: return "recoverable";
        case IssueSeverity::Fatal: return "fatal";
    }
    return "unknown";
}

const char* integrityIssueName(IntegrityIssueKind kind) {
    switch (kind) {
        case IntegrityIssueKind::BadMagic: return "badMagic";
        case IntegrityIssueKind::UnknownVersion: return "unknownVersion";
        case IntegrityIssueKind::MalformedHeader: return "malformedHeader";
        case IntegrityIssueKind::TruncatedBuffer: return "truncatedBuffer";
        case IntegrityIssueKind::ChunkCountOverrun: return "chunkCountOverrun";
        case IntegrityIssueKind::OrphanEntry: return "orphanEntry";
        case IntegrityIssueKind::OverlappingEntries: return "overlappingEntries";
        case IntegrityIssueKind::ChunkOutsideRegion: return "chunkOutsideRegion";
        case IntegrityIssueKind::ChunkHeightOutOfRange: return "chunkHeightOutOfRange";
        case IntegrityIssueKind::MalformedRecord: return "malformedRecord";
        case IntegrityIssueKind::PaletteIndexOutOfRange: return "paletteIndexOutOfRange";
        case IntegrityIssueKind::ChecksumMismatch: return "checksumMismatch";
        case IntegrityIssueKind::LightingInconsistent: return "lightingInconsistent";
        case IntegrityIssueKind::TrailingBytes: return "trailingBytes";
    }
    return "unknown";
}

const char* repairActionName(RepairActionKind kind) {
    switch (kind) {
        case RepairActionKind::DropChunk: return "dropChunk";
        case RepairActionKind::TruncateBuffer: return "truncateBuffer";
        case RepairActionKind::RecomputeChecksum: return "recomputeChecksum";
        case RepairActionKind::RegenerateChunk: return "regenerateChunk";
        case RepairActionKind::RemoveOrphanEntry: return "removeOrphanEntry";
    }
    return "unknown";
}

bool IntegrityReport::fatal() const {
    for (const IntegrityIssue& issue : issues)
        if (issue.severity == IssueSeverity::Fatal) return true;
    return false;
}

bool IntegrityReport::repairable() const {
    if (fatal()) return false;
    for (const IntegrityIssue& issue : issues)
        if (issue.severity == IssueSeverity::Recoverable) return true;
    return false;
}

std::size_t IntegrityReport::countOf(IntegrityIssueKind kind) const {
    std::size_t total = 0;
    for (const IntegrityIssue& issue : issues)
        if (issue.kind == kind) ++total;
    return total;
}

std::size_t IntegrityReport::countAtLeast(IssueSeverity severity) const {
    std::size_t total = 0;
    for (const IntegrityIssue& issue : issues)
        if (issue.severity >= severity) ++total;
    return total;
}

const IntegrityIssue* IntegrityReport::firstOf(IntegrityIssueKind kind) const {
    for (const IntegrityIssue& issue : issues)
        if (issue.kind == kind) return &issue;
    return nullptr;
}

const RepairAction* RepairPlan::forEntry(int entryIndex) const {
    for (const RepairAction& action : actions)
        if (action.entryIndex == entryIndex) return &action;
    return nullptr;
}

std::size_t RepairPlan::countOf(RepairActionKind kind) const {
    std::size_t total = 0;
    for (const RepairAction& action : actions)
        if (action.kind == kind) ++total;
    return total;
}

int regeneratedSurfaceHeight(uint64_t seed, int worldX, int worldZ) {
    int gx = floorDiv(worldX, chunkSize);
    int gz = floorDiv(worldZ, chunkSize);
    int fx = floorMod(worldX, chunkSize);
    int fz = floorMod(worldZ, chunkSize);

    int h00 = latticeHeight(seed, gx, gz);
    int h10 = latticeHeight(seed, gx + 1, gz);
    int h01 = latticeHeight(seed, gx, gz + 1);
    int h11 = latticeHeight(seed, gx + 1, gz + 1);

    int nearEdge = h00 * (chunkSize - fx) + h10 * fx;
    int farEdge = h01 * (chunkSize - fx) + h11 * fx;
    return (nearEdge * (chunkSize - fz) + farEdge * fz) / (chunkSize * chunkSize);
}

Chunk regenerateChunkFromSeed(uint64_t seed, const Vec3i& coord) {
    Chunk chunk;
    int baseY = coord.y * chunkSize;
    for (int x = 0; x < chunkSize; ++x) {
        for (int z = 0; z < chunkSize; ++z) {
            int surface = regeneratedSurfaceHeight(seed, coord.x * chunkSize + x,
                                                   coord.z * chunkSize + z);
            for (int y = 0; y < chunkSize; ++y) {
                int worldY = baseY + y;
                Block block;
                if (worldY > surface) {
                    block = Block::Air;
                } else if (worldY == surface) {
                    block = Block::Grass;
                } else if (worldY > surface - 4) {
                    block = Block::Dirt;
                } else {
                    block = Block::Stone;
                }
                chunk.set(x, y, z, block);
            }
        }
    }
    return chunk;
}

IntegrityReport SaveScanner::scan(const ByteBuffer& buffer) const {
    return scan(buffer.data(), buffer.size());
}

IntegrityReport SaveScanner::scan(const uint8_t* data, std::size_t size) const {
    IntegrityReport report;
    report.bufferBytes = size;

    SaveHeader header;
    if (!readSaveHeader(data, size, header)) {
        report.issues.push_back(makeIssue(
            IntegrityIssueKind::TruncatedBuffer, IssueSeverity::Fatal, size,
            "buffer holds " + decimal(static_cast<long long>(size)) +
                " bytes, too short for the " + decimal(static_cast<long long>(saveHeaderBytes)) +
                " byte header"));
        return report;
    }
    if (header.magic != saveMagic) {
        report.issues.push_back(makeIssue(IntegrityIssueKind::BadMagic, IssueSeverity::Fatal, 0,
                                          "magic is not a save container"));
        return report;
    }
    if (!isKnownSaveVersion(header.version)) {
        report.issues.push_back(
            makeIssue(IntegrityIssueKind::UnknownVersion, IssueSeverity::Fatal, 4,
                      "format version " + decimal(static_cast<long long>(header.version)) +
                          " is outside the supported range v1..v4"));
        return report;
    }
    if (header.sectorBytes == 0) {
        report.issues.push_back(makeIssue(IntegrityIssueKind::MalformedHeader, IssueSeverity::Fatal,
                                          20, "header declares a sector size of zero"));
        return report;
    }

    report.headerReadable = true;
    report.version = static_cast<SaveVersion>(header.version);
    report.regionX = header.regionX;
    report.regionZ = header.regionZ;
    report.declaredChunks = header.chunkCount;

    std::size_t tableEnd = saveHeaderBytes + saveTableBytes(header);
    std::size_t availableEntries = header.chunkCount;
    if (tableEnd > size) {
        availableEntries = size > saveHeaderBytes ? (size - saveHeaderBytes) / saveEntryBytes : 0;
        report.issues.push_back(makeIssue(
            IntegrityIssueKind::ChunkCountOverrun, IssueSeverity::Recoverable, 16,
            "header declares " + decimal(static_cast<long long>(header.chunkCount)) +
                " chunks but only " + decimal(static_cast<long long>(availableEntries)) +
                " table entries fit in the buffer"));
    }

    SaveHeader readable = header;
    readable.chunkCount = static_cast<uint32_t>(availableEntries);
    std::vector<SaveTableEntry> entries;
    readSaveTable(data, size, readable, entries);
    report.tableEntries = entries.size();

    if (versionHasChecksum(report.version) && tableEnd <= size) {
        uint32_t computed = fnv1a32(data + saveHeaderBytes, saveTableBytes(header));
        if (computed != header.tableChecksum) {
            report.issues.push_back(makeIssue(
                IntegrityIssueKind::ChecksumMismatch, IssueSeverity::Recoverable, 24,
                "entry table checksum is " + decimal(static_cast<long long>(header.tableChecksum)) +
                    " but the table hashes to " + decimal(static_cast<long long>(computed))));
        }
    }

    RegionCoord expectedRegion{header.regionX, header.regionZ};
    std::size_t headerSectors = saveHeaderSectors(readable);
    std::vector<SectorSpan> spans;
    std::size_t lastSectorEnd = headerSectors;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const SaveTableEntry& entry = entries[i];
        std::size_t entryAt = saveHeaderBytes + i * saveEntryBytes;
        int index = static_cast<int>(i);

        auto locate = [&](IntegrityIssue& issue) {
            issue.regionX = header.regionX;
            issue.regionZ = header.regionZ;
            issue.entryIndex = index;
            issue.hasChunkCoord = true;
            issue.chunkCoord = entry.coord;
        };

        bool coordBroken = false;
        if (!(regionForChunk(entry.coord) == expectedRegion)) {
            RegionCoord actual = regionForChunk(entry.coord);
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::ChunkOutsideRegion, IssueSeverity::Recoverable, entryAt,
                "chunk " + coordText(entry.coord) + " belongs to region (" + decimal(actual.x) +
                    ", " + decimal(actual.z) + "), not (" + decimal(header.regionX) + ", " +
                    decimal(header.regionZ) + ")");
            locate(issue);
            report.issues.push_back(issue);
            coordBroken = true;
        }
        if (entry.coord.y < saveChunkYMin || entry.coord.y > saveChunkYMax) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::ChunkHeightOutOfRange, IssueSeverity::Recoverable, entryAt + 4,
                "chunk Y " + decimal(entry.coord.y) + " is outside the world range " +
                    decimal(saveChunkYMin) + ".." + decimal(saveChunkYMax));
            locate(issue);
            report.issues.push_back(issue);
            coordBroken = true;
        }

        std::size_t payloadAt = static_cast<std::size_t>(entry.sectorOffset) * header.sectorBytes;
        if (entry.sectorCount == 0 || payloadAt >= size) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::OrphanEntry, IssueSeverity::Recoverable, entryAt + 12,
                "entry for chunk " + coordText(entry.coord) + " points at sector " +
                    decimal(entry.sectorOffset) + " spanning " + decimal(entry.sectorCount) +
                    " sectors, which holds no payload");
            locate(issue);
            report.issues.push_back(issue);
            continue;
        }

