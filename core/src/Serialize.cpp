#include "vf/Serialize.h"

namespace vf {

namespace {

constexpr uint32_t magic = 0x31465856;  // "VXF1" little-endian
constexpr uint32_t formatVersion = 1;

void putU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

void putI32(std::vector<uint8_t>& out, int32_t value) {
    putU32(out, static_cast<uint32_t>(value));
}

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

} // namespace

std::vector<uint8_t> serializeWorld(const World& world) {
    std::vector<uint8_t> out;
    putU32(out, magic);
    putU32(out, formatVersion);
    putU32(out, static_cast<uint32_t>(world.chunkCount()));

    for (const auto& entry : world.allChunks()) {
        const Vec3i& coord = entry.first;
        const Chunk& chunk = *entry.second;
        putI32(out, coord.x);
        putI32(out, coord.y);
        putI32(out, coord.z);

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
    return out;
}

bool deserializeWorld(const uint8_t* data, std::size_t size, World& out) {
    Reader reader{data, size};
    uint32_t fileMagic = 0;
    uint32_t version = 0;
    uint32_t chunkTotal = 0;
    if (!reader.u32(fileMagic) || fileMagic != magic) return false;
    if (!reader.u32(version) || version != formatVersion) return false;
    if (!reader.u32(chunkTotal)) return false;

    for (uint32_t c = 0; c < chunkTotal; ++c) {
        int32_t cx, cy, cz;
        if (!reader.i32(cx) || !reader.i32(cy) || !reader.i32(cz)) return false;
        Chunk& chunk = out.ensureChunk({cx, cy, cz});
        auto& blocks = chunk.raw();

        std::size_t written = 0;
        while (written < blocks.size()) {
            uint8_t value;
            uint16_t run;
            if (!reader.u8(value) || !reader.u16(run)) return false;
            if (run == 0 || written + run > blocks.size()) return false;
            for (uint16_t r = 0; r < run; ++r) blocks[written++] = static_cast<Block>(value);
        }
    }
    return true;
}

} // namespace vf
