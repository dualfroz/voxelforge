#include "vf/Schematic.h"

#include <algorithm>

namespace vf {

namespace {

constexpr uint32_t schematicMagic = 0x56585343u;
constexpr std::size_t maxSchematicCells = 1u << 26;
constexpr uint8_t voidMaskFlag = 0x01;

std::size_t packedByteCount(std::size_t cells, uint8_t bits) {
    return (cells * static_cast<std::size_t>(bits) + 7u) / 8u;
}

void packIndices(const std::vector<uint16_t>& indices, uint8_t bits, ByteBuffer& out) {
    std::size_t base = out.size();
    out.resize(base + packedByteCount(indices.size(), bits));
    uint32_t mask = (1u << bits) - 1u;
    for (std::size_t i = 0; i < indices.size(); ++i) {
        std::size_t bitPosition = i * bits;
        std::size_t byteIndex = base + (bitPosition >> 3);
        unsigned shift = static_cast<unsigned>(bitPosition & 7u);
        out[byteIndex] |= static_cast<uint8_t>((indices[i] & mask) << shift);
    }
}

void unpackIndices(const uint8_t* data, std::size_t cells, uint8_t bits,
                   std::vector<uint16_t>& out) {
    out.assign(cells, 0u);
    uint32_t mask = (1u << bits) - 1u;
    for (std::size_t i = 0; i < cells; ++i) {
        std::size_t bitPosition = i * bits;
        unsigned shift = static_cast<unsigned>(bitPosition & 7u);
        out[i] = static_cast<uint16_t>((data[bitPosition >> 3] >> shift) & mask);
    }
}

void buildPalette(const ClipboardBuffer& buffer, std::vector<Block>& palette,
                  std::vector<uint16_t>& slotOfBlock) {
    palette.clear();
    slotOfBlock.assign(static_cast<std::size_t>(Block::Count), 0u);
    std::vector<uint8_t> used(static_cast<std::size_t>(Block::Count), 0u);
    used[static_cast<std::size_t>(Block::Air)] = 1u;

    const Vec3i& dims = buffer.dims();
    for (int y = 0; y < dims.y; ++y)
        for (int z = 0; z < dims.z; ++z)
            for (int x = 0; x < dims.x; ++x) {
                if (!buffer.filledAt(x, y, z)) continue;
                used[static_cast<std::size_t>(buffer.at(x, y, z))] = 1u;
            }

    palette.push_back(Block::Air);
    for (std::size_t id = 0; id < used.size(); ++id) {
        if (used[id] == 0u || static_cast<Block>(id) == Block::Air) continue;
        slotOfBlock[id] = static_cast<uint16_t>(palette.size());
        palette.push_back(static_cast<Block>(id));
    }
}

void putI32(ByteWriter& writer, int value) { writer.putU32(static_cast<uint32_t>(value)); }

bool getI32(ByteReader& reader, int& out) {
    uint32_t raw = 0;
    if (!reader.getU32(raw)) return false;
    out = static_cast<int>(static_cast<int32_t>(raw));
    return true;
}

}

ClipboardBuffer::ClipboardBuffer(const Vec3i& dims, const Vec3i& anchor) {
    if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) return;
    size = dims;
    handle = anchor;
    blocks.assign(volume(), Block::Air);
    present = SelectionMask(Vec3i{0, 0, 0}, dims);
}

std::size_t ClipboardBuffer::volume() const {
    return static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
           static_cast<std::size_t>(size.z);
}

bool ClipboardBuffer::inBounds(int x, int y, int z) const {
    return x >= 0 && x < size.x && y >= 0 && y < size.y && z >= 0 && z < size.z;
}

std::size_t ClipboardBuffer::indexOf(int x, int y, int z) const {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(size.z) +
            static_cast<std::size_t>(z)) *
               static_cast<std::size_t>(size.x) +
           static_cast<std::size_t>(x);
}

Block ClipboardBuffer::at(int x, int y, int z) const {
    if (!inBounds(x, y, z)) return Block::Air;
    return blocks[indexOf(x, y, z)];
}

bool ClipboardBuffer::filledAt(int x, int y, int z) const {
    if (!inBounds(x, y, z)) return false;
    return present.contains(Vec3i{x, y, z});
}

void ClipboardBuffer::put(int x, int y, int z, Block block, bool filled) {
    if (!inBounds(x, y, z)) return;
    blocks[indexOf(x, y, z)] = block;
    present.setCell(Vec3i{x, y, z}, filled);
}

