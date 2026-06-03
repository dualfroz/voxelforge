#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "Block.h"
#include "Chunk.h"
#include "Codec.h"

namespace vf {

enum class PacketId : uint8_t {
    Handshake = 0,
    LoginStart = 1,
    JoinGame = 2,
    ChunkData = 3,
    BlockChange = 4,
    MultiBlockChange = 5,
    EntityMove = 6,
    PlayerState = 7,
    KeepAlive = 8,
};

// Wire writer: varints (unsigned LEB128 and zigzag signed), fixed scalars,
// length-prefixed strings, a 26/12/26 packed block position, and a 128-bit
// uuid split into two 64-bit halves.
class PacketWriter {
public:
    void writeVarInt(int32_t v) { writer.putVarI32(v); }
    void writeVarUint(uint32_t v) { writer.putVarU32(v); }
    void writeVarLong(int64_t v) { writer.putVarI64(v); }
    void writeU8(uint8_t v) { writer.putU8(v); }
    void writeU16(uint16_t v) { writer.putU16(v); }
    void writeI32(int32_t v) { writer.putU32(static_cast<uint32_t>(v)); }
    void writeI64(int64_t v) { writer.putU64(static_cast<uint64_t>(v)); }
    void writeBool(bool v) { writer.putU8(v ? 1 : 0); }
    void writeFloat(float v) { writer.putFloat(v); }
    void writeDouble(double v) { writer.putDouble(v); }
    void writeString(const std::string& s) { writer.putString(s); }
    void writeBytes(const uint8_t* p, std::size_t n) { writer.putBytes(p, n); }
    void writePosition(int32_t x, int32_t y, int32_t z);
    void writeUuid(uint64_t hi, uint64_t lo);

    std::size_t size() const { return writer.size(); }
    const ByteBuffer& bytes() const { return writer.bytes(); }

private:
    ByteWriter writer;
};

// Wire reader mirroring PacketWriter. Every read is bounds checked; ok() stays
// false once any read has run past the end.
class PacketReader {
public:
    PacketReader(const uint8_t* data, std::size_t size) : reader(data, size) {}

    bool readVarInt(int32_t& out) { return reader.getVarI32(out); }
    bool readVarUint(uint32_t& out) { return reader.getVarU32(out); }
    bool readVarLong(int64_t& out) { return reader.getVarI64(out); }
    bool readU8(uint8_t& out) { return reader.getU8(out); }
    bool readU16(uint16_t& out) { return reader.getU16(out); }
    bool readI32(int32_t& out);
    bool readI64(int64_t& out);
    bool readBool(bool& out);
    bool readFloat(float& out) { return reader.getFloat(out); }
    bool readDouble(double& out) { return reader.getDouble(out); }
    bool readString(std::string& out) { return reader.getString(out); }
    bool readBytes(std::vector<uint8_t>& out, std::size_t n);
    bool readPosition(int32_t& x, int32_t& y, int32_t& z);
    bool readUuid(uint64_t& hi, uint64_t& lo);

    bool ok() const { return reader.ok(); }
    std::size_t remaining() const { return reader.remaining(); }

private:
    ByteReader reader;
};

struct Handshake {
    static constexpr PacketId id = PacketId::Handshake;
    int32_t protocolVersion = 0;
    std::string serverAddress;
    uint16_t serverPort = 0;
    int32_t nextState = 0;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct LoginStart {
    static constexpr PacketId id = PacketId::LoginStart;
    std::string name;
    uint64_t uuidHi = 0;
    uint64_t uuidLo = 0;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct JoinGame {
    static constexpr PacketId id = PacketId::JoinGame;
    int32_t entityId = 0;
    uint8_t gameMode = 0;
    int32_t dimension = 0;
    uint64_t seedHash = 0;
    uint8_t maxPlayers = 0;
    std::string levelType;
    int32_t viewDistance = 0;
    bool reducedDebugInfo = false;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

// A full chunk: coordinate plus the palette and bit-packed indices, carried on
// the wire as a Codec-compressed blob.
struct ChunkData {
    static constexpr PacketId id = PacketId::ChunkData;
    int32_t chunkX = 0;
    int32_t chunkY = 0;
    int32_t chunkZ = 0;
    std::vector<Block> palette;
    int bitWidth = 1;
    std::vector<uint64_t> packed;

    void setFromChunk(int32_t cx, int32_t cy, int32_t cz, const Chunk& chunk);
    void toChunk(Chunk& out) const;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct BlockChange {
    static constexpr PacketId id = PacketId::BlockChange;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    Block block = Block::Air;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct MultiBlockChange {
    static constexpr PacketId id = PacketId::MultiBlockChange;
    struct Change {
        uint16_t localIndex = 0;
        Block block = Block::Air;
    };
    int32_t chunkX = 0;
    int32_t chunkY = 0;
    int32_t chunkZ = 0;
    std::vector<Change> changes;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct EntityMove {
    static constexpr PacketId id = PacketId::EntityMove;
    int32_t entityId = 0;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    bool onGround = false;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct PlayerState {
    static constexpr PacketId id = PacketId::PlayerState;
    float x = 0, y = 0, z = 0;
    float yaw = 0, pitch = 0;
    float health = 0;
    int32_t food = 0;
    int32_t xpLevel = 0;
    uint8_t selectedSlot = 0;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

struct KeepAlive {
    static constexpr PacketId id = PacketId::KeepAlive;
    int64_t token = 0;
    void encode(PacketWriter& w) const;
    bool decode(PacketReader& r);
};

// Length-prefixed framing: varint(1 + payload) + packetId + payload.
void writeFramed(ByteWriter& stream, PacketId id, const PacketWriter& body);

struct Frame {
    PacketId id = PacketId::Handshake;
    ByteBuffer payload;
};

// Pulls every complete frame out of a possibly-glued stream. `consumed` is set
// to the number of bytes fully parsed, so a trailing partial frame can be kept
// for the next read.
void splitFrames(const uint8_t* data, std::size_t size, std::vector<Frame>& frames,
                 std::size_t& consumed);

} // namespace vf
