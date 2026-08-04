#include "vf/SaveMigration.h"

#include <algorithm>
#include <array>

#include "vf/Anvil.h"
#include "vf/Block.h"

namespace vf {

namespace {

std::size_t divideUp(std::size_t value, std::size_t unit) {
    return unit == 0 ? 0 : (value + unit - 1) / unit;
}

void putU32Be(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
}

uint32_t readU32Be(const uint8_t* at) {
    return (static_cast<uint32_t>(at[0]) << 24) | (static_cast<uint32_t>(at[1]) << 16) |
           (static_cast<uint32_t>(at[2]) << 8) | static_cast<uint32_t>(at[3]);
}

struct PayloadCursor {
    const uint8_t* data;
    std::size_t size;
    std::size_t at = 0;
    bool bad = false;

    uint32_t u32() {
        if (bad || at + 4 > size) {
            bad = true;
            return 0;
        }
        uint32_t value = readU32Be(data + at);
        at += 4;
        return value;
    }
    uint64_t u64() {
        uint32_t high = u32();
        uint32_t low = u32();
        return (static_cast<uint64_t>(high) << 32) | static_cast<uint64_t>(low);
    }
    uint8_t u8() {
        if (bad || at + 1 > size) {
            bad = true;
            return 0;
        }
        return data[at++];
    }
    bool take(std::size_t count, std::vector<uint8_t>& out) {
        if (bad || at + count > size) {
            bad = true;
            return false;
        }
        out.assign(data + at, data + at + count);
        at += count;
        return true;
    }
    bool skip(std::size_t count) {
        if (bad || at + count > size) {
            bad = true;
            return false;
        }
        at += count;
        return true;
    }
};

void writeU64Be(std::vector<uint8_t>& out, uint64_t value) {
    putU32Be(out, static_cast<uint32_t>(value >> 32));
    putU32Be(out, static_cast<uint32_t>(value & 0xFFFFFFFFull));
}

void buildPalette(const Chunk& chunk, std::vector<Block>& palette, std::vector<uint32_t>& indices) {
    std::array<int, static_cast<std::size_t>(Block::Count)> slotOf;
    slotOf.fill(-1);
    palette.clear();
    indices.clear();
    indices.reserve(chunkVolume);
    for (Block block : chunk.raw()) {
        int& slot = slotOf[static_cast<std::size_t>(block)];
        if (slot < 0) {
            slot = static_cast<int>(palette.size());
            palette.push_back(block);
        }
        indices.push_back(static_cast<uint32_t>(slot));
    }
}

std::string versionPair(SaveVersion from, SaveVersion to) {
    return std::string(saveVersionName(from)) + " -> " + saveVersionName(to);
}

} // namespace

const SaveChunkRecord* SaveContainer::find(const Vec3i& coord) const {
    for (const SaveChunkRecord& record : chunks)
        if (record.coord == coord) return &record;
    return nullptr;
}

bool saveReadU32At(const uint8_t* data, std::size_t size, std::size_t at, uint32_t& out) {
    if (at + 4 > size) return false;
    out = readU32Be(data + at);
    return true;
}

bool saveWriteU32At(uint8_t* data, std::size_t size, std::size_t at, uint32_t value) {
    if (at + 4 > size) return false;
    data[at + 0] = static_cast<uint8_t>((value >> 24) & 0xFFu);
    data[at + 1] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    data[at + 2] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    data[at + 3] = static_cast<uint8_t>(value & 0xFFu);
    return true;
}

bool isKnownSaveVersion(uint32_t version) { return version >= 1u && version <= 4u; }

bool versionHasPalette(SaveVersion version) { return version >= SaveVersion::V2; }
bool versionHasLight(SaveVersion version) { return version >= SaveVersion::V3; }
bool versionHasChecksum(SaveVersion version) { return version >= SaveVersion::V3; }
bool versionHasMetadata(SaveVersion version) { return version >= SaveVersion::V4; }

const char* saveVersionName(SaveVersion version) {
    switch (version) {
        case SaveVersion::V1: return "v1";
        case SaveVersion::V2: return "v2";
        case SaveVersion::V3: return "v3";
        case SaveVersion::V4: return "v4";
    }
    return "v?";
}

bool readSaveHeader(const uint8_t* data, std::size_t size, SaveHeader& out) {
    if (size < saveHeaderBytes) return false;
    out.magic = readU32Be(data + 0);
    out.version = readU32Be(data + 4);
    out.regionX = static_cast<int32_t>(readU32Be(data + 8));
    out.regionZ = static_cast<int32_t>(readU32Be(data + 12));
    out.chunkCount = readU32Be(data + 16);
    out.sectorBytes = readU32Be(data + 20);
    out.tableChecksum = readU32Be(data + 24);
    return true;
}

std::size_t saveTableBytes(const SaveHeader& header) {
    return static_cast<std::size_t>(header.chunkCount) * saveEntryBytes;
}

std::size_t saveHeaderSectors(const SaveHeader& header) {
    if (header.sectorBytes == 0) return 0;
    return divideUp(saveHeaderBytes + saveTableBytes(header), header.sectorBytes);
}

std::size_t saveDataStart(const SaveHeader& header) {
    return saveHeaderSectors(header) * static_cast<std::size_t>(header.sectorBytes);
}

bool readSaveTable(const uint8_t* data, std::size_t size, const SaveHeader& header,
                   std::vector<SaveTableEntry>& out) {
    out.clear();
    for (uint32_t i = 0; i < header.chunkCount; ++i) {
        std::size_t at = saveHeaderBytes + static_cast<std::size_t>(i) * saveEntryBytes;
        if (at + saveEntryBytes > size) return false;
        SaveTableEntry entry;
        entry.coord.x = static_cast<int32_t>(readU32Be(data + at + 0));
        entry.coord.y = static_cast<int32_t>(readU32Be(data + at + 4));
        entry.coord.z = static_cast<int32_t>(readU32Be(data + at + 8));
        entry.sectorOffset = readU32Be(data + at + 12);
        entry.sectorCount = readU32Be(data + at + 16);
        entry.payloadBytes = readU32Be(data + at + 20);
        out.push_back(entry);
    }
    return true;
}

std::vector<uint8_t> defaultChunkLight(const Chunk& chunk, uint8_t skyLevel) {
    uint8_t sky = skyLevel > 15 ? 15 : skyLevel;
    std::vector<uint8_t> plane(chunkVolume, 0);
    const std::array<Block, chunkVolume>& blocks = chunk.raw();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        uint8_t skyHere = occludesLight(blocks[i]) ? 0u : sky;
        uint8_t emitted = static_cast<uint8_t>(blockLightEmission(blocks[i]));
        plane[i] = static_cast<uint8_t>((skyHere << 4) | (emitted & 0x0Fu));
    }
    return plane;
}