void ClipboardBuffer::clearCell(int x, int y, int z) {
    if (!inBounds(x, y, z)) return;
    blocks[indexOf(x, y, z)] = Block::Air;
    present.remove(Vec3i{x, y, z});
}

ClipboardBuffer ClipboardBuffer::copyFromWorld(const World& world, const Selection& selection) {
    if (selection.empty()) return ClipboardBuffer{};
    Vec3i origin = selection.min();
    ClipboardBuffer copy(selection.dims(), Vec3i{0, 0, 0});
    selection.forEach([&](const Vec3i& cell) {
        copy.put(cell.x - origin.x, cell.y - origin.y, cell.z - origin.z,
                 world.blockAt(cell.x, cell.y, cell.z), true);
    });
    return copy;
}

ClipboardBuffer ClipboardBuffer::rotatedY(Rotation rotation) const {
    if (rotation == Rotation::None || empty()) return *this;

    bool swapsAxes = rotation == Rotation::Cw90 || rotation == Rotation::Cw270;
    Vec3i newDims = swapsAxes ? Vec3i{size.z, size.y, size.x} : size;

    auto mapCell = [&](int x, int y, int z, int& outX, int& outY, int& outZ) {
        outY = y;
        switch (rotation) {
            case Rotation::Cw90:
                outX = size.z - 1 - z;
                outZ = x;
                break;
            case Rotation::Cw180:
                outX = size.x - 1 - x;
                outZ = size.z - 1 - z;
                break;
            case Rotation::Cw270:
                outX = z;
                outZ = size.x - 1 - x;
                break;
            case Rotation::None:
                outX = x;
                outZ = z;
                break;
        }
    };

    int anchorX = 0, anchorY = 0, anchorZ = 0;
    mapCell(handle.x, handle.y, handle.z, anchorX, anchorY, anchorZ);
    ClipboardBuffer turned(newDims, Vec3i{anchorX, anchorY, anchorZ});

    for (int y = 0; y < size.y; ++y)
        for (int z = 0; z < size.z; ++z)
            for (int x = 0; x < size.x; ++x) {
                int tx = 0, ty = 0, tz = 0;
                mapCell(x, y, z, tx, ty, tz);
                turned.put(tx, ty, tz, at(x, y, z), filledAt(x, y, z));
            }
    return turned;
}

ClipboardBuffer ClipboardBuffer::mirrored(MirrorAxis axis) const {
    if (empty()) return *this;
    auto mapCell = [&](int x, int y, int z, int& outX, int& outY, int& outZ) {
        outX = axis == MirrorAxis::X ? size.x - 1 - x : x;
        outY = axis == MirrorAxis::Y ? size.y - 1 - y : y;
        outZ = axis == MirrorAxis::Z ? size.z - 1 - z : z;
    };

    int anchorX = 0, anchorY = 0, anchorZ = 0;
    mapCell(handle.x, handle.y, handle.z, anchorX, anchorY, anchorZ);
    ClipboardBuffer flipped(size, Vec3i{anchorX, anchorY, anchorZ});

    for (int y = 0; y < size.y; ++y)
        for (int z = 0; z < size.z; ++z)
            for (int x = 0; x < size.x; ++x) {
                int tx = 0, ty = 0, tz = 0;
                mapCell(x, y, z, tx, ty, tz);
                flipped.put(tx, ty, tz, at(x, y, z), filledAt(x, y, z));
            }
    return flipped;
}

ClipboardBuffer ClipboardBuffer::trimmed() const {
    Vec3i lo{size.x, size.y, size.z};
    Vec3i hi{-1, -1, -1};
    for (int y = 0; y < size.y; ++y)
        for (int z = 0; z < size.z; ++z)
            for (int x = 0; x < size.x; ++x) {
                if (!filledAt(x, y, z) || isAir(at(x, y, z))) continue;
                lo = {std::min(lo.x, x), std::min(lo.y, y), std::min(lo.z, z)};
                hi = {std::max(hi.x, x), std::max(hi.y, y), std::max(hi.z, z)};
            }
    if (hi.x < lo.x) return ClipboardBuffer{};

    ClipboardBuffer tight({hi.x - lo.x + 1, hi.y - lo.y + 1, hi.z - lo.z + 1},
                          {handle.x - lo.x, handle.y - lo.y, handle.z - lo.z});
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x)
                tight.put(x - lo.x, y - lo.y, z - lo.z, at(x, y, z), filledAt(x, y, z));
    return tight;
}

