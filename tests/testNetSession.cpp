#include "testHarness.h"
#include "vf/NetSession.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed | 1ull) {}
    uint32_t next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(state >> 32);
    }
    int range(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

// Deterministic terrain in the chunk plane y = 0, so both ends have something
// substantial to disagree about if replication is wrong.
void buildTerrain(World& world, int columnsRadius) {
    Lcg rng(0x51ed270b);
    const Block palette[5] = {Block::Stone, Block::Dirt, Block::Grass, Block::Sand, Block::Gravel};
    for (int cx = -columnsRadius; cx <= columnsRadius; ++cx) {
        for (int cz = -columnsRadius; cz <= columnsRadius; ++cz) {
            Chunk& chunk = world.ensureChunk(Vec3i{cx, 0, cz});
            for (int x = 0; x < chunkSize; ++x) {
                for (int z = 0; z < chunkSize; ++z) {
                    int height = 4 + rng.range(9);
                    for (int y = 0; y < height; ++y)
                        chunk.set(x, y, z, palette[rng.range(5)]);
                }
            }
        }
    }
}

// Canonical over the whole world: chunk coordinates are sorted before hashing,
// so the digest does not inherit the hash map's iteration order.
uint64_t worldDigest(const World& world, const std::vector<Vec3i>& coords) {
    ByteWriter stream;
    for (const Vec3i& coord : coords) {
        const Chunk* chunk = world.chunkAt(coord);
        if (!chunk) continue;
        stream.putU32(static_cast<uint32_t>(coord.x));
        stream.putU32(static_cast<uint32_t>(coord.y));
        stream.putU32(static_cast<uint32_t>(coord.z));
        for (Block block : chunk->raw()) stream.putU8(static_cast<uint8_t>(block));
    }
    return fnv1a64(stream.bytes().data(), stream.bytes().size());
}

std::vector<Vec3i> sortedCoords(const std::unordered_set<Vec3i, Vec3iHash>& set) {
    std::vector<Vec3i> coords(set.begin(), set.end());
    for (std::size_t i = 1; i < coords.size(); ++i) {
        Vec3i key = coords[i];
        std::size_t j = i;
        while (j > 0 && (coords[j - 1].y > key.y ||
                         (coords[j - 1].y == key.y &&
                          (coords[j - 1].z > key.z ||
                           (coords[j - 1].z == key.z && coords[j - 1].x > key.x))))) {
            coords[j] = coords[j - 1];
            --j;
        }
        coords[j] = key;
    }
    return coords;
}

// Both ends of one connection, wired through a single loopback link.
struct Pair {
    LoopbackLink link;
    Transport serverTransport;
    Transport clientTransport;
    NetSession server;
    NetSession client;
    World serverWorld;
    World clientWorld;
    ServerChunkSync sync;

    Pair(const LinkQuality& quality, const TransportConfig& transportConfig,
         const SessionConfig& sessionConfig)
        : link(quality),
          serverTransport(link, 0, transportConfig),
          clientTransport(link, 1, transportConfig),
          server(serverTransport, NetSession::Role::Server, sessionConfig),
          client(clientTransport, NetSession::Role::Client, sessionConfig) {
        server.attachWorld(serverWorld);
        server.attachChunkSync(sync);
        server.setEntityId(7);
        client.attachWorld(clientWorld);
    }

    void step(uint64_t tick) {
        server.receive(tick);
        server.send(tick);
        client.receive(tick);
        client.send(tick);
    }

    // Server edit: authoritative world, dirty feed, and this session's own
    // per-chunk work list all move together.
    void edit(int gx, int gy, int gz, Block block) {
        serverWorld.setBlock(gx, gy, gz, block);
        Vec3i coord{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
        sync.markDirty(coord, floorMod(gx, chunkSize), floorMod(gy, chunkSize),
                       floorMod(gz, chunkSize));
        server.markChunkDirty(coord);
    }

    void clearDirty() { sync.clearAll(); }
};

SessionConfig quietConfig() {
    SessionConfig config;
    config.viewRadius = 2;
    config.chunksPerTick = 3;
    config.keepAliveIntervalTicks = 20;
    config.keepAliveTimeoutTicks = 1000000;
    return config;
}

}  // namespace

VF_TEST(handshakeAndLoginCarryBothEndsOfTheSessionIntoThePlayPhase) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    Pair pair(quality, transportConfig, quietConfig());

    CHECK(pair.server.phase() == SessionPhase::Handshake);
    CHECK(pair.client.phase() == SessionPhase::Handshake);

    pair.client.beginHandshake("vulp", 0x1111222233334444ull, 0x5555666677778888ull);
    CHECK(pair.client.phase() == SessionPhase::Login);

    for (uint64_t tick = 0; tick < 12; ++tick) pair.step(tick);

    CHECK(pair.server.phase() == SessionPhase::Play);
    CHECK(pair.client.phase() == SessionPhase::Play);
    CHECK(pair.server.playerName() == "vulp");
    CHECK_EQ(pair.server.uuidHigh(), 0x1111222233334444ull);
    CHECK_EQ(pair.client.entityId(), 7);
    CHECK(pair.server.disconnectReason().empty());
}

