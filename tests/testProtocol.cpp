#include "testHarness.h"
#include "vf/Protocol.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint32_t next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(state >> 32);
    }
};

template <typename Packet>
Packet reencode(const Packet& original) {
    PacketWriter w;
    original.encode(w);
    PacketReader r(w.bytes().data(), w.bytes().size());
    Packet restored;
    CHECK(restored.decode(r));
    CHECK(r.remaining() == 0);
    return restored;
}

Chunk mixedChunk() {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) {
                int v = (x + y * 3 + z * 5) % 6;
                Block table[6] = {Block::Air,  Block::Stone, Block::Dirt,
                                  Block::Grass, Block::Sand,  Block::Water};
                chunk.set(x, y, z, table[v]);
            }
    return chunk;
}

}  // namespace

VF_TEST(protocolHandshakeRoundTrip) {
    Handshake packet;
    packet.protocolVersion = 763;
    packet.serverAddress = "voxelforge.local";
    packet.serverPort = 25565;
    packet.nextState = 2;
    Handshake back = reencode(packet);
    CHECK_EQ(back.protocolVersion, 763);
    CHECK(back.serverAddress == "voxelforge.local");
    CHECK_EQ(back.serverPort, static_cast<uint16_t>(25565));
    CHECK_EQ(back.nextState, 2);
}

VF_TEST(protocolLoginStartRoundTrip) {
    LoginStart packet;
    packet.name = "foksiu";
    packet.uuidHi = 0x0011223344556677ull;
    packet.uuidLo = 0x8899AABBCCDDEEFFull;
    LoginStart back = reencode(packet);
    CHECK(back.name == "foksiu");
    CHECK(back.uuidHi == 0x0011223344556677ull);
    CHECK(back.uuidLo == 0x8899AABBCCDDEEFFull);
}

VF_TEST(protocolJoinGameRoundTrip) {
    JoinGame packet;
    packet.entityId = 4242;
    packet.gameMode = 1;
    packet.dimension = -1;
    packet.seedHash = 0xDEADBEEFCAFEBABEull;
    packet.maxPlayers = 20;
    packet.levelType = "default";
    packet.viewDistance = 12;
    packet.reducedDebugInfo = true;
    JoinGame back = reencode(packet);
    CHECK_EQ(back.entityId, 4242);
    CHECK_EQ(back.gameMode, static_cast<uint8_t>(1));
    CHECK_EQ(back.dimension, -1);
    CHECK(back.seedHash == 0xDEADBEEFCAFEBABEull);
    CHECK(back.levelType == "default");
    CHECK_EQ(back.viewDistance, 12);
    CHECK(back.reducedDebugInfo);
}

VF_TEST(protocolBlockChangePositionSignExtends) {
    const int coords[][3] = {{0, 0, 0}, {100, 64, -100}, {-33554432, -2048, 33554431},
                             {-1, -1, -1}, {1234567, 300, -7654321}};
    for (const auto& c : coords) {
        BlockChange packet;
        packet.x = c[0];
        packet.y = c[1];
        packet.z = c[2];
        packet.block = Block::RedstoneLamp;
        BlockChange back = reencode(packet);
        CHECK_EQ(back.x, c[0]);
        CHECK_EQ(back.y, c[1]);
        CHECK_EQ(back.z, c[2]);
        CHECK(back.block == Block::RedstoneLamp);
    }
}

VF_TEST(protocolMultiBlockChangeRoundTrip) {
    MultiBlockChange packet;
    packet.chunkX = -5;
    packet.chunkY = 2;
    packet.chunkZ = 9;
    for (uint16_t i = 0; i < 300; ++i)
        packet.changes.push_back({static_cast<uint16_t>(i * 13 % chunkVolume),
                                  static_cast<Block>(1 + i % (static_cast<int>(Block::Count) - 1))});
    MultiBlockChange back = reencode(packet);
    CHECK_EQ(back.chunkX, -5);
    CHECK_EQ(back.changes.size(), packet.changes.size());
    bool same = true;
    for (std::size_t i = 0; i < packet.changes.size(); ++i)
        if (back.changes[i].localIndex != packet.changes[i].localIndex ||
            back.changes[i].block != packet.changes[i].block)
            same = false;
    CHECK(same);
}

VF_TEST(protocolEntityMoveRoundTrip) {
    EntityMove packet;
    packet.entityId = 77;
    packet.x = 12.5f;
    packet.y = -3.25f;
    packet.z = 100.125f;
    packet.yaw = 90.0f;
    packet.pitch = -45.0f;
    packet.onGround = true;
    EntityMove back = reencode(packet);
    CHECK_EQ(back.entityId, 77);
    CHECK(back.x == 12.5f && back.y == -3.25f && back.z == 100.125f);
    CHECK(back.yaw == 90.0f && back.pitch == -45.0f);
    CHECK(back.onGround);
}

VF_TEST(protocolPlayerStateRoundTrip) {
    PlayerState packet;
    packet.x = 1.0f;
    packet.y = 65.0f;
    packet.z = -8.0f;
    packet.yaw = 180.0f;
    packet.pitch = 10.0f;
    packet.health = 15.5f;
    packet.food = 17;
    packet.xpLevel = 30;
    packet.selectedSlot = 4;
    PlayerState back = reencode(packet);
    CHECK(back.health == 15.5f);
    CHECK_EQ(back.food, 17);
    CHECK_EQ(back.xpLevel, 30);
    CHECK_EQ(back.selectedSlot, static_cast<uint8_t>(4));
}