bool ClipboardBuffer::sameContent(const ClipboardBuffer& other) const {
    if (!(size == other.size)) return false;
    for (int y = 0; y < size.y; ++y)
        for (int z = 0; z < size.z; ++z)
            for (int x = 0; x < size.x; ++x) {
                if (filledAt(x, y, z) != other.filledAt(x, y, z)) return false;
                if (!filledAt(x, y, z)) continue;
                if (at(x, y, z) != other.at(x, y, z)) return false;
            }
    return true;
}

uint8_t Schematic::bitsForPalette(std::size_t paletteSize) {
    if (paletteSize <= 2u) return 1u;
    if (paletteSize <= 4u) return 2u;
    if (paletteSize <= 16u) return 4u;
    return 8u;
}

const char* Schematic::errorText(SchematicError error) {
    switch (error) {
        case SchematicError::None: return "ok";
        case SchematicError::BadMagic: return "not a schematic";
        case SchematicError::UnsupportedVersion: return "unsupported schematic version";
        case SchematicError::Truncated: return "truncated schematic";
        case SchematicError::BadChecksum: return "checksum mismatch";
        case SchematicError::DimensionMismatch: return "dimensions do not match payload";
        case SchematicError::BadPalette: return "invalid block palette";
    }
    return "unknown schematic error";
}

Schematic Schematic::capture(const World& world, const Selection& selection,
                             const std::string& name, const std::string& author, uint64_t tick) {
    return fromBuffer(ClipboardBuffer::copyFromWorld(world, selection), name, author, tick);
}

Schematic Schematic::fromBuffer(const ClipboardBuffer& buffer, const std::string& name,
                                const std::string& author, uint64_t tick) {
    Schematic out;
    out.cells = buffer;
    out.meta.version = SchematicVersion::V2;
    out.meta.name = name;
    out.meta.author = author;
    out.meta.tick = tick;
    out.refreshHeaderFromBuffer();
    return out;
}

void Schematic::refreshHeaderFromBuffer() {
    std::vector<Block> palette;
    std::vector<uint16_t> slotOfBlock;
    buildPalette(cells, palette, slotOfBlock);
    meta.dims = cells.dims();
    meta.anchor = cells.anchor();
    meta.paletteSize = static_cast<uint16_t>(palette.size());
    meta.indexBits = bitsForPalette(palette.size());
    meta.hasVoidMask = cells.filledCount() != cells.volume();
}

void Schematic::setAnchor(const Vec3i& anchor) {
    cells.setAnchor(anchor);
    meta.anchor = anchor;
}

void Schematic::rotate(Rotation rotation) {
    cells = cells.rotatedY(rotation);
    refreshHeaderFromBuffer();
}

void Schematic::mirror(MirrorAxis axis) {
    cells = cells.mirrored(axis);
    refreshHeaderFromBuffer();
}

void Schematic::trim() {
    cells = cells.trimmed();
    refreshHeaderFromBuffer();
}

ByteBuffer Schematic::encode() const { return encode(SchematicVersion::V2); }

ByteBuffer Schematic::encode(SchematicVersion version) const {
    std::vector<Block> palette;
    std::vector<uint16_t> slotOfBlock;
    buildPalette(cells, palette, slotOfBlock);
    uint8_t bits = bitsForPalette(palette.size());

    const Vec3i& dims = cells.dims();
    std::size_t cellCount = cells.volume();
    bool writeVoidMask =
        version == SchematicVersion::V2 && cells.filledCount() != cellCount;

    std::vector<uint16_t> indices;
    indices.reserve(cellCount);
    std::vector<uint16_t> presence;
    presence.reserve(writeVoidMask ? cellCount : 0u);
    for (int y = 0; y < dims.y; ++y)
        for (int z = 0; z < dims.z; ++z)
            for (int x = 0; x < dims.x; ++x) {
                bool filled = cells.filledAt(x, y, z);
                Block block = filled ? cells.at(x, y, z) : Block::Air;
                indices.push_back(slotOfBlock[static_cast<std::size_t>(block)]);
                if (writeVoidMask) presence.push_back(filled ? 1u : 0u);
            }

    ByteBuffer raw;
    raw.reserve(packedByteCount(cellCount, bits) + (writeVoidMask ? packedByteCount(cellCount, 1) : 0u));
    packIndices(indices, bits, raw);
    if (writeVoidMask) packIndices(presence, 1u, raw);
    ByteBuffer payload = compress(raw.data(), raw.size());

    ByteWriter body;
    putI32(body, dims.x);
    putI32(body, dims.y);
    putI32(body, dims.z);
    putI32(body, cells.anchor().x);
    putI32(body, cells.anchor().y);
    putI32(body, cells.anchor().z);
    body.putU16(static_cast<uint16_t>(palette.size()));
    for (Block block : palette) body.putU8(static_cast<uint8_t>(block));
    body.putU8(bits);
    if (version == SchematicVersion::V2) {
        body.putString(meta.name);
        body.putString(meta.author);
        body.putU64(meta.tick);
        body.putU8(writeVoidMask ? voidMaskFlag : 0u);
    }
    body.putU32(static_cast<uint32_t>(raw.size()));
    body.putU32(static_cast<uint32_t>(payload.size()));
    body.putBytes(payload.data(), payload.size());

    ByteWriter out;
    out.putU32(schematicMagic);
    out.putU32(static_cast<uint32_t>(version));
    out.putU32(static_cast<uint32_t>(body.size()));
    out.putBytes(body.bytes().data(), body.size());
    out.putU32(fnv1a32(body.bytes().data(), body.size()));
    return out.bytes();
}