VF_TEST(aPlayPacketArrivingBeforeLoginEndsTheSessionWithAReasonInsteadOfCrashing) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    Pair pair(quality, transportConfig, quietConfig());

    // A well-formed PlayerState sent straight at a server still in handshake.
    PlayerState early;
    early.x = 12.0f;
    early.y = 40.0f;
    early.z = -3.0f;
    early.health = 20.0f;
    PacketWriter body;
    early.encode(body);
    ByteWriter stream;
    writeFramed(stream, PacketId::PlayerState, body);
    pair.clientTransport.sendReliable(stream.bytes());

    for (uint64_t tick = 0; tick < 10; ++tick) pair.step(tick);

    CHECK(pair.server.phase() == SessionPhase::Disconnected);
    CHECK(pair.server.disconnectReason() == "packet out of phase");
    CHECK_EQ(pair.server.stats().packetsRejected, static_cast<uint64_t>(1));
    CHECK(!pair.server.timedOut());
    CHECK_EQ(pair.server.view().position.x, 0.0f);
}

VF_TEST(aJoinGameArrivingAfterTheClientIsAlreadyPlayingIsRejected) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    Pair pair(quality, transportConfig, quietConfig());
    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 10; ++tick) pair.step(tick);
    CHECK(pair.client.phase() == SessionPhase::Play);

    JoinGame again;
    again.entityId = 99;
    again.levelType = "default";
    PacketWriter body;
    again.encode(body);
    ByteWriter stream;
    writeFramed(stream, PacketId::JoinGame, body);
    pair.serverTransport.sendReliable(stream.bytes());

    for (uint64_t tick = 10; tick < 20; ++tick) pair.step(tick);
    CHECK(pair.client.phase() == SessionPhase::Disconnected);
    CHECK(pair.client.disconnectReason() == "packet out of phase");
    CHECK_EQ(pair.client.entityId(), 7);
}

VF_TEST(theServerDisconnectsAClientThatStopsAnsweringKeepAlive) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    SessionConfig config = quietConfig();
    config.keepAliveIntervalTicks = 5;
    config.keepAliveTimeoutTicks = 30;
    Pair pair(quality, transportConfig, config);

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 60; ++tick) pair.step(tick);
    CHECK(pair.server.phase() == SessionPhase::Play);
    CHECK(pair.server.stats().keepAlivesSent > 5u);
    CHECK(!pair.server.timedOut());

    // The client goes silent: the server keeps ticking on its own.
    for (uint64_t tick = 60; tick < 130; ++tick) {
        pair.server.receive(tick);
        pair.server.send(tick);
    }
    CHECK(pair.server.phase() == SessionPhase::Disconnected);
    CHECK(pair.server.timedOut());
    CHECK(pair.server.disconnectReason() == "keep alive timeout");
}

