#include "vf/RegionCompact.h"

#include <algorithm>

namespace vf {

namespace {

std::size_t divideUp(std::size_t value, std::size_t unit) {
    return unit == 0 ? 0 : (value + unit - 1) / unit;
}

struct LayoutMeasure {
    bool ok = false;
    SaveHeader header;
    std::vector<SaveTableEntry> entries;
    uint32_t totalSectors = 0;
    uint32_t headerSectors = 0;
    uint32_t referencedSectors = 0;
    uint32_t holeSectors = 0;
    uint32_t excessSectors = 0;
    uint32_t slackBytes = 0;
};

LayoutMeasure measureLayout(const uint8_t* data, std::size_t size) {
    LayoutMeasure measure;
    if (!readSaveHeader(data, size, measure.header)) return measure;
    if (measure.header.magic != saveMagic) return measure;
    if (!isKnownSaveVersion(measure.header.version)) return measure;
    if (measure.header.sectorBytes == 0) return measure;
    if (!readSaveTable(data, size, measure.header, measure.entries)) return measure;

    uint32_t sectorBytes = measure.header.sectorBytes;
    measure.totalSectors = static_cast<uint32_t>(divideUp(size, sectorBytes));
    measure.headerSectors = static_cast<uint32_t>(saveHeaderSectors(measure.header));

    for (const SaveTableEntry& entry : measure.entries) {
        measure.referencedSectors += entry.sectorCount;
        uint32_t needed =
            static_cast<uint32_t>(divideUp(entry.payloadBytes, sectorBytes));
        if (entry.sectorCount > needed) measure.excessSectors += entry.sectorCount - needed;
        uint32_t runBytes = entry.sectorCount * sectorBytes;
        if (runBytes > entry.payloadBytes) measure.slackBytes += runBytes - entry.payloadBytes;
    }

    uint32_t accounted = measure.headerSectors + measure.referencedSectors;
    measure.holeSectors = measure.totalSectors > accounted ? measure.totalSectors - accounted : 0;
    measure.ok = true;
    return measure;
}

void rewriteTableChecksum(ByteBuffer& buffer, const SaveHeader& header) {
    if (!versionHasChecksum(static_cast<SaveVersion>(header.version))) return;
    std::size_t tableBytes = saveTableBytes(header);
    uint32_t checksum = fnv1a32(buffer.data() + saveHeaderBytes, tableBytes);
    saveWriteU32At(buffer.data(), buffer.size(), 24, checksum);
}

} // namespace

bool updateChunkInPlace(ByteBuffer& buffer, const SaveChunkRecord& record, bool compressBody) {
    SaveHeader header;
    if (!readSaveHeader(buffer.data(), buffer.size(), header)) return false;
    if (header.magic != saveMagic || !isKnownSaveVersion(header.version)) return false;
    if (header.sectorBytes == 0) return false;

    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(buffer.data(), buffer.size(), header, entries)) return false;

    std::size_t slot = entries.size();
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (entries[i].coord == record.coord) slot = i;
    if (slot == entries.size()) return false;

    SaveVersion version = static_cast<SaveVersion>(header.version);
    ByteBuffer payload = encodeChunkPayload(version, record, compressBody);
    uint32_t sectorBytes = header.sectorBytes;
    uint32_t needed = static_cast<uint32_t>(divideUp(payload.size(), sectorBytes));
    std::size_t entryAt = saveHeaderBytes + slot * saveEntryBytes;

    if (needed <= entries[slot].sectorCount) {
        std::size_t at = static_cast<std::size_t>(entries[slot].sectorOffset) * sectorBytes;
        std::size_t runBytes = static_cast<std::size_t>(entries[slot].sectorCount) * sectorBytes;
        if (at + runBytes > buffer.size()) return false;
        std::fill(buffer.begin() + static_cast<std::ptrdiff_t>(at),
                  buffer.begin() + static_cast<std::ptrdiff_t>(at + runBytes), uint8_t{0});
        std::copy(payload.begin(), payload.end(),
                  buffer.begin() + static_cast<std::ptrdiff_t>(at));
        saveWriteU32At(buffer.data(), buffer.size(), entryAt + 20,
                       static_cast<uint32_t>(payload.size()));
        rewriteTableChecksum(buffer, header);
        return true;
    }

    if (buffer.size() % sectorBytes != 0)
        buffer.resize(divideUp(buffer.size(), sectorBytes) * sectorBytes, 0);
    uint32_t newOffset = static_cast<uint32_t>(buffer.size() / sectorBytes);
    buffer.resize(buffer.size() + static_cast<std::size_t>(needed) * sectorBytes, 0);
    std::copy(payload.begin(), payload.end(),
              buffer.begin() + static_cast<std::ptrdiff_t>(
                                   static_cast<std::size_t>(newOffset) * sectorBytes));
    saveWriteU32At(buffer.data(), buffer.size(), entryAt + 12, newOffset);
    saveWriteU32At(buffer.data(), buffer.size(), entryAt + 16, needed);
    saveWriteU32At(buffer.data(), buffer.size(), entryAt + 20,
                   static_cast<uint32_t>(payload.size()));
    rewriteTableChecksum(buffer, header);
    return true;
}

std::vector<ChunkFragment> RegionCompactor::survey(const ByteBuffer& buffer) const {
    return survey(buffer.data(), buffer.size());
}

