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

}

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

        std::size_t runBytes = static_cast<std::size_t>(entry.sectorCount) * header.sectorBytes;
        spans.push_back(SectorSpan{entry.sectorOffset, entry.sectorOffset + entry.sectorCount,
                                   index});
        lastSectorEnd = std::max<std::size_t>(
            lastSectorEnd, static_cast<std::size_t>(entry.sectorOffset) + entry.sectorCount);

        if (entry.payloadBytes > runBytes) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::MalformedRecord, IssueSeverity::Recoverable, entryAt + 20,
                "entry claims " + decimal(entry.payloadBytes) + " payload bytes inside a " +
                    decimal(static_cast<long long>(runBytes)) + " byte sector run");
            locate(issue);
            report.issues.push_back(issue);
            continue;
        }
        if (payloadAt + entry.payloadBytes > size) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::TruncatedBuffer, IssueSeverity::Recoverable, payloadAt,
                "payload of chunk " + coordText(entry.coord) + " runs to byte " +
                    decimal(static_cast<long long>(payloadAt + entry.payloadBytes)) +
                    " but the buffer ends at " + decimal(static_cast<long long>(size)));
            locate(issue);
            report.issues.push_back(issue);
            continue;
        }

        SaveChunkRecord record;
        SavePayloadInfo info;
        record.coord = entry.coord;
        if (!decodeChunkPayload(report.version, data + payloadAt, entry.payloadBytes, record,
                                &info)) {
            IntegrityIssue issue =
                makeIssue(IntegrityIssueKind::MalformedRecord, IssueSeverity::Recoverable, payloadAt,
                          "payload of chunk " + coordText(entry.coord) +
                              " does not decode as a " + saveVersionName(report.version) +
                              " record");
            locate(issue);
            report.issues.push_back(issue);
            continue;
        }

        if (checkPaletteIndices && versionHasPalette(report.version) &&
            info.maxIndex >= info.paletteCount) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::PaletteIndexOutOfRange, IssueSeverity::Recoverable, payloadAt,
                "chunk " + coordText(entry.coord) + " uses palette index " +
                    decimal(info.maxIndex) + " but the palette holds " +
                    decimal(info.paletteCount) + " entries");
            locate(issue);
            report.issues.push_back(issue);
        }

        if (checkLighting && versionHasLight(report.version)) {
            if (info.lightBytes != static_cast<uint32_t>(chunkVolume)) {
                IntegrityIssue issue = makeIssue(
                    IntegrityIssueKind::LightingInconsistent, IssueSeverity::Recoverable, payloadAt,
                    "chunk " + coordText(entry.coord) + " carries " + decimal(info.lightBytes) +
                        " light bytes, expected " + decimal(chunkVolume));
                locate(issue);
                report.issues.push_back(issue);
            } else {
                std::size_t disagreeing = 0;
                const std::array<Block, chunkVolume>& blocks = record.blocks.raw();
                for (std::size_t cell = 0; cell < blocks.size(); ++cell) {
                    uint8_t sky = static_cast<uint8_t>(record.light[cell] >> 4);
                    uint8_t emitted = static_cast<uint8_t>(record.light[cell] & 0x0Fu);
                    if (occludesLight(blocks[cell]) && sky != 0) ++disagreeing;
                    else if (emitted < static_cast<uint8_t>(blockLightEmission(blocks[cell])))
                        ++disagreeing;
                }
                if (disagreeing != 0) {
                    IntegrityIssue issue = makeIssue(
                        IntegrityIssueKind::LightingInconsistent, IssueSeverity::Warning, payloadAt,
                        "chunk " + coordText(entry.coord) + " has " +
                            decimal(static_cast<long long>(disagreeing)) +
                            " light cells that disagree with their blocks");
                    locate(issue);
                    report.issues.push_back(issue);
                }
            }
        }

        if (versionHasChecksum(report.version)) {
            uint32_t stored = 0;
            uint32_t computed = 0;
            if (verifyChunkPayloadChecksum(report.version, data + payloadAt, entry.payloadBytes,
                                           stored, computed) &&
                stored != computed) {
                IntegrityIssue issue = makeIssue(
                    IntegrityIssueKind::ChecksumMismatch, IssueSeverity::Recoverable,
                    payloadAt + entry.payloadBytes - 4,
                    "chunk " + coordText(entry.coord) + " stores checksum " + decimal(stored) +
                        " but its body hashes to " + decimal(computed));
                locate(issue);
                report.issues.push_back(issue);
            }
        }

        if (!coordBroken) ++report.readableChunks;
    }

    std::sort(spans.begin(), spans.end(), [](const SectorSpan& a, const SectorSpan& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.entryIndex < b.entryIndex;
    });
    uint32_t reach = static_cast<uint32_t>(headerSectors);
    int reachOwner = -1;
    for (const SectorSpan& span : spans) {
        if (span.first < reach) {
            IntegrityIssue issue = makeIssue(
                IntegrityIssueKind::OverlappingEntries, IssueSeverity::Recoverable,
                saveHeaderBytes + static_cast<std::size_t>(span.entryIndex) * saveEntryBytes + 12,
                "sector run [" + decimal(span.first) + ", " + decimal(span.last) +
                    ") overlaps " +
                    (reachOwner < 0 ? std::string("the header and entry table")
                                    : "entry #" + decimal(reachOwner)) +
                    " which reaches sector " + decimal(reach));
            issue.regionX = header.regionX;
            issue.regionZ = header.regionZ;
            issue.entryIndex = span.entryIndex;
            issue.hasChunkCoord = true;
            issue.chunkCoord = entries[static_cast<std::size_t>(span.entryIndex)].coord;
            report.issues.push_back(issue);
        }
        if (span.last > reach) {
            reach = span.last;
            reachOwner = span.entryIndex;
        }
    }

    std::size_t usedEnd = lastSectorEnd * static_cast<std::size_t>(header.sectorBytes);
    if (size > usedEnd) {
        report.issues.push_back(makeIssue(
            IntegrityIssueKind::TrailingBytes, IssueSeverity::Info, usedEnd,
            decimal(static_cast<long long>(size - usedEnd)) +
                " bytes sit past the last sector run and belong to no chunk"));
    }
    return report;
}