VF_TEST(chunksAreStreamedNearestFirstAndNeverMoreThanTheTickBudget) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    SessionConfig config = quietConfig();
    config.chunksPerTick = 2;
    Pair pair(quality, transportConfig, config);
    buildTerrain(pair.serverWorld, 3);
    pair.server.view().position = Vec3{8.0f, 8.0f, 8.0f};

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 6; ++tick) pair.step(tick);

    CHECK(pair.server.phase() == SessionPhase::Play);
    uint64_t afterFirstTicks = pair.server.stats().chunksSent;
    CHECK(afterFirstTicks > 0u);
    CHECK(afterFirstTicks <= 2u * 6u);

    // The chunk the player stands in must be among the first handful sent.
    CHECK(pair.server.view().hasChunk(Vec3i{0, 0, 0}));
    CHECK(!pair.server.view().hasChunk(Vec3i{2, 0, 2}));

    for (uint64_t tick = 6; tick < 60; ++tick) pair.step(tick);
    CHECK_EQ(pair.server.view().pendingCount(), static_cast<std::size_t>(0));
    // The sphere of radius 2 holds 33 chunk slots, only 13 of which exist in
    // the y = 0 plane, so exactly those get snapshots.
    CHECK_EQ(pair.server.stats().chunksSent, static_cast<uint64_t>(13));
    CHECK_EQ(pair.client.stats().chunksApplied, static_cast<uint64_t>(13));
    CHECK_EQ(pair.clientWorld.chunkCount(), static_cast<std::size_t>(13));
}

VF_TEST(manyChangesInOneChunkBecomeAMultiBlockChangeAndFewBecomeSingleBlockChanges) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    SessionConfig config = quietConfig();
    config.multiBlockThreshold = 4;
    Pair pair(quality, transportConfig, config);
    buildTerrain(pair.serverWorld, 1);

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 40; ++tick) pair.step(tick);
    CHECK(pair.server.view().hasChunk(Vec3i{0, 0, 0}));

    for (int i = 0; i < 3; ++i) pair.edit(i, 14, 0, Block::Glowstone);
    pair.step(40);
    pair.clearDirty();
    for (uint64_t tick = 41; tick < 50; ++tick) pair.step(tick);
    CHECK_EQ(pair.server.stats().blockChangesSent, static_cast<uint64_t>(3));
    CHECK_EQ(pair.server.stats().multiBlockChangesSent, static_cast<uint64_t>(0));
    CHECK_EQ(pair.client.stats().blockChangesApplied, static_cast<uint64_t>(3));

    for (int i = 0; i < 9; ++i) pair.edit(i, 15, 0, Block::Obsidian);
    pair.step(50);
    pair.clearDirty();
    for (uint64_t tick = 51; tick < 60; ++tick) pair.step(tick);
    CHECK_EQ(pair.server.stats().blockChangesSent, static_cast<uint64_t>(3));
    CHECK_EQ(pair.server.stats().multiBlockChangesSent, static_cast<uint64_t>(1));
    CHECK_EQ(pair.client.stats().multiBlockChangesApplied, static_cast<uint64_t>(1));

    for (int i = 0; i < 9; ++i) CHECK(pair.clientWorld.blockAt(i, 15, 0) == Block::Obsidian);
    for (int i = 0; i < 3; ++i) CHECK(pair.clientWorld.blockAt(i, 14, 0) == Block::Glowstone);
}