SchematicError Schematic::decode(const uint8_t* data, std::size_t size, Schematic& out) {
    ByteReader head(data, size);
    uint32_t magic = 0, version = 0, bodyLength = 0;
    if (!head.getU32(magic)) return SchematicError::Truncated;
    if (magic != schematicMagic) return SchematicError::BadMagic;
    if (!head.getU32(version)) return SchematicError::Truncated;
    if (version != static_cast<uint32_t>(SchematicVersion::V1) &&
        version != static_cast<uint32_t>(SchematicVersion::V2))
        return SchematicError::UnsupportedVersion;
    if (!head.getU32(bodyLength)) return SchematicError::Truncated;

    std::size_t bodyStart = head.position();
    std::size_t bodySize = static_cast<std::size_t>(bodyLength);
    if (size < bodyStart + bodySize + 4u) return SchematicError::Truncated;

    ByteReader tail(data + bodyStart + bodySize, 4u);
    uint32_t storedChecksum = 0;
    tail.getU32(storedChecksum);
    if (fnv1a32(data + bodyStart, bodySize) != storedChecksum) return SchematicError::BadChecksum;

    ByteReader body(data + bodyStart, bodySize);
    Vec3i dims{0, 0, 0};
    Vec3i anchor{0, 0, 0};
    if (!getI32(body, dims.x) || !getI32(body, dims.y) || !getI32(body, dims.z))
        return SchematicError::Truncated;
    if (!getI32(body, anchor.x) || !getI32(body, anchor.y) || !getI32(body, anchor.z))
        return SchematicError::Truncated;
    bool wholeVolumeEmpty = dims.x == 0 && dims.y == 0 && dims.z == 0;
    if (!wholeVolumeEmpty && (dims.x <= 0 || dims.y <= 0 || dims.z <= 0))
        return SchematicError::DimensionMismatch;

    std::size_t cellCount = static_cast<std::size_t>(dims.x) * static_cast<std::size_t>(dims.y) *
                            static_cast<std::size_t>(dims.z);
    if (cellCount > maxSchematicCells) return SchematicError::DimensionMismatch;

    uint16_t paletteSize = 0;
    if (!body.getU16(paletteSize)) return SchematicError::Truncated;
    if (paletteSize == 0u) return SchematicError::BadPalette;
    std::vector<Block> palette;
    palette.reserve(paletteSize);
    for (uint16_t i = 0; i < paletteSize; ++i) {
        uint8_t id = 0;
        if (!body.getU8(id)) return SchematicError::Truncated;
        if (id >= static_cast<uint8_t>(Block::Count)) return SchematicError::BadPalette;
        palette.push_back(static_cast<Block>(id));
    }

    uint8_t bits = 0;
    if (!body.getU8(bits)) return SchematicError::Truncated;
    if (bits != bitsForPalette(paletteSize)) return SchematicError::BadPalette;

    std::string name;
    std::string author;
    uint64_t tick = 0;
    uint8_t flags = 0;
    if (version == static_cast<uint32_t>(SchematicVersion::V2)) {
        if (!body.getString(name) || !body.getString(author)) return SchematicError::Truncated;
        if (!body.getU64(tick)) return SchematicError::Truncated;
        if (!body.getU8(flags)) return SchematicError::Truncated;
    }
    bool hasVoidMask = (flags & voidMaskFlag) != 0u;

    uint32_t rawLength = 0, payloadLength = 0;
    if (!body.getU32(rawLength) || !body.getU32(payloadLength)) return SchematicError::Truncated;
    if (payloadLength > body.remaining()) return SchematicError::Truncated;

    std::size_t expectedRaw = packedByteCount(cellCount, bits);
    if (hasVoidMask) expectedRaw += packedByteCount(cellCount, 1u);
    if (rawLength != expectedRaw) return SchematicError::DimensionMismatch;

    std::vector<uint8_t> payload(payloadLength, 0u);
    if (payloadLength != 0u && !body.getBytes(payload.data(), payloadLength))
        return SchematicError::Truncated;

    ByteBuffer raw;
    if (!decompress(payload.data(), payload.size(), raw)) return SchematicError::BadChecksum;
    if (raw.size() != expectedRaw) return SchematicError::DimensionMismatch;

    std::vector<uint16_t> indices;
    unpackIndices(raw.data(), cellCount, bits, indices);
    std::vector<uint16_t> presence;
    if (hasVoidMask)
        unpackIndices(raw.data() + packedByteCount(cellCount, bits), cellCount, 1u, presence);

    ClipboardBuffer buffer(dims, anchor);
    std::size_t cursor = 0;
    for (int y = 0; y < dims.y; ++y)
        for (int z = 0; z < dims.z; ++z)
            for (int x = 0; x < dims.x; ++x) {
                uint16_t slot = indices[cursor];
                if (slot >= paletteSize) return SchematicError::BadPalette;
                bool filled = hasVoidMask ? presence[cursor] != 0u : true;
                buffer.put(x, y, z, palette[slot], filled);
                ++cursor;
            }

    out = Schematic{};
    out.cells = buffer;
    out.meta.name = name;
    out.meta.author = author;
    out.meta.tick = tick;
    out.meta.version = SchematicVersion::V2;
    out.migrated = version == static_cast<uint32_t>(SchematicVersion::V1);
    out.refreshHeaderFromBuffer();
    return SchematicError::None;
}

