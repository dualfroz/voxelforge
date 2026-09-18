#include "vf/Protocol.h"

namespace vf {

void PacketWriter::writePosition(int32_t x, int32_t y, int32_t z) {
    uint64_t packed = ((static_cast<uint64_t>(x) & 0x3FFFFFFull) << 38) |
                      ((static_cast<uint64_t>(z) & 0x3FFFFFFull) << 12) |
                      (static_cast<uint64_t>(y) & 0xFFFull);
    writer.putU64(packed);
}

void PacketWriter::writeUuid(uint64_t hi, uint64_t lo) {
    writer.putU64(hi);
    writer.putU64(lo);
}

namespace {

int32_t signExtend(uint64_t value, int bits) {
    uint64_t signBit = 1ull << (bits - 1);
    if (value & signBit) value |= ~((1ull << bits) - 1ull);
    return static_cast<int32_t>(static_cast<int64_t>(value));
}

}

bool PacketReader::readI32(int32_t& out) {
    uint32_t raw;
    if (!reader.getU32(raw)) return false;
    out = static_cast<int32_t>(raw);
    return true;
}

bool PacketReader::readI64(int64_t& out) {
    uint64_t raw;
    if (!reader.getU64(raw)) return false;
    out = static_cast<int64_t>(raw);
    return true;
}

bool PacketReader::readBool(bool& out) {
    uint8_t raw;
    if (!reader.getU8(raw)) return false;
    out = raw != 0;
    return true;
}

bool PacketReader::readBytes(std::vector<uint8_t>& out, std::size_t n) {
    out.resize(n);
    return reader.getBytes(out.data(), n);
}

bool PacketReader::readPosition(int32_t& x, int32_t& y, int32_t& z) {
    uint64_t packed;
    if (!reader.getU64(packed)) return false;
    x = signExtend(packed >> 38, 26);
    z = signExtend((packed >> 12) & 0x3FFFFFFull, 26);
    y = signExtend(packed & 0xFFFull, 12);
    return true;
}

bool PacketReader::readUuid(uint64_t& hi, uint64_t& lo) {
    return reader.getU64(hi) && reader.getU64(lo);
}

void Handshake::encode(PacketWriter& w) const {
    w.writeVarInt(protocolVersion);
    w.writeString(serverAddress);
    w.writeU16(serverPort);
    w.writeVarInt(nextState);
}

bool Handshake::decode(PacketReader& r) {
    return r.readVarInt(protocolVersion) && r.readString(serverAddress) &&
           r.readU16(serverPort) && r.readVarInt(nextState);
}

void LoginStart::encode(PacketWriter& w) const {
    w.writeString(name);
    w.writeUuid(uuidHi, uuidLo);
}

bool LoginStart::decode(PacketReader& r) {
    return r.readString(name) && r.readUuid(uuidHi, uuidLo);
}

void JoinGame::encode(PacketWriter& w) const {
    w.writeVarInt(entityId);
    w.writeU8(gameMode);
    w.writeVarInt(dimension);
    w.writeI64(static_cast<int64_t>(seedHash));
    w.writeU8(maxPlayers);
    w.writeString(levelType);
    w.writeVarInt(viewDistance);
    w.writeBool(reducedDebugInfo);
}

bool JoinGame::decode(PacketReader& r) {
    int64_t seed;
    if (!r.readVarInt(entityId) || !r.readU8(gameMode) || !r.readVarInt(dimension) ||
        !r.readI64(seed) || !r.readU8(maxPlayers) || !r.readString(levelType) ||
        !r.readVarInt(viewDistance) || !r.readBool(reducedDebugInfo))
        return false;
    seedHash = static_cast<uint64_t>(seed);
    return true;
}

void ChunkData::setFromChunk(int32_t cx, int32_t cy, int32_t cz, const Chunk& chunk) {
    chunkX = cx;
    chunkY = cy;
    chunkZ = cz;
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
    bitWidth = 1;
    while ((static_cast<std::size_t>(1) << bitWidth) < palette.size()) ++bitWidth;

    int perWord = 64 / bitWidth;
    std::size_t wordCount = (indices.size() + static_cast<std::size_t>(perWord) - 1) /
                            static_cast<std::size_t>(perWord);
    packed.assign(wordCount, 0);
    uint64_t mask = bitWidth >= 64 ? ~0ull : ((1ull << bitWidth) - 1ull);
    for (std::size_t i = 0; i < indices.size(); ++i) {
        std::size_t word = i / static_cast<std::size_t>(perWord);
        int inWord = static_cast<int>(i % static_cast<std::size_t>(perWord));
        packed[word] |= (static_cast<uint64_t>(indices[i]) & mask) << (inWord * bitWidth);
    }
}

void ChunkData::toChunk(Chunk& out) const {
    int perWord = 64 / bitWidth;
    uint64_t mask = bitWidth >= 64 ? ~0ull : ((1ull << bitWidth) - 1ull);
    std::array<Block, chunkVolume>& blocks = out.raw();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        std::size_t word = i / static_cast<std::size_t>(perWord);
        int inWord = static_cast<int>(i % static_cast<std::size_t>(perWord));
        uint32_t index = word < packed.size()
                             ? static_cast<uint32_t>((packed[word] >> (inWord * bitWidth)) & mask)
                             : 0;
        blocks[i] = index < palette.size() ? palette[index] : Block::Air;
    }
}

