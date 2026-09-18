#include "vf/Anvil.h"

#include <algorithm>

namespace vf {

int BitPacker::bitWidthForPalette(std::size_t paletteSize) {
    if (paletteSize <= 1) return 1;
    int bits = 1;
    while ((static_cast<std::size_t>(1) << bits) < paletteSize) ++bits;
    return bits;
}

namespace {

uint64_t widthMask(int bitWidth) {
    return bitWidth >= 64 ? ~0ull : ((1ull << bitWidth) - 1ull);
}

}

std::vector<uint64_t> BitPacker::pack(const std::vector<uint32_t>& values, int bitWidth) {
    int perWord = 64 / bitWidth;
    std::size_t wordCount = (values.size() + static_cast<std::size_t>(perWord) - 1) /
                            static_cast<std::size_t>(perWord);
    std::vector<uint64_t> words(wordCount, 0);
    uint64_t mask = widthMask(bitWidth);
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t word = i / static_cast<std::size_t>(perWord);
        int slot = static_cast<int>(i % static_cast<std::size_t>(perWord));
        words[word] |= (static_cast<uint64_t>(values[i]) & mask) << (slot * bitWidth);
    }
    return words;
}

std::vector<uint32_t> BitPacker::unpack(const std::vector<uint64_t>& words, int bitWidth,
                                        std::size_t count) {
    int perWord = 64 / bitWidth;
    uint64_t mask = widthMask(bitWidth);
    std::vector<uint32_t> values(count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        std::size_t word = i / static_cast<std::size_t>(perWord);
        int slot = static_cast<int>(i % static_cast<std::size_t>(perWord));
        if (word >= words.size()) break;
        values[i] = static_cast<uint32_t>((words[word] >> (slot * bitWidth)) & mask);
    }
    return values;
}

void AnvilSection::encodeFrom(const Chunk& chunk) {
    palette.clear();
    std::array<int, static_cast<std::size_t>(Block::Count)> indexOf;
    indexOf.fill(-1);

    std::vector<uint32_t> indices;
    indices.reserve(chunkVolume);
    for (Block block : chunk.raw()) {
        int& slot = indexOf[static_cast<std::size_t>(block)];
        if (slot < 0) {
            slot = static_cast<int>(palette.size());
            palette.push_back(block);
        }
        indices.push_back(static_cast<uint32_t>(slot));
    }

    bitWidth = BitPacker::bitWidthForPalette(palette.size());
    packed = BitPacker::pack(indices, bitWidth);
}

void AnvilSection::decodeTo(Chunk& chunk) const {
    std::vector<uint32_t> indices = BitPacker::unpack(packed, bitWidth, chunkVolume);
    std::array<Block, chunkVolume>& blocks = chunk.raw();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        uint32_t index = indices[i];
        blocks[i] = index < palette.size() ? palette[index] : Block::Air;
    }
}

const AnvilSection* AnvilColumn::section(int sectionY) const {
    for (const AnvilSection& s : sections)
        if (s.sectionY == sectionY) return &s;
    return nullptr;
}

AnvilColumn& AnvilRegion::columnFor(int localX, int localZ) {
    for (AnvilColumn& column : columns)
        if (column.localX == localX && column.localZ == localZ) return column;
    columns.push_back(AnvilColumn{});
    AnvilColumn& created = columns.back();
    created.localX = localX;
    created.localZ = localZ;
    return created;
}

const AnvilColumn* AnvilRegion::findColumn(int localX, int localZ) const {
    for (const AnvilColumn& column : columns)
        if (column.localX == localX && column.localZ == localZ) return &column;
    return nullptr;
}

void AnvilRegion::putChunk(const Vec3i& chunkCoord, const Chunk& chunk) {
    int localX = floorMod(chunkCoord.x, anvilRegionSpan);
    int localZ = floorMod(chunkCoord.z, anvilRegionSpan);
    AnvilColumn& column = columnFor(localX, localZ);
    AnvilSection encoded;
    encoded.sectionY = chunkCoord.y;
    encoded.encodeFrom(chunk);
    for (AnvilSection& existing : column.sections) {
        if (existing.sectionY == chunkCoord.y) {
            existing = std::move(encoded);
            return;
        }
    }
    column.sections.push_back(std::move(encoded));
    std::sort(column.sections.begin(), column.sections.end(),
              [](const AnvilSection& a, const AnvilSection& b) { return a.sectionY < b.sectionY; });
}

bool AnvilRegion::hasChunk(const Vec3i& chunkCoord) const {
    int localX = floorMod(chunkCoord.x, anvilRegionSpan);
    int localZ = floorMod(chunkCoord.z, anvilRegionSpan);
    const AnvilColumn* column = findColumn(localX, localZ);
    return column && column->section(chunkCoord.y) != nullptr;
}