RepairPlan SaveScanner::planRepair(const uint8_t* data, std::size_t size,
                                   const IntegrityReport& report) const {
    RepairPlan plan;
    plan.worldSeed = worldSeed;

    if (!report.headerReadable || report.fatal()) {
        plan.possible = false;
        plan.refusal =
            "the container header cannot be interpreted, so there is nothing to salvage chunks "
            "from; restore from a backup instead";
        return plan;
    }

    SaveHeader header;
    if (!readSaveHeader(data, size, header) || header.magic != saveMagic ||
        static_cast<SaveVersion>(header.version) != report.version ||
        report.bufferBytes != size) {
        plan.possible = false;
        plan.refusal = "the report was produced for a different buffer than the one handed in";
        return plan;
    }

    std::vector<RepairAction> bufferActions;
    std::vector<int> entryOrder;
    std::vector<RepairAction> entryActions;

    auto recordForEntry = [&](int entryIndex, RepairActionKind kind, const Vec3i& coord,
                              bool hasCoord, const std::string& reason) {
        for (std::size_t i = 0; i < entryOrder.size(); ++i) {
            if (entryOrder[i] != entryIndex) continue;
            if (actionOutranks(kind, entryActions[i].kind)) {
                entryActions[i].kind = kind;
                entryActions[i].reason = reason;
            }
            return;
        }
        RepairAction action;
        action.kind = kind;
        action.entryIndex = entryIndex;
        action.hasChunkCoord = hasCoord;
        action.chunkCoord = coord;
        action.reason = reason;
        entryOrder.push_back(entryIndex);
        entryActions.push_back(action);
    };

    for (const IntegrityIssue& issue : report.issues) {
        switch (issue.kind) {
            case IntegrityIssueKind::TrailingBytes: {
                RepairAction action;
                action.kind = RepairActionKind::TruncateBuffer;
                action.truncateTo = issue.byteOffset;
                action.reason = "drop the bytes past the last sector run";
                bufferActions.push_back(action);
                break;
            }
            case IntegrityIssueKind::ChunkCountOverrun: {
                RepairAction action;
                action.kind = RepairActionKind::TruncateBuffer;
                action.truncateTo =
                    saveHeaderBytes + report.tableEntries * saveEntryBytes;
                action.reason = "keep only the " + decimal(static_cast<long long>(report.tableEntries)) +
                                " table entries the buffer actually holds";
                bufferActions.push_back(action);
                break;
            }
            case IntegrityIssueKind::ChecksumMismatch:
                if (issue.entryIndex < 0) {
                    RepairAction action;
                    action.kind = RepairActionKind::RecomputeChecksum;
                    action.reason = "rewrite the entry table checksum over the salvaged table";
                    bufferActions.push_back(action);
                } else {
                    recordForEntry(issue.entryIndex, RepairActionKind::RecomputeChecksum,
                                   issue.chunkCoord, issue.hasChunkCoord,
                                   "rewrite the payload checksum, the body still decodes");
                }
                break;
            case IntegrityIssueKind::OrphanEntry:
                recordForEntry(issue.entryIndex, RepairActionKind::RemoveOrphanEntry,
                               issue.chunkCoord, issue.hasChunkCoord,
                               "drop the table entry, it addresses no payload");
                break;
            case IntegrityIssueKind::PaletteIndexOutOfRange:
            case IntegrityIssueKind::LightingInconsistent: {
                if (issue.severity != IssueSeverity::Recoverable) break;
                RepairActionKind kind = allowRegeneration ? RepairActionKind::RegenerateChunk
                                                          : RepairActionKind::DropChunk;
                std::string reason = allowRegeneration
                                         ? "rebuild the chunk from the world seed, its stored "
                                           "body no longer describes valid blocks"
                                         : "drop the chunk, regeneration is disabled";
                recordForEntry(issue.entryIndex, kind, issue.chunkCoord, issue.hasChunkCoord,
                               reason);
                break;
            }
            case IntegrityIssueKind::TruncatedBuffer:
            case IntegrityIssueKind::MalformedRecord:
            case IntegrityIssueKind::OverlappingEntries:
            case IntegrityIssueKind::ChunkOutsideRegion:
            case IntegrityIssueKind::ChunkHeightOutOfRange:
                recordForEntry(issue.entryIndex, RepairActionKind::DropChunk, issue.chunkCoord,
                               issue.hasChunkCoord,
                               "drop the chunk, its entry cannot be trusted: " +
                                   std::string(integrityIssueName(issue.kind)));
                break;
            case IntegrityIssueKind::BadMagic:
            case IntegrityIssueKind::UnknownVersion:
            case IntegrityIssueKind::MalformedHeader:
                break;
        }
    }

    plan.possible = true;
    plan.actions = std::move(bufferActions);
    std::vector<std::size_t> order(entryOrder.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return entryOrder[a] < entryOrder[b];
    });
    for (std::size_t i : order) plan.actions.push_back(entryActions[i]);
    return plan;
}