VF_TEST(theClientWorldMirrorsTheServerByteForByteAcrossAnUnreliableLink) {
    LinkQuality quality;
    quality.lossPercent = 30;
    quality.duplicatePercent = 10;
    quality.reorderPercent = 25;
    quality.reorderExtraTicks = 4;
    quality.latencyTicks = 2;
    quality.seed = 0xabcdef0123456789ull;

    TransportConfig transportConfig;
    transportConfig.mtu = 512;
    transportConfig.retransmitTimeoutTicks = 10;
    transportConfig.sendWindow = 24;

    SessionConfig config = quietConfig();
    config.chunksPerTick = 2;
    config.multiBlockThreshold = 6;
    Pair pair(quality, transportConfig, config);
    buildTerrain(pair.serverWorld, 2);
    pair.server.view().position = Vec3{8.0f, 8.0f, 8.0f};

    pair.client.beginHandshake("vulp", 1, 2);

    Lcg rng(0x2ee9d1);
    const Block palette[4] = {Block::Glowstone, Block::Obsidian, Block::Ice, Block::Planks};
    uint64_t tick = 0;
    for (; tick < 400; ++tick) {
        if (pair.server.phase() == SessionPhase::Play && tick % 3 == 0) {
            for (int i = 0; i < 4; ++i) {
                int gx = rng.range(32) - 16;
                int gz = rng.range(32) - 16;
                int gy = rng.range(14);
                pair.edit(gx, gy, gz, palette[rng.range(4)]);
            }
        }
        pair.step(tick);
        pair.clearDirty();
    }
    // Editing stops; the link is given time to finish retransmitting the tail.
    for (; tick < 600; ++tick) {
        pair.step(tick);
        pair.clearDirty();
    }

    CHECK(pair.server.phase() == SessionPhase::Play);
    CHECK(pair.client.phase() == SessionPhase::Play);
    CHECK(pair.link.droppedCount() > 50u);
    CHECK(pair.serverTransport.stats().retransmitted > 0u);

    std::vector<Vec3i> coords = sortedCoords(pair.server.view().sent);
    CHECK_EQ(coords.size(), static_cast<std::size_t>(13));
    CHECK_EQ(worldDigest(pair.serverWorld, coords), worldDigest(pair.clientWorld, coords));

    // A digest that ignores content would pass anything: break one block and it
    // must disagree.
    pair.clientWorld.setBlock(0, 0, 0, Block::Bedrock);
    CHECK(worldDigest(pair.serverWorld, coords) != worldDigest(pair.clientWorld, coords));
}

VF_TEST(entityPositionsRideTheUnreliableChannelAndOnlyTheNewestSurvives) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    quality.seed = 0x77777777ull;
    TransportConfig transportConfig;
    Pair pair(quality, transportConfig, quietConfig());

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 10; ++tick) pair.step(tick);
    CHECK(pair.client.phase() == SessionPhase::Play);

    for (uint64_t tick = 10; tick < 60; ++tick) {
        float progress = static_cast<float>(tick - 10);
        pair.server.sendEntityMove(42, Vec3{progress, 64.0f, -progress}, 0.5f, -0.25f, true);
        pair.step(tick);
    }
    for (uint64_t tick = 60; tick < 70; ++tick) pair.step(tick);

    EntityMove seen;
    CHECK(pair.client.entityMove(42, seen));
    CHECK_NEAR(seen.x, 49.0f, 0.001f);
    CHECK_NEAR(seen.z, -49.0f, 0.001f);
    CHECK_NEAR(seen.yaw, 0.5f, 0.001f);
    CHECK(seen.onGround);
    CHECK_EQ(pair.client.trackedEntityCount(), static_cast<std::size_t>(1));
    CHECK_EQ(pair.server.stats().entityMovesSent, static_cast<uint64_t>(50));
    CHECK_EQ(pair.serverTransport.stats().retransmitted, static_cast<uint64_t>(0));

    EntityMove missing;
    CHECK(!pair.client.entityMove(43, missing));
}