bool AnvilRegion::getChunk(const Vec3i& chunkCoord, Chunk& out) const {
    int localX = floorMod(chunkCoord.x, anvilRegionSpan);
    int localZ = floorMod(chunkCoord.z, anvilRegionSpan);
    const AnvilColumn* column = findColumn(localX, localZ);
    if (!column) return false;
    const AnvilSection* section = column->section(chunkCoord.y);
    if (!section) return false;
    section->decodeTo(out);
    return true;
}

std::size_t AnvilRegion::sectionCount() const {
    std::size_t total = 0;
    for (const AnvilColumn& column : columns) total += column.sections.size();
    return total;
}

int32_t AnvilRegion::heightAt(int localX, int localZ, int x, int z) const {
    const AnvilColumn* column = findColumn(localX, localZ);
    if (!column || column->heightmap.empty()) return heightNone;
    return column->heightmap[static_cast<std::size_t>(z * chunkSize + x)];
}

namespace {

std::vector<int32_t> computeHeightmap(const AnvilColumn& column) {
    std::vector<int32_t> heights(static_cast<std::size_t>(chunkSize * chunkSize), heightNone);
    for (const AnvilSection& section : column.sections) {
        Chunk decoded;
        section.decodeTo(decoded);
        for (int x = 0; x < chunkSize; ++x)
            for (int z = 0; z < chunkSize; ++z)
                for (int y = 0; y < chunkSize; ++y) {
                    if (decoded.at(x, y, z) == Block::Air) continue;
                    int32_t worldY = section.sectionY * chunkSize + y;
                    int32_t& slot = heights[static_cast<std::size_t>(z * chunkSize + x)];
                    if (slot == heightNone || worldY > slot) slot = worldY;
                }
    }
    return heights;
}

void writeColumnBody(ByteWriter& body, const AnvilColumn& column) {
    std::vector<int32_t> heights = computeHeightmap(column);
    for (int32_t h : heights) body.putVarI32(h);
    body.putVarU32(static_cast<uint32_t>(column.sections.size()));
    for (const AnvilSection& section : column.sections) {
        body.putVarI32(section.sectionY);
        body.putVarU32(static_cast<uint32_t>(section.palette.size()));
        for (Block block : section.palette) body.putU8(static_cast<uint8_t>(block));
        body.putU8(static_cast<uint8_t>(section.bitWidth));
        body.putVarU32(static_cast<uint32_t>(section.packed.size()));
        for (uint64_t word : section.packed) body.putU64(word);
    }
}

bool readColumnBody(ByteReader& reader, AnvilColumn& column) {
    column.heightmap.assign(static_cast<std::size_t>(chunkSize * chunkSize), heightNone);
    for (std::size_t i = 0; i < column.heightmap.size(); ++i) {
        int32_t h;
        if (!reader.getVarI32(h)) return false;
        column.heightmap[i] = h;
    }
    uint32_t sectionCount;
    if (!reader.getVarU32(sectionCount)) return false;
    column.sections.clear();
    column.sections.reserve(sectionCount);
    for (uint32_t s = 0; s < sectionCount; ++s) {
        AnvilSection section;
        int32_t sectionY;
        if (!reader.getVarI32(sectionY)) return false;
        section.sectionY = sectionY;
        uint32_t paletteSize;
        if (!reader.getVarU32(paletteSize)) return false;
        section.palette.reserve(paletteSize);
        for (uint32_t p = 0; p < paletteSize; ++p) {
            uint8_t block;
            if (!reader.getU8(block)) return false;
            section.palette.push_back(static_cast<Block>(block));
        }
        uint8_t bitWidth;
        if (!reader.getU8(bitWidth)) return false;
        if (bitWidth < 1 || bitWidth > 64) return false;
        section.bitWidth = bitWidth;
        uint32_t wordCount;
        if (!reader.getVarU32(wordCount)) return false;
        section.packed.reserve(wordCount);
        for (uint32_t w = 0; w < wordCount; ++w) {
            uint64_t word;
            if (!reader.getU64(word)) return false;
            section.packed.push_back(word);
        }
        column.sections.push_back(std::move(section));
    }
    return true;
}

const uint8_t anvilMagic[4] = {'V', 'X', 'A', '5'};

}