RepairResult SaveScanner::repair(const uint8_t* data, std::size_t size,
                                 const RepairPlan& plan) const {
    RepairResult result;
    if (!plan.possible) {
        result.failure = plan.refusal.empty() ? "the repair plan is not applicable" : plan.refusal;
        return result;
    }

    SaveHeader header;
    if (!readSaveHeader(data, size, header) || header.magic != saveMagic ||
        !isKnownSaveVersion(header.version) || header.sectorBytes == 0) {
        result.failure = "the container header cannot be interpreted";
        return result;
    }

    SaveVersion version = static_cast<SaveVersion>(header.version);
    SaveHeader readable = header;
    std::size_t tableEnd = saveHeaderBytes + saveTableBytes(header);
    if (tableEnd > size) {
        readable.chunkCount = static_cast<uint32_t>(
            size > saveHeaderBytes ? (size - saveHeaderBytes) / saveEntryBytes : 0);
    }
    std::vector<SaveTableEntry> entries;
    readSaveTable(data, size, readable, entries);

    SaveContainer rebuilt;
    rebuilt.version = version;
    rebuilt.regionX = header.regionX;
    rebuilt.regionZ = header.regionZ;
    rebuilt.sectorBytes = header.sectorBytes;

    for (const RepairAction& action : plan.actions) {
        if (action.entryIndex < 0) result.applied.push_back(action);
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const SaveTableEntry& entry = entries[i];
        const RepairAction* action = plan.forEntry(static_cast<int>(i));
        std::size_t payloadAt = static_cast<std::size_t>(entry.sectorOffset) * header.sectorBytes;
        bool payloadInBounds = entry.sectorCount != 0 && payloadAt < size &&
                               payloadAt + entry.payloadBytes <= size;

        if (action != nullptr) {
            result.applied.push_back(*action);
            if (action->kind == RepairActionKind::RemoveOrphanEntry) {
                ++result.orphanEntriesRemoved;
                continue;
            }
            if (action->kind == RepairActionKind::DropChunk) {
                ++result.chunksDropped;
                if (payloadInBounds) result.bytesLost += entry.payloadBytes;
                continue;
            }
            if (action->kind == RepairActionKind::RegenerateChunk) {
                SaveChunkRecord record;
                record.coord = entry.coord;
                record.blocks = regenerateChunkFromSeed(plan.worldSeed, entry.coord);
                if (versionHasLight(version))
                    record.light = defaultChunkLight(record.blocks, saveDefaultSkyLight);
                rebuilt.chunks.push_back(std::move(record));
                ++result.chunksRegenerated;
                if (payloadInBounds) result.bytesLost += entry.payloadBytes;
                continue;
            }
            if (action->kind == RepairActionKind::RecomputeChecksum) ++result.checksumsRewritten;
        }

        if (!payloadInBounds) {
            ++result.chunksDropped;
            continue;
        }
        SaveChunkRecord record;
        SavePayloadInfo info;
        record.coord = entry.coord;
        if (!decodeChunkPayload(version, data + payloadAt, entry.payloadBytes, record, &info)) {
            ++result.chunksDropped;
            result.bytesLost += entry.payloadBytes;
            continue;
        }
        if (info.compressed) rebuilt.compressPayloads = true;
        rebuilt.chunks.push_back(std::move(record));
        ++result.chunksPreserved;
    }

    result.output = writeSaveContainer(rebuilt);
    result.ok = true;
    return result;
}

RepairResult SaveScanner::scanAndRepair(const ByteBuffer& buffer) const {
    IntegrityReport report = scan(buffer.data(), buffer.size());
    RepairPlan plan = planRepair(buffer.data(), buffer.size(), report);
    return repair(buffer.data(), buffer.size(), plan);
}

}