bool chunkIsAllAir(const Chunk& chunk) {
    for (Block block : chunk.raw())
        if (block != Block::Air) return false;
    return true;
}

ByteBuffer encodeChunkPayload(SaveVersion version, const SaveChunkRecord& record,
                              bool compressBody) {
    ByteBuffer out;
    if (version == SaveVersion::V1) {
        putU32Be(out, static_cast<uint32_t>(chunkVolume));
        for (Block block : record.blocks.raw()) out.push_back(static_cast<uint8_t>(block));
        return out;
    }

    std::vector<Block> palette;
    std::vector<uint32_t> indices;
    buildPalette(record.blocks, palette, indices);

    if (version == SaveVersion::V2 || version == SaveVersion::V3) {
        putU32Be(out, static_cast<uint32_t>(palette.size()));
        for (Block block : palette) out.push_back(static_cast<uint8_t>(block));
        putU32Be(out, static_cast<uint32_t>(indices.size()));
        for (uint32_t index : indices) out.push_back(static_cast<uint8_t>(index));
        if (version == SaveVersion::V3) {
            putU32Be(out, static_cast<uint32_t>(record.light.size()));
            out.insert(out.end(), record.light.begin(), record.light.end());
            putU32Be(out, fnv1a32(out.data(), out.size()));
        }
        return out;
    }

    int bitWidth = BitPacker::bitWidthForPalette(palette.size());
    std::vector<uint64_t> words = BitPacker::pack(indices, bitWidth);

    ByteBuffer body;
    putU32Be(body, static_cast<uint32_t>(palette.size()));
    for (Block block : palette) body.push_back(static_cast<uint8_t>(block));
    putU32Be(body, static_cast<uint32_t>(bitWidth));
    putU32Be(body, static_cast<uint32_t>(words.size()));
    for (uint64_t word : words) writeU64Be(body, word);
    putU32Be(body, static_cast<uint32_t>(record.light.size()));
    body.insert(body.end(), record.light.begin(), record.light.end());
    writeU64Be(body, record.meta.inhabitedTicks);
    writeU64Be(body, record.meta.lastSavedTick);
    body.push_back(record.meta.generationStage);

    uint32_t bodyChecksum = fnv1a32(body.data(), body.size());
    ByteBuffer stored = compressBody ? compress(body) : body;

    uint32_t flags = 0;
    if (record.empty || chunkIsAllAir(record.blocks)) flags |= saveRecordEmpty;
    if (compressBody) flags |= saveRecordCompressed;

    putU32Be(out, flags);
    putU32Be(out, static_cast<uint32_t>(stored.size()));
    out.insert(out.end(), stored.begin(), stored.end());
    putU32Be(out, bodyChecksum);
    return out;
}