VF_TEST(aPlayerStateReportMovesTheServerSideViewAndStreamsTheNewChunks) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    TransportConfig transportConfig;
    SessionConfig config = quietConfig();
    config.viewRadius = 1;
    config.chunksPerTick = 4;
    Pair pair(quality, transportConfig, config);
    buildTerrain(pair.serverWorld, 4);

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 30; ++tick) pair.step(tick);
    CHECK(pair.server.view().centreChunk == Vec3i(0, 0, 0));
    // Radius 1 is a 7-slot sphere; five of those slots sit in the y = 0 plane.
    std::size_t nearOrigin = pair.server.view().sent.size();
    CHECK_EQ(nearOrigin, static_cast<std::size_t>(5));

    PlayerState moved;
    moved.x = 40.0f;
    moved.y = 8.0f;
    moved.z = 40.0f;
    moved.health = 20.0f;
    pair.client.sendPlayerState(moved);
    for (uint64_t tick = 30; tick < 70; ++tick) pair.step(tick);

    CHECK(pair.server.view().centreChunk == Vec3i(2, 0, 2));
    CHECK(pair.server.view().hasChunk(Vec3i{2, 0, 2}));
    CHECK(!pair.server.view().hasChunk(Vec3i{0, 0, 0}));
    CHECK(!pair.server.view().tracker.contains(Vec3i{0, 0, 0}));
    CHECK_NEAR(pair.server.view().position.x, 40.0f, 0.001f);
}

VF_TEST(aClientBlockEditTravelsUpstreamAndComesBackAsAConfirmedChange) {
    LinkQuality quality;
    quality.lossPercent = 20;
    quality.latencyTicks = 1;
    quality.seed = 0x31415926ull;
    TransportConfig transportConfig;
    transportConfig.retransmitTimeoutTicks = 6;
    Pair pair(quality, transportConfig, quietConfig());
    buildTerrain(pair.serverWorld, 1);

    pair.client.beginHandshake("vulp", 1, 2);
    for (uint64_t tick = 0; tick < 40; ++tick) pair.step(tick);
    CHECK(pair.server.view().hasChunk(Vec3i{0, 0, 0}));
    CHECK(pair.clientWorld.blockAt(3, 15, 4) == Block::Air);

    pair.client.requestBlockEdit(3, 15, 4, Block::Glowstone);

    for (uint64_t tick = 40; tick < 90; ++tick) {
        pair.server.receive(tick);
        for (const BlockChange& edit : pair.server.requestedEdits())
            pair.edit(edit.x, edit.y, edit.z, edit.block);
        pair.server.clearRequestedEdits();
        pair.server.send(tick);
        pair.clearDirty();
        pair.client.receive(tick);
        pair.client.send(tick);
    }

    CHECK(pair.serverWorld.blockAt(3, 15, 4) == Block::Glowstone);
    CHECK(pair.clientWorld.blockAt(3, 15, 4) == Block::Glowstone);
    CHECK_EQ(pair.client.stats().blockChangesApplied, static_cast<uint64_t>(1));
}

VF_TEST(viewTrackerReportsOnlyTheDifferenceWhenTheCentreMoves) {
    ViewTracker tracker(2);
    std::vector<Vec3i> entered;
    std::vector<Vec3i> left;

    tracker.update(Vec3i{0, 0, 0}, entered, left);
    CHECK_EQ(entered.size(), static_cast<std::size_t>(33));
    CHECK_EQ(left.size(), static_cast<std::size_t>(0));
    CHECK(entered[0] == Vec3i(0, 0, 0));
    CHECK(tracker.contains(Vec3i{0, 0, 2}));
    CHECK(!tracker.contains(Vec3i{2, 2, 0}));
    CHECK_EQ(tracker.size(), static_cast<std::size_t>(33));

    std::size_t previous = tracker.size();
    tracker.update(Vec3i{1, 0, 0}, entered, left);
    CHECK(entered.size() < previous);
    CHECK(entered.size() > 0u);
    CHECK_EQ(entered.size(), left.size());
    CHECK_EQ(tracker.size(), previous);
    for (const Vec3i& coord : entered) CHECK(tracker.contains(coord));
    for (const Vec3i& coord : left) CHECK(!tracker.contains(coord));

    tracker.update(Vec3i{40, 0, 40}, entered, left);
    CHECK_EQ(entered.size(), static_cast<std::size_t>(33));
    CHECK_EQ(left.size(), static_cast<std::size_t>(33));
}