VF_TEST(protocolKeepAliveRoundTrip) {
    KeepAlive packet;
    packet.token = -9000000000000000000LL;
    KeepAlive back = reencode(packet);
    CHECK(back.token == -9000000000000000000LL);
}

VF_TEST(protocolChunkDataRoundTripThroughCodec) {
    Chunk chunk = mixedChunk();
    ChunkData packet;
    packet.setFromChunk(3, -1, 7, chunk);
    CHECK(packet.palette.size() >= 6);

    PacketWriter w;
    packet.encode(w);
    // The palette + packed payload must be compressed on the wire.
    CHECK(w.size() < chunkVolume);

    PacketReader r(w.bytes().data(), w.bytes().size());
    ChunkData back;
    CHECK(back.decode(r));
    CHECK_EQ(back.chunkX, 3);
    CHECK_EQ(back.chunkY, -1);
    CHECK_EQ(back.chunkZ, 7);
    Chunk rebuilt;
    back.toChunk(rebuilt);
    CHECK(rebuilt.raw() == chunk.raw());
}

VF_TEST(protocolFramingSplitsGluedStream) {
    ByteWriter stream;
    Handshake handshake;
    handshake.protocolVersion = 763;
    handshake.serverAddress = "host";
    handshake.serverPort = 25565;
    handshake.nextState = 1;
    PacketWriter hw;
    handshake.encode(hw);
    writeFramed(stream, PacketId::Handshake, hw);

    KeepAlive keepAlive;
    keepAlive.token = 123456789;
    PacketWriter kw;
    keepAlive.encode(kw);
    writeFramed(stream, PacketId::KeepAlive, kw);

    BlockChange blockChange;
    blockChange.x = 10;
    blockChange.y = 20;
    blockChange.z = 30;
    blockChange.block = Block::Glowstone;
    PacketWriter bw;
    blockChange.encode(bw);
    writeFramed(stream, PacketId::BlockChange, bw);

    std::vector<Frame> frames;
    std::size_t consumed = 0;
    splitFrames(stream.bytes().data(), stream.bytes().size(), frames, consumed);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(3));
    CHECK_EQ(consumed, stream.bytes().size());
    CHECK(frames[0].id == PacketId::Handshake);
    CHECK(frames[1].id == PacketId::KeepAlive);
    CHECK(frames[2].id == PacketId::BlockChange);

    PacketReader r0(frames[0].payload.data(), frames[0].payload.size());
    Handshake h;
    CHECK(h.decode(r0) && h.protocolVersion == 763 && h.serverAddress == "host");
    PacketReader r2(frames[2].payload.data(), frames[2].payload.size());
    BlockChange b;
    CHECK(b.decode(r2) && b.x == 10 && b.y == 20 && b.z == 30 && b.block == Block::Glowstone);
}

VF_TEST(protocolFramingKeepsTrailingPartial) {
    ByteWriter stream;
    KeepAlive first;
    first.token = 1;
    PacketWriter fw;
    first.encode(fw);
    writeFramed(stream, PacketId::KeepAlive, fw);
    std::size_t completeEnd = stream.bytes().size();

    KeepAlive second;
    second.token = 2;
    PacketWriter sw;
    second.encode(sw);
    writeFramed(stream, PacketId::KeepAlive, sw);

    // Cut the stream one byte before the second frame ends: only the first is whole.
    ByteBuffer truncated(stream.bytes().begin(), stream.bytes().end() - 1);
    std::vector<Frame> frames;
    std::size_t consumed = 0;
    splitFrames(truncated.data(), truncated.size(), frames, consumed);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    CHECK_EQ(consumed, completeEnd);

    // Feeding the withheld tail back completes the second frame.
    std::vector<Frame> frames2;
    std::size_t consumed2 = 0;
    splitFrames(stream.bytes().data(), stream.bytes().size(), frames2, consumed2);
    CHECK_EQ(frames2.size(), static_cast<std::size_t>(2));
}

VF_TEST(protocolManyRandomPacketsFramedAndRecovered) {
    Lcg rng(0x7777);
    ByteWriter stream;
    std::vector<int64_t> tokens;
    for (int i = 0; i < 50; ++i) {
        KeepAlive packet;
        packet.token = static_cast<int64_t>((static_cast<uint64_t>(rng.next()) << 32) | rng.next());
        tokens.push_back(packet.token);
        PacketWriter w;
        packet.encode(w);
        writeFramed(stream, PacketId::KeepAlive, w);
    }
    std::vector<Frame> frames;
    std::size_t consumed = 0;
    splitFrames(stream.bytes().data(), stream.bytes().size(), frames, consumed);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(50));
    bool allMatch = true;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        PacketReader r(frames[i].payload.data(), frames[i].payload.size());
        KeepAlive back;
        if (!back.decode(r) || back.token != tokens[i]) allMatch = false;
    }
    CHECK(allMatch);
}

VF_TEST(protocolTruncatedPayloadFailsDecode) {
    JoinGame packet;
    packet.entityId = 1;
    packet.levelType = "flat";
    PacketWriter w;
    packet.encode(w);
    ByteBuffer half(w.bytes().begin(), w.bytes().begin() + w.bytes().size() / 2);
    PacketReader r(half.data(), half.size());
    JoinGame back;
    CHECK(!back.decode(r));
}
