#include "testHarness.h"
#include "vf/ChunkSync.h"

#include <cstdint>
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
    int range(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

void serverEdit(World& world, ServerChunkSync& sync, const Vec3i& chunkCoord, int x, int y, int z,
                Block block) {
    Chunk& chunk = world.ensureChunk(chunkCoord);
    chunk.set(x, y, z, block);
    sync.markDirty(chunkCoord, x, y, z);
}

bool chunksEqual(const World& a, const World& b, const Vec3i& coord) {
    const Chunk* ca = a.chunkAt(coord);
    const Chunk* cb = b.chunkAt(coord);
    if (!ca || !cb) return false;
    return ca->raw() == cb->raw();
}

ChunkData reframeChunkData(const ChunkData& packet) {
    PacketWriter w;
    packet.encode(w);
    PacketReader r(w.bytes().data(), w.bytes().size());
    ChunkData back;
    CHECK(back.decode(r));
    return back;
}

MultiBlockChange reframeDelta(const MultiBlockChange& packet) {
    PacketWriter w;
    packet.encode(w);
    PacketReader r(w.bytes().data(), w.bytes().size());
    MultiBlockChange back;
    CHECK(back.decode(r));
    return back;
}

}

VF_TEST(chunkSyncFullSnapshotReplicatesChunk) {
    World server;
    ServerChunkSync sync;
    Vec3i coord{2, -1, 3};
    for (int x = 0; x < chunkSize; ++x)
        for (int z = 0; z < chunkSize; ++z) {
            server.ensureChunk(coord).set(x, 0, z, Block::Bedrock);
            for (int y = 1; y < 6; ++y) server.ensureChunk(coord).set(x, y, z, Block::Stone);
            server.ensureChunk(coord).set(x, 6, z, Block::Grass);
        }

    ChunkData full;
    CHECK(sync.buildFull(server, coord, full));
    ChunkData wire = reframeChunkData(full);

    World client;
    ClientChunkSync clientSync;
    clientSync.applyFull(wire, client);
    CHECK(chunksEqual(server, client, coord));
}

VF_TEST(chunkSyncDeltaAppliesDirtyBlocks) {
    World server;
    ServerChunkSync sync;
    Vec3i coord{0, 0, 0};
    server.ensureChunk(coord);

    World client;
    ClientChunkSync clientSync;
    ChunkData full;
    CHECK(sync.buildFull(server, coord, full));
    clientSync.applyFull(reframeChunkData(full), client);
    CHECK(chunksEqual(server, client, coord));

    serverEdit(server, sync, coord, 1, 1, 1, Block::Glowstone);
    serverEdit(server, sync, coord, 5, 8, 2, Block::DiamondOre);
    serverEdit(server, sync, coord, 15, 15, 15, Block::Obsidian);
    CHECK_EQ(sync.dirtyCount(coord), static_cast<std::size_t>(3));

    MultiBlockChange delta;
    CHECK(sync.buildDelta(server, coord, delta));
    clientSync.applyDelta(reframeDelta(delta), client);
    CHECK(chunksEqual(server, client, coord));

    sync.clear(coord);
    CHECK(!sync.anyDirty());
    MultiBlockChange empty;
    CHECK(!sync.buildDelta(server, coord, empty));
}

VF_TEST(chunkSyncSingleBlockChange) {
    World client;
    ClientChunkSync clientSync;
    BlockChange packet;
    packet.x = -7;
    packet.y = 70;
    packet.z = 42;
    packet.block = Block::RedstoneLampLit;
    clientSync.applyBlockChange(packet, client);
    CHECK(client.blockAt(-7, 70, 42) == Block::RedstoneLampLit);
}

VF_TEST(chunkSyncResendThreshold) {
    World server;
    ServerChunkSync sync;
    Vec3i coord{1, 0, 1};
    server.ensureChunk(coord);
    for (int i = 0; i < 40; ++i) serverEdit(server, sync, coord, i % 16, i / 16, 0, Block::Sand);
    CHECK(sync.shouldResendFull(coord, 32));
    CHECK(!sync.shouldResendFull(coord, 1000));
}

VF_TEST(chunkSyncOverlappingEditsCollapse) {
    World server;
    ServerChunkSync sync;
    Vec3i coord{0, 0, 0};
    server.ensureChunk(coord);
    serverEdit(server, sync, coord, 3, 3, 3, Block::Dirt);
    serverEdit(server, sync, coord, 3, 3, 3, Block::Stone);
    serverEdit(server, sync, coord, 3, 3, 3, Block::Grass);
    CHECK_EQ(sync.dirtyCount(coord), static_cast<std::size_t>(1));

    MultiBlockChange delta;
    CHECK(sync.buildDelta(server, coord, delta));
    CHECK_EQ(delta.changes.size(), static_cast<std::size_t>(1));
    CHECK(delta.changes[0].block == Block::Grass);
}

VF_TEST(chunkSyncManyTicksConverge) {
    World server;
    World client;
    ServerChunkSync sync;
    ClientChunkSync clientSync;
    Vec3i coord{4, 2, -6};
    server.ensureChunk(coord);

    ChunkData full;
    CHECK(sync.buildFull(server, coord, full));
    clientSync.applyFull(reframeChunkData(full), client);

    Lcg rng(0xC0FFEE);
    Block palette[6] = {Block::Air,  Block::Stone,     Block::Dirt,
                        Block::Water, Block::Glowstone, Block::Sand};
    for (int tick = 0; tick < 30; ++tick) {
        int edits = 1 + rng.range(20);
        for (int e = 0; e < edits; ++e) {
            int x = rng.range(chunkSize);
            int y = rng.range(chunkSize);
            int z = rng.range(chunkSize);
            serverEdit(server, sync, coord, x, y, z, palette[rng.range(6)]);
        }
        MultiBlockChange delta;
        if (sync.buildDelta(server, coord, delta)) {
            clientSync.applyDelta(reframeDelta(delta), client);
            sync.clear(coord);
        }
    }
    CHECK(chunksEqual(server, client, coord));
    CHECK(!sync.anyDirty());
}

VF_TEST(chunkSyncFramedEndToEnd) {
    World server;
    ServerChunkSync sync;
    Vec3i coord{7, 0, 7};
    Chunk& source = server.ensureChunk(coord);
    for (int i = 0; i < chunkVolume; ++i)
        source.raw()[static_cast<std::size_t>(i)] = static_cast<Block>(i % 5);

    ChunkData full;
    CHECK(sync.buildFull(server, coord, full));

    ByteWriter stream;
    PacketWriter body;
    full.encode(body);
    writeFramed(stream, PacketId::ChunkData, body);

    std::vector<Frame> frames;
    std::size_t consumed = 0;
    splitFrames(stream.bytes().data(), stream.bytes().size(), frames, consumed);
    CHECK_EQ(frames.size(), static_cast<std::size_t>(1));
    CHECK(frames[0].id == PacketId::ChunkData);

    PacketReader r(frames[0].payload.data(), frames[0].payload.size());
    ChunkData back;
    CHECK(back.decode(r));

    World client;
    ClientChunkSync clientSync;
    clientSync.applyFull(back, client);
    CHECK(chunksEqual(server, client, coord));
}