namespace {

bool decodeBodyV4(const uint8_t* body, std::size_t bodySize, SaveChunkRecord& out,
                  SavePayloadInfo* info) {
    PayloadCursor cursor{body, bodySize};
    uint32_t paletteCount = cursor.u32();
    if (cursor.bad || paletteCount == 0 || paletteCount > 4096u) return false;
    std::vector<uint8_t> paletteBytes;
    if (!cursor.take(paletteCount, paletteBytes)) return false;

    uint32_t bitWidth = cursor.u32();
    uint32_t wordCount = cursor.u32();
    if (cursor.bad || bitWidth == 0 || bitWidth > 32u) return false;
    int perWord = 64 / static_cast<int>(bitWidth);
    std::size_t neededWords = divideUp(chunkVolume, static_cast<std::size_t>(perWord));
    if (wordCount < neededWords) return false;
    if (wordCount > neededWords + 64u) return false;

    std::vector<uint64_t> words;
    words.reserve(wordCount);
    for (uint32_t i = 0; i < wordCount; ++i) words.push_back(cursor.u64());
    if (cursor.bad) return false;

    uint32_t lightBytes = cursor.u32();
    if (cursor.bad || lightBytes > static_cast<uint32_t>(chunkVolume)) return false;
    if (!cursor.take(lightBytes, out.light)) return false;

    out.meta.inhabitedTicks = cursor.u64();
    out.meta.lastSavedTick = cursor.u64();
    out.meta.generationStage = cursor.u8();
    if (cursor.bad) return false;

    std::vector<uint32_t> indices =
        BitPacker::unpack(words, static_cast<int>(bitWidth), chunkVolume);
    uint32_t maxIndex = 0;
    std::array<Block, chunkVolume>& blocks = out.blocks.raw();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        uint32_t index = indices[i];
        if (index > maxIndex) maxIndex = index;
        blocks[i] = index < paletteCount ? static_cast<Block>(paletteBytes[index]) : Block::Air;
    }
    if (info) {
        info->paletteCount = paletteCount;
        info->maxIndex = maxIndex;
        info->bitWidth = bitWidth;
        info->lightBytes = lightBytes;
        info->rawBodyBytes = bodySize;
    }
    return true;
}

} // namespace