std::vector<ChunkFragment> RegionCompactor::survey(const uint8_t* data, std::size_t size) const {
    std::vector<ChunkFragment> fragments;
    LayoutMeasure measure = measureLayout(data, size);
    if (!measure.ok) return fragments;

    uint32_t sectorBytes = measure.header.sectorBytes;
    SaveVersion version = static_cast<SaveVersion>(measure.header.version);
    for (std::size_t i = 0; i < measure.entries.size(); ++i) {
        const SaveTableEntry& entry = measure.entries[i];
        ChunkFragment fragment;
        fragment.coord = entry.coord;
        fragment.entryIndex = static_cast<int>(i);
        fragment.sectorOffset = entry.sectorOffset;
        fragment.sectorCount = entry.sectorCount;
        fragment.payloadBytes = entry.payloadBytes;
        fragment.neededSectors = static_cast<uint32_t>(divideUp(entry.payloadBytes, sectorBytes));
        fragment.excessSectors = entry.sectorCount > fragment.neededSectors
                                     ? entry.sectorCount - fragment.neededSectors
                                     : 0;
        uint32_t runBytes = entry.sectorCount * sectorBytes;
        fragment.slackBytes = runBytes > entry.payloadBytes ? runBytes - entry.payloadBytes : 0;

        std::size_t at = static_cast<std::size_t>(entry.sectorOffset) * sectorBytes;
        fragment.reachable =
            entry.sectorCount != 0 && at + entry.payloadBytes <= size && at < size;
        if (fragment.reachable && versionHasMetadata(version)) {
            uint32_t flags = 0;
            if (saveReadU32At(data, size, at, flags))
                fragment.empty = (flags & saveRecordEmpty) != 0;
        }
        fragments.push_back(fragment);
    }
    return fragments;
}

int RegionCompactor::fragmentationPercent(const ByteBuffer& buffer) const {
    return fragmentationPercent(buffer.data(), buffer.size());
}

int RegionCompactor::fragmentationPercent(const uint8_t* data, std::size_t size) const {
    LayoutMeasure measure = measureLayout(data, size);
    if (!measure.ok || measure.totalSectors == 0) return -1;
    uint32_t reclaimable = measure.holeSectors + measure.excessSectors;
    return static_cast<int>((reclaimable * 100u) / measure.totalSectors);
}

bool RegionCompactor::compact(const ByteBuffer& in, ByteBuffer& out,
                              CompactionStats& stats) const {
    return compact(in.data(), in.size(), out, stats);
}

bool RegionCompactor::compact(const uint8_t* data, std::size_t size, ByteBuffer& out,
                              CompactionStats& stats) const {
    stats = CompactionStats{};
    LayoutMeasure measure = measureLayout(data, size);
    if (!measure.ok) {
        stats.failure = "buffer is not a readable save container";
        return false;
    }

    SaveScanner scanner;
    IntegrityReport report = scanner.scan(data, size);
    if (report.countAtLeast(IssueSeverity::Recoverable) != 0) {
        stats.failure = "container reports " +
                        std::to_string(report.countAtLeast(IssueSeverity::Recoverable)) +
                        " unresolved integrity findings; repair it before compacting";
        return false;
    }

    SaveVersion version = static_cast<SaveVersion>(measure.header.version);
    uint32_t sectorBytes = measure.header.sectorBytes;

    stats.sectorBytes = sectorBytes;
    stats.sectorsBefore = measure.totalSectors;
    stats.headerSectors = measure.headerSectors;
    stats.referencedSectors = measure.referencedSectors;
    stats.holeSectors = measure.holeSectors;
    stats.excessSectors = measure.excessSectors;
    stats.slackBytes = measure.slackBytes;
    stats.bytesBefore = size;
    stats.fragmentationBefore = fragmentationPercent(data, size);

    std::vector<ChunkFragment> fragments = survey(data, size);
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < fragments.size(); ++i) {
        ++stats.operations;
        if (dropEmptyChunks && fragments[i].empty) {
            ++stats.chunksDropped;
            ++stats.operations;
            continue;
        }
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return saveCoordLess(fragments[a].coord, fragments[b].coord);
    });

    std::vector<Vec3i> coords;
    std::vector<ByteBuffer> payloads;
    coords.reserve(order.size());
    payloads.reserve(order.size());
    bool repack = repackWithStrongerCompression && version == currentSaveVersion;

    for (std::size_t index : order) {
        const ChunkFragment& fragment = fragments[index];
        std::size_t at = static_cast<std::size_t>(fragment.sectorOffset) * sectorBytes;
        ByteBuffer payload(data + at, data + at + fragment.payloadBytes);
        if (repack) {
            SaveChunkRecord record;
            record.coord = fragment.coord;
            if (!decodeChunkPayload(version, payload.data(), payload.size(), record, nullptr)) {
                stats.failure = "a payload stopped decoding while repacking";
                return false;
            }
            ByteBuffer repacked = encodeChunkPayload(version, record, true);
            if (repacked.size() < payload.size()) {
                payload = std::move(repacked);
                ++stats.chunksRepacked;
                ++stats.operations;
            }
        }
        coords.push_back(fragment.coord);
        payloads.push_back(std::move(payload));
    }

    std::vector<uint32_t> newOffsets;
    out = packSaveContainer(version, measure.header.regionX, measure.header.regionZ, sectorBytes,
                            coords, payloads, &newOffsets);
    ++stats.operations;

    for (std::size_t i = 0; i < order.size(); ++i) {
        if (fragments[order[i]].sectorOffset != newOffsets[i]) {
            ++stats.chunksMoved;
            ++stats.operations;
        }
    }

    stats.chunksKept = coords.size();
    stats.bytesAfter = out.size();
    stats.bytesReclaimed = size > out.size() ? size - out.size() : 0;
    stats.sectorsAfter = static_cast<uint32_t>(divideUp(out.size(), sectorBytes));
    stats.fragmentationAfter = fragmentationPercent(out.data(), out.size());
    stats.ok = true;
    return true;
}

} // namespace vf
