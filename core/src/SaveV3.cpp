#include "vf/SaveV3.h"

#include <cstring>

#include "vf/Serialize.h"

namespace vf {

namespace {

constexpr uint32_t magicV1 = 0x31465856;
constexpr uint32_t magicV3 = 0x33465856;
constexpr uint32_t formatVersion3 = 3;

constexpr uint32_t tagMobs = 0x4d4f4253;
constexpr uint32_t tagFluid = 0x464c5544;
constexpr uint32_t tagLight = 0x4c494754;

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

void putF32(std::vector<uint8_t>& out, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    putU32(out, bits);
}

void putBytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& bytes) {
    putU32(out, static_cast<uint32_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
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
    bool f32(float& out) {
        uint32_t bits;
        if (!u32(bits)) return false;
        std::memcpy(&out, &bits, 4);
        return true;
    }
    bool bytes(std::vector<uint8_t>& out) {
        uint32_t len;
        if (!u32(len)) return false;
        if (pos + len > size) return false;
        out.assign(data + pos, data + pos + len);
        pos += len;
        return true;
    }
};

void writeBlocks(std::vector<uint8_t>& out, const World& world) {
    putU32(out, static_cast<uint32_t>(world.chunkCount()));
    for (const auto& entry : world.allChunks()) {
        const Vec3i& coord = entry.first;
        putI32(out, coord.x);
        putI32(out, coord.y);
        putI32(out, coord.z);
        const auto& blocks = entry.second->raw();
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
}

bool readBlocks(Reader& reader, World& world) {
    uint32_t chunkTotal = 0;
    if (!reader.u32(chunkTotal)) return false;
    for (uint32_t c = 0; c < chunkTotal; ++c) {
        int32_t cx, cy, cz;
        if (!reader.i32(cx) || !reader.i32(cy) || !reader.i32(cz)) return false;
        Chunk& chunk = world.ensureChunk({cx, cy, cz});
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

void writeGrid(std::vector<uint8_t>& out, const GridSnapshot& grid) {
    putI32(out, grid.origin.x);
    putI32(out, grid.origin.y);
    putI32(out, grid.origin.z);
    putI32(out, grid.dims.x);
    putI32(out, grid.dims.y);
    putI32(out, grid.dims.z);
    putBytes(out, grid.channelA);
    putBytes(out, grid.channelB);
}

bool readGrid(Reader& reader, GridSnapshot& grid) {
    if (!reader.i32(grid.origin.x) || !reader.i32(grid.origin.y) || !reader.i32(grid.origin.z))
        return false;
    if (!reader.i32(grid.dims.x) || !reader.i32(grid.dims.y) || !reader.i32(grid.dims.z))
        return false;
    return reader.bytes(grid.channelA) && reader.bytes(grid.channelB);
}

void writeMob(std::vector<uint8_t>& out, const Mob& mob) {
    putF32(out, mob.position.x);
    putF32(out, mob.position.y);
    putF32(out, mob.position.z);
    putF32(out, mob.velocity.x);
    putF32(out, mob.velocity.y);
    putF32(out, mob.velocity.z);
    putF32(out, mob.width);
    putF32(out, mob.height);
    out.push_back(mob.onGround ? 1u : 0u);
    out.push_back(static_cast<uint8_t>(mob.state));
    putU32(out, mob.rng);
    putF32(out, mob.wanderDir.x);
    putF32(out, mob.wanderDir.y);
    putF32(out, mob.wanderDir.z);
}

bool readMob(Reader& reader, Mob& mob) {
    uint8_t onGround = 0, state = 0;
    if (!reader.f32(mob.position.x) || !reader.f32(mob.position.y) || !reader.f32(mob.position.z))
        return false;
    if (!reader.f32(mob.velocity.x) || !reader.f32(mob.velocity.y) || !reader.f32(mob.velocity.z))
        return false;
    if (!reader.f32(mob.width) || !reader.f32(mob.height)) return false;
    if (!reader.u8(onGround) || !reader.u8(state)) return false;
    if (!reader.u32(mob.rng)) return false;
    if (!reader.f32(mob.wanderDir.x) || !reader.f32(mob.wanderDir.y) || !reader.f32(mob.wanderDir.z))
        return false;
    mob.onGround = onGround != 0;
    mob.state = static_cast<MobState>(state);
    return true;
}

}

std::vector<uint8_t> serializeSaveV3(const SaveV3& save) {
    std::vector<uint8_t> out;
    putU32(out, magicV3);
    putU32(out, formatVersion3);
    writeBlocks(out, save.world);

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> sections;

    std::vector<uint8_t> mobPayload;
    putU32(mobPayload, static_cast<uint32_t>(save.mobs.size()));
    for (const Mob& mob : save.mobs) writeMob(mobPayload, mob);
    sections.push_back({tagMobs, std::move(mobPayload)});

    if (save.hasFluid) {
        std::vector<uint8_t> payload;
        writeGrid(payload, save.fluid);
        sections.push_back({tagFluid, std::move(payload)});
    }
    if (save.hasLight) {
        std::vector<uint8_t> payload;
        writeGrid(payload, save.light);
        sections.push_back({tagLight, std::move(payload)});
    }

    putU32(out, static_cast<uint32_t>(sections.size()));
    for (const auto& section : sections) {
        putU32(out, section.first);
        putBytes(out, section.second);
    }
    return out;
}

bool deserializeSaveV3(const uint8_t* data, std::size_t size, SaveV3& out) {
    Reader peek{data, size};
    uint32_t magic = 0;
    if (!peek.u32(magic)) return false;

    if (magic == magicV1) {
        return deserializeWorld(data, size, out.world);
    }
    if (magic != magicV3) return false;

    Reader reader{data, size};
    uint32_t fileMagic = 0, version = 0;
    reader.u32(fileMagic);
    if (!reader.u32(version) || version != formatVersion3) return false;
    if (!readBlocks(reader, out.world)) return false;

    uint32_t sectionCount = 0;
    if (!reader.u32(sectionCount)) return false;
    for (uint32_t s = 0; s < sectionCount; ++s) {
        uint32_t tag = 0, length = 0;
        if (!reader.u32(tag) || !reader.u32(length)) return false;
        std::size_t sectionStart = reader.pos;
        if (sectionStart + length > size) return false;

        if (tag == tagMobs) {
            uint32_t count = 0;
            if (!reader.u32(count)) return false;
            out.mobs.resize(count);
            for (uint32_t m = 0; m < count; ++m)
                if (!readMob(reader, out.mobs[m])) return false;
        } else if (tag == tagFluid) {
            if (!readGrid(reader, out.fluid)) return false;
            out.hasFluid = true;
        } else if (tag == tagLight) {
            if (!readGrid(reader, out.light)) return false;
            out.hasLight = true;
        }
        reader.pos = sectionStart + length;
    }
    return true;
}

}