bool decodeChunkPayload(SaveVersion version, const uint8_t* data, std::size_t size,
                        SaveChunkRecord& out, SavePayloadInfo* info) {
    if (info) *info = SavePayloadInfo{};
    out.light.clear();
    out.empty = false;
    out.meta = SaveChunkMeta{};

    if (version == SaveVersion::V1) {
        PayloadCursor cursor{data, size};
        uint32_t blockBytes = cursor.u32();
        if (cursor.bad || blockBytes != static_cast<uint32_t>(chunkVolume)) return false;
        std::vector<uint8_t> raw;
        if (!cursor.take(blockBytes, raw)) return false;
        std::array<Block, chunkVolume>& blocks = out.blocks.raw();
        for (std::size_t i = 0; i < blocks.size(); ++i) blocks[i] = static_cast<Block>(raw[i]);
        out.empty = chunkIsAllAir(out.blocks);
        if (info) info->rawBodyBytes = size;
        return true;
    }

    if (version == SaveVersion::V2 || version == SaveVersion::V3) {
        PayloadCursor cursor{data, size};
        uint32_t paletteCount = cursor.u32();
        if (cursor.bad || paletteCount == 0 || paletteCount > 4096u) return false;
        std::vector<uint8_t> paletteBytes;
        if (!cursor.take(paletteCount, paletteBytes)) return false;
        uint32_t indexBytes = cursor.u32();
        if (cursor.bad || indexBytes != static_cast<uint32_t>(chunkVolume)) return false;
        std::vector<uint8_t> indices;
        if (!cursor.take(indexBytes, indices)) return false;

        uint32_t maxIndex = 0;
        std::array<Block, chunkVolume>& blocks = out.blocks.raw();
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            uint32_t index = indices[i];
            if (index > maxIndex) maxIndex = index;
            blocks[i] = index < paletteCount ? static_cast<Block>(paletteBytes[index]) : Block::Air;
        }
        uint32_t lightBytes = 0;
        if (version == SaveVersion::V3) {
            lightBytes = cursor.u32();
            if (cursor.bad || lightBytes > static_cast<uint32_t>(chunkVolume)) return false;
            if (!cursor.take(lightBytes, out.light)) return false;
            if (!cursor.skip(4)) return false;
        }
        out.empty = chunkIsAllAir(out.blocks);
        if (info) {
            info->paletteCount = paletteCount;
            info->maxIndex = maxIndex;
            info->bitWidth = 8;
            info->lightBytes = lightBytes;
            info->rawBodyBytes = size;
        }
        return true;
    }

    PayloadCursor cursor{data, size};
    uint32_t flags = cursor.u32();
    uint32_t storedBytes = cursor.u32();
    if (cursor.bad || storedBytes > size) return false;
    std::vector<uint8_t> stored;
    if (!cursor.take(storedBytes, stored)) return false;
    if (!cursor.skip(4)) return false;

    ByteBuffer body;
    if ((flags & saveRecordCompressed) != 0) {
        if (!decompress(stored.data(), stored.size(), body)) return false;
    } else {
        body = std::move(stored);
    }
    if (!decodeBodyV4(body.data(), body.size(), out, info)) return false;
    out.empty = (flags & saveRecordEmpty) != 0;
    if (info) {
        info->compressed = (flags & saveRecordCompressed) != 0;
        info->empty = out.empty;
    }
    return true;
}

bool verifyChunkPayloadChecksum(SaveVersion version, const uint8_t* data, std::size_t size,
                                uint32_t& stored, uint32_t& computed) {
    if (!versionHasChecksum(version)) return false;
    if (version == SaveVersion::V3) {
        if (size < 4) return false;
        stored = readU32Be(data + size - 4);
        computed = fnv1a32(data, size - 4);
        return true;
    }
    if (size < 12) return false;
    uint32_t flags = readU32Be(data + 0);
    uint32_t storedBytes = readU32Be(data + 4);
    if (static_cast<std::size_t>(storedBytes) + 12 > size) return false;
    stored = readU32Be(data + 8 + storedBytes);
    if ((flags & saveRecordCompressed) != 0) {
        ByteBuffer body;
        if (!decompress(data + 8, storedBytes, body)) return false;
        computed = fnv1a32(body.data(), body.size());
    } else {
        computed = fnv1a32(data + 8, storedBytes);
    }
    return true;
}

bool saveCoordLess(const Vec3i& a, const Vec3i& b) {
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
}