ByteBuffer AnvilRegion::serialize() const {
    ByteWriter out;
    for (uint8_t byte : anvilMagic) out.putU8(byte);
    out.putU8(5);
    out.putU32(static_cast<uint32_t>(regionX));
    out.putU32(static_cast<uint32_t>(regionZ));
    out.putVarU32(static_cast<uint32_t>(columns.size()));
    for (const AnvilColumn& column : columns) {
        out.putU8(static_cast<uint8_t>(column.localX));
        out.putU8(static_cast<uint8_t>(column.localZ));
        ByteWriter body;
        writeColumnBody(body, column);
        ByteBuffer compressed = compress(body.bytes());
        out.putVarU32(static_cast<uint32_t>(compressed.size()));
        out.putBytes(compressed.data(), compressed.size());
    }
    return out.bytes();
}

bool AnvilRegion::deserialize(const uint8_t* data, std::size_t size, AnvilRegion& out) {
    ByteReader reader(data, size);
    for (uint8_t expected : anvilMagic) {
        uint8_t got;
        if (!reader.getU8(got) || got != expected) return false;
    }
    uint8_t version;
    if (!reader.getU8(version) || version != 5) return false;
    uint32_t rx, rz;
    if (!reader.getU32(rx) || !reader.getU32(rz)) return false;
    out.regionX = static_cast<int32_t>(rx);
    out.regionZ = static_cast<int32_t>(rz);

    uint32_t columnCount;
    if (!reader.getVarU32(columnCount)) return false;
    out.columns.clear();
    out.columns.reserve(columnCount);
    for (uint32_t c = 0; c < columnCount; ++c) {
        uint8_t localX, localZ;
        if (!reader.getU8(localX) || !reader.getU8(localZ)) return false;
        if (localX >= anvilRegionSpan || localZ >= anvilRegionSpan) return false;
        uint32_t compressedLen;
        if (!reader.getVarU32(compressedLen)) return false;
        if (reader.remaining() < compressedLen) return false;
        ByteBuffer body;
        if (!decompress(data + reader.position(), compressedLen, body)) return false;
        std::vector<uint8_t> skip(compressedLen);
        if (!reader.getBytes(skip.data(), compressedLen)) return false;

        AnvilColumn column;
        column.localX = localX;
        column.localZ = localZ;
        ByteReader bodyReader(body.data(), body.size());
        if (!readColumnBody(bodyReader, column)) return false;
        out.columns.push_back(std::move(column));
    }
    return true;
}

std::vector<AnvilRegion> anvilRegionsFromWorld(const World& world) {
    std::vector<AnvilRegion> regions;
    for (const auto& entry : world.allChunks()) {
        const Vec3i& coord = entry.first;
        int regionX = floorDiv(coord.x, anvilRegionSpan);
        int regionZ = floorDiv(coord.z, anvilRegionSpan);
        AnvilRegion* target = nullptr;
        for (AnvilRegion& region : regions) {
            if (region.rx() == regionX && region.rz() == regionZ) {
                target = &region;
                break;
            }
        }
        if (!target) {
            regions.emplace_back(regionX, regionZ);
            target = &regions.back();
        }
        target->putChunk(coord, *entry.second);
    }
    return regions;
}

ByteBuffer anvilSerializeWorld(const World& world) {
    std::vector<AnvilRegion> regions = anvilRegionsFromWorld(world);
    ByteWriter out;
    out.putVarU32(static_cast<uint32_t>(regions.size()));
    for (const AnvilRegion& region : regions) {
        ByteBuffer blob = region.serialize();
        out.putVarU32(static_cast<uint32_t>(blob.size()));
        out.putBytes(blob.data(), blob.size());
    }
    return out.bytes();
}

bool anvilDeserializeWorld(const uint8_t* data, std::size_t size, World& out) {
    ByteReader reader(data, size);
    uint32_t regionCount;
    if (!reader.getVarU32(regionCount)) return false;
    for (uint32_t r = 0; r < regionCount; ++r) {
        uint32_t blobLen;
        if (!reader.getVarU32(blobLen)) return false;
        if (reader.remaining() < blobLen) return false;
        AnvilRegion region;
        if (!AnvilRegion::deserialize(data + reader.position(), blobLen, region)) return false;
        std::vector<uint8_t> skip(blobLen);
        if (!reader.getBytes(skip.data(), blobLen)) return false;

        for (const AnvilColumn& column : region.allColumns()) {
            int chunkX = region.rx() * anvilRegionSpan + column.localX;
            int chunkZ = region.rz() * anvilRegionSpan + column.localZ;
            for (const AnvilSection& section : column.sections) {
                Chunk chunk;
                section.decodeTo(chunk);
                out.ensureChunk(Vec3i{chunkX, section.sectionY, chunkZ}) = chunk;
            }
        }
    }
    return true;
}

}