std::size_t Schematic::pasteInto(World& world, const Vec3i& target,
                                 ClipboardBuffer::PasteMode mode) const {
    const Vec3i& dims = cells.dims();
    const Vec3i& anchor = cells.anchor();
    std::size_t written = 0;
    for (int y = 0; y < dims.y; ++y)
        for (int z = 0; z < dims.z; ++z)
            for (int x = 0; x < dims.x; ++x) {
                if (!cells.filledAt(x, y, z)) continue;
                Vec3i at{target.x + x - anchor.x, target.y + y - anchor.y,
                         target.z + z - anchor.z};
                Block existing = world.blockAt(at.x, at.y, at.z);
                if (mode == ClipboardBuffer::PasteMode::IntoAirOnly && !isAir(existing)) continue;
                if (mode == ClipboardBuffer::PasteMode::ReplaceExisting && isAir(existing)) continue;
                Block value = cells.at(x, y, z);
                if (existing == value) continue;
                world.setBlock(at.x, at.y, at.z, value);
                ++written;
            }
    return written;
}

std::size_t Schematic::pasteInto(World& world, EditHistory& history, const Vec3i& target,
                                 ClipboardBuffer::PasteMode mode, uint64_t tick) const {
    const Vec3i& dims = cells.dims();
    const Vec3i& anchor = cells.anchor();
    history.beginBatch(meta.name.empty() ? std::string("paste") : "paste " + meta.name, tick);
    std::size_t written = 0;
    for (int y = 0; y < dims.y; ++y)
        for (int z = 0; z < dims.z; ++z)
            for (int x = 0; x < dims.x; ++x) {
                if (!cells.filledAt(x, y, z)) continue;
                Vec3i at{target.x + x - anchor.x, target.y + y - anchor.y,
                         target.z + z - anchor.z};
                Block existing = world.blockAt(at.x, at.y, at.z);
                if (mode == ClipboardBuffer::PasteMode::IntoAirOnly && !isAir(existing)) continue;
                if (mode == ClipboardBuffer::PasteMode::ReplaceExisting && isAir(existing)) continue;
                if (history.recordSet(world, at, cells.at(x, y, z))) ++written;
            }
    history.endBatch();
    return written;
}

}