ByteBuffer packSaveContainer(SaveVersion version, int regionX, int regionZ, uint32_t sectorBytes,
                             const std::vector<Vec3i>& coords,
                             const std::vector<ByteBuffer>& payloads,
                             std::vector<uint32_t>* sectorOffsets) {
    uint32_t sectors = sectorBytes == 0 ? saveSectorBytes : sectorBytes;

    SaveHeader header;
    header.magic = saveMagic;
    header.version = static_cast<uint32_t>(version);
    header.regionX = regionX;
    header.regionZ = regionZ;
    header.chunkCount = static_cast<uint32_t>(coords.size());
    header.sectorBytes = sectors;

    std::size_t sectorCursor = saveHeaderSectors(header);
    if (sectorOffsets) sectorOffsets->clear();

    ByteBuffer table;
    table.reserve(coords.size() * saveEntryBytes);
    std::vector<std::size_t> starts;
    starts.reserve(coords.size());
    for (std::size_t i = 0; i < coords.size(); ++i) {
        uint32_t sectorCount = static_cast<uint32_t>(divideUp(payloads[i].size(), sectors));
        putU32Be(table, static_cast<uint32_t>(coords[i].x));
        putU32Be(table, static_cast<uint32_t>(coords[i].y));
        putU32Be(table, static_cast<uint32_t>(coords[i].z));
        putU32Be(table, static_cast<uint32_t>(sectorCursor));
        putU32Be(table, sectorCount);
        putU32Be(table, static_cast<uint32_t>(payloads[i].size()));
        if (sectorOffsets) sectorOffsets->push_back(static_cast<uint32_t>(sectorCursor));
        starts.push_back(sectorCursor * static_cast<std::size_t>(sectors));
        sectorCursor += sectorCount;
    }

    header.tableChecksum =
        versionHasChecksum(version) ? fnv1a32(table.data(), table.size()) : 0u;

    ByteBuffer out(sectorCursor * static_cast<std::size_t>(sectors), 0);
    saveWriteU32At(out.data(), out.size(), 0, header.magic);
    saveWriteU32At(out.data(), out.size(), 4, header.version);
    saveWriteU32At(out.data(), out.size(), 8, static_cast<uint32_t>(header.regionX));
    saveWriteU32At(out.data(), out.size(), 12, static_cast<uint32_t>(header.regionZ));
    saveWriteU32At(out.data(), out.size(), 16, header.chunkCount);
    saveWriteU32At(out.data(), out.size(), 20, header.sectorBytes);
    saveWriteU32At(out.data(), out.size(), 24, header.tableChecksum);
    std::copy(table.begin(), table.end(), out.begin() + static_cast<std::ptrdiff_t>(saveHeaderBytes));

    for (std::size_t i = 0; i < payloads.size(); ++i)
        std::copy(payloads[i].begin(), payloads[i].end(),
                  out.begin() + static_cast<std::ptrdiff_t>(starts[i]));
    return out;
}

ByteBuffer writeSaveContainer(const SaveContainer& container) {
    std::vector<const SaveChunkRecord*> ordered;
    ordered.reserve(container.chunks.size());
    for (const SaveChunkRecord& record : container.chunks) ordered.push_back(&record);
    std::sort(ordered.begin(), ordered.end(),
              [](const SaveChunkRecord* a, const SaveChunkRecord* b) {
                  return saveCoordLess(a->coord, b->coord);
              });

    bool compressPayloads = container.compressPayloads && container.version == SaveVersion::V4;
    std::vector<Vec3i> coords;
    std::vector<ByteBuffer> payloads;
    coords.reserve(ordered.size());
    payloads.reserve(ordered.size());
    for (const SaveChunkRecord* record : ordered) {
        coords.push_back(record->coord);
        payloads.push_back(encodeChunkPayload(container.version, *record, compressPayloads));
    }
    return packSaveContainer(container.version, container.regionX, container.regionZ,
                             container.sectorBytes, coords, payloads, nullptr);
}