void ChunkData::encode(PacketWriter& w) const {
    w.writeVarInt(chunkX);
    w.writeVarInt(chunkY);
    w.writeVarInt(chunkZ);

    ByteWriter raw;
    raw.putVarU32(static_cast<uint32_t>(palette.size()));
    for (Block block : palette) raw.putU8(static_cast<uint8_t>(block));
    raw.putU8(static_cast<uint8_t>(bitWidth));
    raw.putVarU32(static_cast<uint32_t>(packed.size()));
    for (uint64_t word : packed) raw.putU64(word);

    ByteBuffer compressed = compress(raw.bytes());
    w.writeVarUint(static_cast<uint32_t>(compressed.size()));
    w.writeBytes(compressed.data(), compressed.size());
}

bool ChunkData::decode(PacketReader& r) {
    if (!r.readVarInt(chunkX) || !r.readVarInt(chunkY) || !r.readVarInt(chunkZ)) return false;
    uint32_t compressedLen;
    if (!r.readVarUint(compressedLen)) return false;
    std::vector<uint8_t> compressed;
    if (!r.readBytes(compressed, compressedLen)) return false;
    ByteBuffer raw;
    if (!decompress(compressed.data(), compressed.size(), raw)) return false;

    ByteReader body(raw.data(), raw.size());
    uint32_t paletteSize;
    if (!body.getVarU32(paletteSize)) return false;
    palette.clear();
    palette.reserve(paletteSize);
    for (uint32_t i = 0; i < paletteSize; ++i) {
        uint8_t block;
        if (!body.getU8(block)) return false;
        palette.push_back(static_cast<Block>(block));
    }
    uint8_t width;
    if (!body.getU8(width) || width < 1 || width > 64) return false;
    bitWidth = width;
    uint32_t wordCount;
    if (!body.getVarU32(wordCount)) return false;
    packed.clear();
    packed.reserve(wordCount);
    for (uint32_t i = 0; i < wordCount; ++i) {
        uint64_t word;
        if (!body.getU64(word)) return false;
        packed.push_back(word);
    }
    return true;
}

void BlockChange::encode(PacketWriter& w) const {
    w.writePosition(x, y, z);
    w.writeVarUint(static_cast<uint32_t>(block));
}

bool BlockChange::decode(PacketReader& r) {
    uint32_t rawBlock;
    if (!r.readPosition(x, y, z) || !r.readVarUint(rawBlock)) return false;
    block = static_cast<Block>(rawBlock);
    return true;
}

