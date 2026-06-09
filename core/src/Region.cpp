#include "vf/Region.h"

namespace vf {

namespace {

constexpr uint32_t regionMagic = 0x31525856;   // "VXR1"
constexpr uint32_t regionVersion = 1;
constexpr uint32_t regionsMagic = 0x53525856;   // "VXRS"

void putU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void putI32(std::vector<uint8_t>& out, int32_t value) { putU32(out, static_cast<uint32_t>(value)); }

void putU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
}

struct Reader {
    const uint8_t* data;
    std::size_t size;
    std::size_t pos = 0;

    bool u32(uint32_t& out) {
        if (pos + 4 > size) return false;
        out = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8) |
              (static_cast<uint32_t>(data[pos + 2]) << 16) |
              (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return true;
    }
    bool i32(int32_t& out) {
        uint32_t v;
        if (!u32(v)) return false;
        out = static_cast<int32_t>(v);
        return true;
    }
    bool u16(uint16_t& out) {
        if (pos + 2 > size) return false;
        out = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        return true;
    }
    bool u8(uint8_t& out) {
        if (pos + 1 > size) return false;
        out = data[pos++];
        return true;
    }
};

void encodeChunk(std::vector<uint8_t>& out, const Chunk& chunk) {
    const auto& blocks = chunk.raw();
    std::size_t i = 0;
    while (i < blocks.size()) {
        Block value = blocks[i];
        uint16_t run = 1;
        while (i + run < blocks.size() && blocks[i + run] == value && run < 0xffff) ++run;
        out.push_back(static_cast<uint8_t>(value));
        putU16(out, run);
        i += run;
    }
}

bool decodeChunk(Reader& reader, Chunk& chunk) {
    auto& blocks = chunk.raw();
    std::size_t written = 0;
    while (written < blocks.size()) {
        uint8_t value;
        uint16_t run;
        if (!reader.u8(value) || !reader.u16(run)) return false;
        if (run == 0 || written + run > blocks.size()) return false;
        for (uint16_t r = 0; r < run; ++r) blocks[written++] = static_cast<Block>(value);
    }
    return true;
}

} // namespace

RegionCoord regionForChunk(const Vec3i& chunkCoord) {
    return {floorDiv(chunkCoord.x, regionSize), floorDiv(chunkCoord.z, regionSize)};
}

void Region::putChunk(const Vec3i& chunkCoord, const Chunk& chunk) {
    chunks[chunkCoord] = chunk;
}

bool Region::getChunk(const Vec3i& chunkCoord, Chunk& out) const {
    auto it = chunks.find(chunkCoord);
    if (it == chunks.end()) return false;
    out = it->second;
    return true;
}

std::vector<uint8_t> Region::serialize() const {
    std::vector<uint8_t> out;
    putU32(out, regionMagic);
    putU32(out, regionVersion);
    putI32(out, coord.x);
    putI32(out, coord.z);
    putU32(out, static_cast<uint32_t>(chunks.size()));

    for (const auto& entry : chunks) {
        putI32(out, entry.first.x);
        putI32(out, entry.first.y);
        putI32(out, entry.first.z);
        std::vector<uint8_t> payload;
        encodeChunk(payload, entry.second);
        putU32(out, static_cast<uint32_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

bool Region::deserialize(const uint8_t* data, std::size_t size, Region& out) {
    Reader reader{data, size};
    uint32_t magic = 0, version = 0, count = 0;
    int32_t rx = 0, rz = 0;
    if (!reader.u32(magic) || magic != regionMagic) return false;
    if (!reader.u32(version) || version != regionVersion) return false;
    if (!reader.i32(rx) || !reader.i32(rz)) return false;
    if (!reader.u32(count)) return false;

    out.coord = {rx, rz};
    out.chunks.clear();
    for (uint32_t c = 0; c < count; ++c) {
        int32_t cx, cy, cz;
        uint32_t length;
        if (!reader.i32(cx) || !reader.i32(cy) || !reader.i32(cz)) return false;
        if (!reader.u32(length)) return false;
        if (reader.pos + length > size) return false;
        Reader chunkReader{data + reader.pos, length};
        Chunk chunk;
        if (!decodeChunk(chunkReader, chunk)) return false;
        out.chunks[{cx, cy, cz}] = chunk;
        reader.pos += length;
    }
    return true;
}

std::unordered_map<RegionCoord, Region, RegionCoordHash> splitWorldIntoRegions(const World& world) {
    std::unordered_map<RegionCoord, Region, RegionCoordHash> regions;
    for (const auto& entry : world.allChunks()) {
        if (entry.second->empty()) continue;
        RegionCoord rc = regionForChunk(entry.first);
        auto it = regions.find(rc);
        if (it == regions.end()) it = regions.emplace(rc, Region{rc}).first;
        it->second.putChunk(entry.first, *entry.second);
    }
    return regions;
}

std::vector<uint8_t> serializeRegions(const World& world) {
    auto regions = splitWorldIntoRegions(world);
    std::vector<uint8_t> out;
    putU32(out, regionsMagic);
    putU32(out, static_cast<uint32_t>(regions.size()));
    for (const auto& entry : regions) {
        std::vector<uint8_t> payload = entry.second.serialize();
        putU32(out, static_cast<uint32_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

bool deserializeRegions(const uint8_t* data, std::size_t size, World& out) {
    Reader reader{data, size};
    uint32_t magic = 0, count = 0;
    if (!reader.u32(magic) || magic != regionsMagic) return false;
    if (!reader.u32(count)) return false;

    for (uint32_t r = 0; r < count; ++r) {
        uint32_t length;
        if (!reader.u32(length)) return false;
        if (reader.pos + length > size) return false;
        Region region;
        if (!Region::deserialize(data + reader.pos, length, region)) return false;
        reader.pos += length;
        for (const auto& entry : region.allChunks())
            out.ensureChunk(entry.first) = entry.second;
    }
    return true;
}

} // namespace vf