bool readSaveContainer(const uint8_t* data, std::size_t size, SaveContainer& out) {
    SaveHeader header;
    if (!readSaveHeader(data, size, header)) return false;
    if (header.magic != saveMagic) return false;
    if (!isKnownSaveVersion(header.version)) return false;
    if (header.sectorBytes == 0) return false;

    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(data, size, header, entries)) return false;

    out.version = static_cast<SaveVersion>(header.version);
    out.regionX = header.regionX;
    out.regionZ = header.regionZ;
    out.sectorBytes = header.sectorBytes;
    out.compressPayloads = false;
    out.chunks.clear();
    out.chunks.reserve(entries.size());

    for (const SaveTableEntry& entry : entries) {
        std::size_t at = static_cast<std::size_t>(entry.sectorOffset) * header.sectorBytes;
        if (at + entry.payloadBytes > size) return false;
        SaveChunkRecord record;
        SavePayloadInfo info;
        record.coord = entry.coord;
        if (!decodeChunkPayload(out.version, data + at, entry.payloadBytes, record, &info))
            return false;
        if (info.compressed) out.compressPayloads = true;
        out.chunks.push_back(std::move(record));
    }
    return true;
}

bool readChunkPayloadBytes(const uint8_t* data, std::size_t size, const Vec3i& coord,
                           ByteBuffer& out) {
    SaveHeader header;
    if (!readSaveHeader(data, size, header)) return false;
    if (header.magic != saveMagic || header.sectorBytes == 0) return false;
    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(data, size, header, entries)) return false;
    for (const SaveTableEntry& entry : entries) {
        if (!(entry.coord == coord)) continue;
        std::size_t at = static_cast<std::size_t>(entry.sectorOffset) * header.sectorBytes;
        if (at + entry.payloadBytes > size) return false;
        out.assign(data + at, data + at + entry.payloadBytes);
        return true;
    }
    return false;
}

bool isCanonicalSaveLayout(const uint8_t* data, std::size_t size) {
    SaveHeader header;
    if (!readSaveHeader(data, size, header)) return false;
    if (header.magic != saveMagic || !isKnownSaveVersion(header.version)) return false;
    if (header.sectorBytes == 0 || size % header.sectorBytes != 0) return false;

    std::vector<SaveTableEntry> entries;
    if (!readSaveTable(data, size, header, entries)) return false;

    ByteBuffer table(data + saveHeaderBytes, data + saveHeaderBytes + saveTableBytes(header));
    SaveVersion version = static_cast<SaveVersion>(header.version);
    uint32_t expectedTableChecksum =
        versionHasChecksum(version) ? fnv1a32(table.data(), table.size()) : 0u;
    if (header.tableChecksum != expectedTableChecksum) return false;

    std::size_t cursor = saveHeaderSectors(header);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i > 0 && !saveCoordLess(entries[i - 1].coord, entries[i].coord)) return false;
        if (entries[i].sectorOffset != cursor) return false;
        if (entries[i].sectorCount != divideUp(entries[i].payloadBytes, header.sectorBytes))
            return false;
        cursor += entries[i].sectorCount;
    }
    return cursor * static_cast<std::size_t>(header.sectorBytes) == size;
}

SaveContainer saveContainerFromWorld(const World& world, RegionCoord region, SaveVersion version) {
    SaveContainer container;
    container.version = version;
    container.regionX = region.x;
    container.regionZ = region.z;
    for (const auto& entry : world.allChunks()) {
        const Vec3i& coord = entry.first;
        if (!(regionForChunk(coord) == region)) continue;
        if (coord.y < saveChunkYMin || coord.y > saveChunkYMax) continue;
        if (entry.second->empty()) continue;
        SaveChunkRecord record;
        record.coord = coord;
        record.blocks = *entry.second;
        if (versionHasLight(version)) record.light = defaultChunkLight(record.blocks, saveDefaultSkyLight);
        container.chunks.push_back(std::move(record));
    }
    return container;
}

void applySaveContainerToWorld(const SaveContainer& container, World& world) {
    for (const SaveChunkRecord& record : container.chunks) {
        if (record.empty) continue;
        world.ensureChunk(record.coord) = record.blocks;
    }
}