void MultiBlockChange::encode(PacketWriter& w) const {
    w.writeVarInt(chunkX);
    w.writeVarInt(chunkY);
    w.writeVarInt(chunkZ);
    w.writeVarUint(static_cast<uint32_t>(changes.size()));
    for (const Change& change : changes) {
        w.writeU16(change.localIndex);
        w.writeU8(static_cast<uint8_t>(change.block));
    }
}

bool MultiBlockChange::decode(PacketReader& r) {
    if (!r.readVarInt(chunkX) || !r.readVarInt(chunkY) || !r.readVarInt(chunkZ)) return false;
    uint32_t count;
    if (!r.readVarUint(count)) return false;
    changes.clear();
    changes.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Change change;
        uint8_t block;
        if (!r.readU16(change.localIndex) || !r.readU8(block)) return false;
        change.block = static_cast<Block>(block);
        changes.push_back(change);
    }
    return true;
}

void EntityMove::encode(PacketWriter& w) const {
    w.writeVarInt(entityId);
    w.writeFloat(x);
    w.writeFloat(y);
    w.writeFloat(z);
    w.writeFloat(yaw);
    w.writeFloat(pitch);
    w.writeBool(onGround);
}

bool EntityMove::decode(PacketReader& r) {
    return r.readVarInt(entityId) && r.readFloat(x) && r.readFloat(y) && r.readFloat(z) &&
           r.readFloat(yaw) && r.readFloat(pitch) && r.readBool(onGround);
}

void PlayerState::encode(PacketWriter& w) const {
    w.writeFloat(x);
    w.writeFloat(y);
    w.writeFloat(z);
    w.writeFloat(yaw);
    w.writeFloat(pitch);
    w.writeFloat(health);
    w.writeVarInt(food);
    w.writeVarInt(xpLevel);
    w.writeU8(selectedSlot);
}

bool PlayerState::decode(PacketReader& r) {
    return r.readFloat(x) && r.readFloat(y) && r.readFloat(z) && r.readFloat(yaw) &&
           r.readFloat(pitch) && r.readFloat(health) && r.readVarInt(food) &&
           r.readVarInt(xpLevel) && r.readU8(selectedSlot);
}

void KeepAlive::encode(PacketWriter& w) const { w.writeVarLong(token); }

bool KeepAlive::decode(PacketReader& r) { return r.readVarLong(token); }

void writeFramed(ByteWriter& stream, PacketId id, const PacketWriter& body) {
    uint32_t frameLength = static_cast<uint32_t>(1 + body.size());
    stream.putVarU32(frameLength);
    stream.putU8(static_cast<uint8_t>(id));
    stream.putBytes(body.bytes().data(), body.bytes().size());
}

namespace {

bool peekVarU32(const uint8_t* data, std::size_t size, std::size_t pos, uint32_t& value,
                std::size_t& next) {
    value = 0;
    int shift = 0;
    while (shift < 35) {
        if (pos >= size) return false;
        uint8_t byte = data[pos++];
        value |= static_cast<uint32_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) {
            next = pos;
            return true;
        }
        shift += 7;
    }
    return false;
}

}

void splitFrames(const uint8_t* data, std::size_t size, std::vector<Frame>& frames,
                 std::size_t& consumed) {
    frames.clear();
    std::size_t pos = 0;
    for (;;) {
        std::size_t afterLength = 0;
        uint32_t frameLength = 0;
        if (!peekVarU32(data, size, pos, frameLength, afterLength)) break;
        if (frameLength == 0) break;
        if (afterLength + frameLength > size) break;

        Frame frame;
        frame.id = static_cast<PacketId>(data[afterLength]);
        frame.payload.assign(data + afterLength + 1, data + afterLength + frameLength);
        frames.push_back(std::move(frame));
        pos = afterLength + frameLength;
    }
    consumed = pos;
}

}