bool SaveMigrator::peekVersion(const uint8_t* data, std::size_t size, SaveVersion& out) {
    SaveHeader header;
    if (!readSaveHeader(data, size, header)) return false;
    if (header.magic != saveMagic) return false;
    if (!isKnownSaveVersion(header.version)) return false;
    out = static_cast<SaveVersion>(header.version);
    return true;
}

MigrationPlan SaveMigrator::planFor(SaveVersion from, SaveVersion to) {
    MigrationPlan plan;
    plan.from = from;
    plan.to = to;
    if (to < from) {
        plan.possible = false;
        plan.refusal = "downgrade " + versionPair(from, to) +
                       " is refused: the newer format carries chunk metadata, a light plane and "
                       "checksums that the older one cannot express, so the step would drop data "
                       "silently";
        return plan;
    }
    plan.possible = true;
    for (uint32_t v = static_cast<uint32_t>(from); v < static_cast<uint32_t>(to); ++v) {
        MigrationStep step;
        step.from = static_cast<SaveVersion>(v);
        step.to = static_cast<SaveVersion>(v + 1);
        if (step.from == SaveVersion::V1)
            step.summary = "raw block ids replaced by a per-chunk palette and an index array";
        else if (step.from == SaveVersion::V2)
            step.summary = "light plane and a payload checksum added";
        else
            step.summary = "chunk metadata added and palette indices bit-packed";
        plan.steps.push_back(step);
    }
    return plan;
}

bool SaveMigrator::applyStep(const ByteBuffer& in, const MigrationStep& step,
                             MigrationResult& result, ByteBuffer& out) const {
    SaveContainer container;
    if (!readSaveContainer(in.data(), in.size(), container)) {
        result.failure = "container is not readable at " + std::string(saveVersionName(step.from));
        return false;
    }
    if (container.version != step.from) {
        result.failure = "expected " + std::string(saveVersionName(step.from)) + " but found " +
                         saveVersionName(container.version);
        return false;
    }

    for (SaveChunkRecord& record : container.chunks) {
        if (step.to == SaveVersion::V2) {
            std::vector<Block> palette;
            std::vector<uint32_t> indices;
            buildPalette(record.blocks, palette, indices);
            result.paletteEntriesCreated += palette.size();
        } else if (step.to == SaveVersion::V3) {
            record.light = defaultChunkLight(record.blocks, skyLightForNewPlanes);
            result.lightCellsInitialized += record.light.size();
        } else {
            record.meta = metadataDefaults;
            ++result.metadataRecordsCreated;
        }
    }

    container.version = step.to;
    container.compressPayloads = compressMigratedPayloads && step.to == currentSaveVersion;
    out = writeSaveContainer(container);
    return true;
}

MigrationResult SaveMigrator::migrate(const uint8_t* data, std::size_t size,
                                      SaveVersion target) const {
    MigrationResult result;
    result.to = target;

    SaveVersion from = currentSaveVersion;
    if (!peekVersion(data, size, from)) {
        result.failure = "save header is unreadable: bad magic, short buffer or unknown version";
        return result;
    }
    result.from = from;

    MigrationPlan plan = planFor(from, target);
    if (!plan.possible) {
        result.failure = plan.refusal;
        return result;
    }

    SaveContainer source;
    if (!readSaveContainer(data, size, source)) {
        result.failure = "save body is unreadable at " + std::string(saveVersionName(from));
        return result;
    }
    result.chunksMigrated = source.chunks.size();

    ByteBuffer current(data, data + size);
    for (const MigrationStep& step : plan.steps) {
        ByteBuffer next;
        if (!applyStep(current, step, result, next)) return result;
        current = std::move(next);
        result.applied.push_back(step);
    }

    result.output = std::move(current);
    result.ok = true;
    return result;
}

MigrationResult SaveMigrator::migrateToCurrent(const ByteBuffer& buffer) const {
    return migrate(buffer.data(), buffer.size(), currentSaveVersion);
}

} // namespace vf
