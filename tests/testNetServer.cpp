#include "testHarness.h"
#include "vf/NetServer.h"

#include <cstdint>
#include <memory>
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

void buildTerrain(World& world, int chunkMin, int chunkMax) {
    Lcg rng(0x1a2b3c4d);
    const Block palette[4] = {Block::Stone, Block::Dirt, Block::Grass, Block::Sand};
    for (int cx = chunkMin; cx <= chunkMax; ++cx) {
        for (int cz = chunkMin; cz <= chunkMax; ++cz) {
            Chunk& chunk = world.ensureChunk(Vec3i{cx, 0, cz});
            for (int x = 0; x < chunkSize; ++x)
                for (int z = 0; z < chunkSize; ++z)
                    for (int y = 0; y < 6; ++y) chunk.set(x, y, z, palette[rng.range(4)]);
        }
    }
}

// The far end of one slot: an independent transport and session on endpoint 1
// of that slot's link, which is exactly what a real remote player would be.
struct RemoteClient {
    World world;
    std::unique_ptr<Transport> transport;
    std::unique_ptr<NetSession> session;

    RemoteClient(NetServer& server, int slot, const TransportConfig& transportConfig,
                 const SessionConfig& sessionConfig, const std::string& name) {
        transport.reset(new Transport(*server.linkFor(slot), 1, transportConfig));
        session.reset(new NetSession(*transport, NetSession::Role::Client, sessionConfig));
        session->attachWorld(world);
        session->beginHandshake(name, 0x1000 + static_cast<uint64_t>(slot), 0x2000);
    }

    void step(uint64_t tick) {
        session->receive(tick);
        session->send(tick);
    }
};

SessionConfig clientConfigFor(const ServerConfig& config) {
    SessionConfig sessionConfig;
    sessionConfig.protocolVersion = config.protocolVersion;
    sessionConfig.viewRadius = config.viewRadius;
    sessionConfig.keepAliveIntervalTicks = config.keepAliveIntervalTicks;
    sessionConfig.keepAliveTimeoutTicks = config.keepAliveTimeoutTicks;
    return sessionConfig;
}

ServerConfig quietServerConfig() {
    ServerConfig config;
    config.maxClients = 4;
    config.viewRadius = 1;
    config.chunksPerClientPerTick = 4;
    config.keepAliveIntervalTicks = 10;
    config.keepAliveTimeoutTicks = 1000000;
    config.link.latencyTicks = 1;
    return config;
}

// Seeds a slot's view before login; the first snapshot burst then goes out
// around this spot instead of around the origin.
void placeAt(NetServer& server, int slot, float x, float y, float z) {
    server.sessionFor(slot)->view().position = Vec3{x, y, z};
}

}  // namespace

VF_TEST(twoClientsStandingApartReceiveDisjointSetsOfChunks) {
    ServerConfig config = quietServerConfig();
    World world;
    NetServer server(world, config);
    buildTerrain(world, -6, 6);

    int slotNear = server.acceptClient();
    int slotFar = server.acceptClient();
    CHECK_EQ(slotNear, 0);
    CHECK_EQ(slotFar, 1);
    placeAt(server, slotNear, 8.0f, 8.0f, 8.0f);
    placeAt(server, slotFar, 72.0f, 8.0f, 72.0f);

    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    RemoteClient near(server, slotNear, transportConfig, sessionConfig, "near");
    RemoteClient far(server, slotFar, transportConfig, sessionConfig, "far");

    for (uint64_t tick = 0; tick < 60; ++tick) {
        server.update(tick);
        near.step(tick);
        far.step(tick);
    }

    CHECK(near.session->phase() == SessionPhase::Play);
    CHECK(far.session->phase() == SessionPhase::Play);

    const ClientView& nearView = server.sessionFor(slotNear)->view();
    const ClientView& farView = server.sessionFor(slotFar)->view();
    CHECK_EQ(nearView.sent.size(), static_cast<std::size_t>(5));
    CHECK_EQ(farView.sent.size(), static_cast<std::size_t>(5));

    std::size_t shared = 0;
    for (const Vec3i& coord : nearView.sent)
        if (farView.sent.count(coord) != 0) ++shared;
    CHECK_EQ(shared, static_cast<std::size_t>(0));

    CHECK(nearView.hasChunk(Vec3i{0, 0, 0}));
    CHECK(farView.hasChunk(Vec3i{4, 0, 4}));
    CHECK(!nearView.hasChunk(Vec3i{4, 0, 4}));
    CHECK(!farView.hasChunk(Vec3i{0, 0, 0}));

    CHECK_EQ(near.world.chunkCount(), static_cast<std::size_t>(5));
    CHECK_EQ(far.world.chunkCount(), static_cast<std::size_t>(5));
    CHECK(near.world.chunkAt(Vec3i{4, 0, 4}) == nullptr);
    CHECK(far.world.chunkAt(Vec3i{0, 0, 0}) == nullptr);
    CHECK_EQ(server.stats().chunksSent, static_cast<uint64_t>(10));
}

VF_TEST(aBlockChangeInsideBothViewsReachesEachClientExactlyOnce) {
    ServerConfig config = quietServerConfig();
    config.viewRadius = 2;
    World world;
    NetServer server(world, config);
    buildTerrain(world, -4, 4);

    int slotLeft = server.acceptClient();
    int slotRight = server.acceptClient();
    // Both stand two chunks apart, so chunk (1,0,0) is inside both spheres.
    placeAt(server, slotLeft, 8.0f, 8.0f, 8.0f);
    placeAt(server, slotRight, 40.0f, 8.0f, 8.0f);

    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    sessionConfig.viewRadius = 2;
    RemoteClient left(server, slotLeft, transportConfig, sessionConfig, "left");
    RemoteClient right(server, slotRight, transportConfig, sessionConfig, "right");

    for (uint64_t tick = 0; tick < 80; ++tick) {
        server.update(tick);
        left.step(tick);
        right.step(tick);
    }
    CHECK(server.sessionFor(slotLeft)->view().hasChunk(Vec3i{1, 0, 0}));
    CHECK(server.sessionFor(slotRight)->view().hasChunk(Vec3i{1, 0, 0}));

    server.setBlock(20, 3, 5, Block::Glowstone);
    for (uint64_t tick = 80; tick < 120; ++tick) {
        server.update(tick);
        left.step(tick);
        right.step(tick);
    }

    CHECK(left.world.blockAt(20, 3, 5) == Block::Glowstone);
    CHECK(right.world.blockAt(20, 3, 5) == Block::Glowstone);
    CHECK_EQ(left.session->stats().blockChangesApplied, static_cast<uint64_t>(1));
    CHECK_EQ(right.session->stats().blockChangesApplied, static_cast<uint64_t>(1));
    CHECK_EQ(server.stats().blockUpdatesSent, static_cast<uint64_t>(2));
    CHECK_EQ(server.stats().editsApplied, static_cast<uint64_t>(1));
}

VF_TEST(aBlockChangeIsNeverSentToAClientThatDoesNotHoldThatChunk) {
    ServerConfig config = quietServerConfig();
    World world;
    NetServer server(world, config);
    buildTerrain(world, -6, 6);

    int slotNear = server.acceptClient();
    int slotFar = server.acceptClient();
    placeAt(server, slotNear, 8.0f, 8.0f, 8.0f);
    placeAt(server, slotFar, 72.0f, 8.0f, 72.0f);

    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    RemoteClient near(server, slotNear, transportConfig, sessionConfig, "near");
    RemoteClient far(server, slotFar, transportConfig, sessionConfig, "far");
    for (uint64_t tick = 0; tick < 60; ++tick) {
        server.update(tick);
        near.step(tick);
        far.step(tick);
    }

    for (int i = 0; i < 3; ++i) server.setBlock(i, 3, 0, Block::Obsidian);
    for (uint64_t tick = 60; tick < 100; ++tick) {
        server.update(tick);
        near.step(tick);
        far.step(tick);
    }

    for (int i = 0; i < 3; ++i) CHECK(near.world.blockAt(i, 3, 0) == Block::Obsidian);
    CHECK_EQ(near.session->stats().blockChangesApplied, static_cast<uint64_t>(3));
    CHECK_EQ(far.session->stats().blockChangesApplied, static_cast<uint64_t>(0));
    CHECK_EQ(far.session->stats().multiBlockChangesApplied, static_cast<uint64_t>(0));
    CHECK_EQ(far.world.chunkAt(Vec3i{0, 0, 0}), nullptr);
    CHECK_EQ(server.stats().blockUpdatesSent, static_cast<uint64_t>(3));
}

VF_TEST(anEditFromOneClientIsBroadcastToTheOtherThroughTheAuthoritativeWorld) {
    ServerConfig config = quietServerConfig();
    World world;
    NetServer server(world, config);
    buildTerrain(world, -2, 2);

    int slotA = server.acceptClient();
    int slotB = server.acceptClient();
    placeAt(server, slotA, 8.0f, 8.0f, 8.0f);
    placeAt(server, slotB, 8.0f, 8.0f, 8.0f);

    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    RemoteClient builder(server, slotA, transportConfig, sessionConfig, "builder");
    RemoteClient watcher(server, slotB, transportConfig, sessionConfig, "watcher");
    for (uint64_t tick = 0; tick < 40; ++tick) {
        server.update(tick);
        builder.step(tick);
        watcher.step(tick);
    }

    builder.session->requestBlockEdit(4, 9, 4, Block::Glowstone);
    for (uint64_t tick = 40; tick < 80; ++tick) {
        server.update(tick);
        builder.step(tick);
        watcher.step(tick);
    }

    CHECK(world.blockAt(4, 9, 4) == Block::Glowstone);
    CHECK(builder.world.blockAt(4, 9, 4) == Block::Glowstone);
    CHECK(watcher.world.blockAt(4, 9, 4) == Block::Glowstone);
    CHECK_EQ(server.stats().editsApplied, static_cast<uint64_t>(1));
    CHECK_EQ(watcher.session->stats().blockChangesApplied, static_cast<uint64_t>(1));
}

VF_TEST(theBandwidthBudgetDelaysPacketsAcrossTicksWithoutLosingAnyOfThem) {
    ServerConfig config = quietServerConfig();
    config.viewRadius = 1;
    config.chunksPerClientPerTick = 4;
    config.bytesPerClientPerTick = 220;
    World world;
    NetServer server(world, config);
    buildTerrain(world, -3, 3);

    int slot = server.acceptClient();
    placeAt(server, slot, 8.0f, 8.0f, 8.0f);
    TransportConfig transportConfig = config.transport;
    RemoteClient only(server, slot, transportConfig, clientConfigFor(config), "budgeted");

    bool everQueued = false;
    uint64_t peakQueuedBytes = 0;
    for (uint64_t tick = 0; tick < 40; ++tick) {
        server.update(tick);
        only.step(tick);
        if (server.stats().queuedPackets > 0) everQueued = true;
        if (server.stats().queuedBytes > peakQueuedBytes)
            peakQueuedBytes = server.stats().queuedBytes;
        // A single oversized packet is allowed through on its own, but the
        // budget must never be exceeded by a whole extra packet.
        CHECK(server.stats().bytesSentThisTick <= 220u + 4096u);
    }
    CHECK(everQueued);
    CHECK(peakQueuedBytes > 220u);

    for (uint64_t tick = 40; tick < 400; ++tick) {
        server.update(tick);
        only.step(tick);
    }
    CHECK_EQ(server.stats().queuedPackets, static_cast<uint64_t>(0));
    CHECK_EQ(only.world.chunkCount(), static_cast<std::size_t>(5));
    CHECK_EQ(only.session->stats().chunksApplied, static_cast<uint64_t>(5));

    // Nothing was dropped on the way: the client sees the same blocks.
    for (int x = 0; x < chunkSize; ++x)
        for (int z = 0; z < chunkSize; ++z)
            CHECK(only.world.blockAt(x, 3, z) == world.blockAt(x, 3, z));
}

VF_TEST(anUnbudgetedServerSendsTheSameChunksInFarFewerTicks) {
    ServerConfig budgeted = quietServerConfig();
    budgeted.viewRadius = 1;
    budgeted.chunksPerClientPerTick = 4;
    budgeted.bytesPerClientPerTick = 220;

    ServerConfig open = budgeted;
    open.bytesPerClientPerTick = 0;

    uint64_t ticksNeeded[2] = {0, 0};
    for (int variant = 0; variant < 2; ++variant) {
        const ServerConfig& config = variant == 0 ? budgeted : open;
        World world;
        NetServer server(world, config);
        buildTerrain(world, -3, 3);
        int slot = server.acceptClient();
        placeAt(server, slot, 8.0f, 8.0f, 8.0f);
        TransportConfig transportConfig = config.transport;
        RemoteClient only(server, slot, transportConfig, clientConfigFor(config), "probe");

        uint64_t tick = 0;
        for (; tick < 500; ++tick) {
            server.update(tick);
            only.step(tick);
            if (only.world.chunkCount() == 5) break;
        }
        ticksNeeded[variant] = tick;
        CHECK_EQ(only.world.chunkCount(), static_cast<std::size_t>(5));
    }
    CHECK(ticksNeeded[0] > ticksNeeded[1]);
    CHECK(ticksNeeded[1] < 20u);
}

VF_TEST(theServerRefusesConnectionsOnceEverySlotIsTaken) {
    ServerConfig config = quietServerConfig();
    config.maxClients = 2;
    World world;
    NetServer server(world, config);

    CHECK_EQ(server.slotCount(), static_cast<std::size_t>(2));
    CHECK_EQ(server.acceptClient(), 0);
    CHECK_EQ(server.acceptClient(), 1);
    CHECK_EQ(server.acceptClient(), -1);
    CHECK_EQ(server.acceptClient(), -1);
    CHECK_EQ(server.stats().rejectedConnections, static_cast<uint64_t>(2));
    CHECK_EQ(server.stats().acceptedConnections, static_cast<uint64_t>(2));
    CHECK_EQ(server.activeClients(), 2u);
    CHECK_EQ(server.linkFor(2), nullptr);
    CHECK_EQ(server.sessionFor(-1), nullptr);

    server.dropClient(0);
    CHECK_EQ(server.activeClients(), 1u);
    CHECK_EQ(server.acceptClient(), 0);
    CHECK_EQ(server.activeClients(), 2u);
}

VF_TEST(aSilentClientTimesOutAndItsSlotComesBackForReuse) {
    ServerConfig config = quietServerConfig();
    config.maxClients = 1;
    config.keepAliveIntervalTicks = 5;
    config.keepAliveTimeoutTicks = 40;
    World world;
    NetServer server(world, config);
    buildTerrain(world, -2, 2);

    int slot = server.acceptClient();
    TransportConfig transportConfig = config.transport;
    {
        RemoteClient leaving(server, slot, transportConfig, clientConfigFor(config), "leaving");
        for (uint64_t tick = 0; tick < 30; ++tick) {
            server.update(tick);
            leaving.step(tick);
        }
        CHECK(server.sessionFor(slot) != nullptr);
        CHECK(server.sessionFor(slot)->phase() == SessionPhase::Play);
    }

    for (uint64_t tick = 30; tick < 200; ++tick) server.update(tick);

    CHECK_EQ(server.activeClients(), 0u);
    CHECK_EQ(server.stats().timeouts, static_cast<uint64_t>(1));
    CHECK_EQ(server.stats().connectedClients, 0u);
    CHECK_EQ(server.acceptClient(), 0);
    CHECK_EQ(server.activeClients(), 1u);
}

VF_TEST(entityMovesGoOnlyToTheClientsWhoseViewHoldsTheEntity) {
    ServerConfig config = quietServerConfig();
    World world;
    NetServer server(world, config);
    buildTerrain(world, -6, 6);

    int slotNear = server.acceptClient();
    int slotFar = server.acceptClient();
    placeAt(server, slotNear, 8.0f, 8.0f, 8.0f);
    placeAt(server, slotFar, 72.0f, 8.0f, 72.0f);
    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    RemoteClient near(server, slotNear, transportConfig, sessionConfig, "near");
    RemoteClient far(server, slotFar, transportConfig, sessionConfig, "far");

    for (uint64_t tick = 0; tick < 40; ++tick) {
        server.update(tick);
        near.step(tick);
        far.step(tick);
    }

    for (uint64_t tick = 40; tick < 70; ++tick) {
        server.moveEntity(31, Vec3{9.0f, 5.0f, 9.0f}, 1.0f, 0.0f, true);
        server.update(tick);
        near.step(tick);
        far.step(tick);
    }

    EntityMove seen;
    CHECK(near.session->entityMove(31, seen));
    CHECK_NEAR(seen.x, 9.0f, 0.001f);
    CHECK_EQ(far.session->trackedEntityCount(), static_cast<std::size_t>(0));
    CHECK(far.session->stats().entityMovesApplied == 0u);
}

VF_TEST(serverStatsAccountForClientsBytesChunksAndTheOutgoingQueue) {
    ServerConfig config = quietServerConfig();
    config.viewRadius = 1;
    config.chunksPerClientPerTick = 1;
    World world;
    NetServer server(world, config);
    buildTerrain(world, -2, 2);

    CHECK_EQ(server.stats().connectedClients, 0u);
    int slot = server.acceptClient();
    placeAt(server, slot, 8.0f, 8.0f, 8.0f);
    TransportConfig transportConfig = config.transport;
    RemoteClient only(server, slot, transportConfig, clientConfigFor(config), "counted");

    uint64_t bytesSeenInAnyTick = 0;
    for (uint64_t tick = 0; tick < 40; ++tick) {
        server.update(tick);
        only.step(tick);
        bytesSeenInAnyTick += server.stats().bytesSentThisTick;
    }

    CHECK_EQ(server.stats().connectedClients, 1u);
    CHECK_EQ(server.stats().chunksSent, static_cast<uint64_t>(5));
    CHECK_EQ(server.stats().totalBytesSent, bytesSeenInAnyTick);
    CHECK(server.stats().totalBytesSent > 500u);
    CHECK_EQ(server.stats().queuedPackets, static_cast<uint64_t>(0));

    server.setBlock(1, 2, 3, Block::Ice);
    server.setBlock(2, 2, 3, Block::Ice);
    for (uint64_t tick = 40; tick < 60; ++tick) {
        server.update(tick);
        only.step(tick);
    }
    CHECK_EQ(server.stats().blockUpdatesSent, static_cast<uint64_t>(2));
    CHECK_EQ(server.stats().editsApplied, static_cast<uint64_t>(2));
    CHECK(only.world.blockAt(1, 2, 3) == Block::Ice);
}

VF_TEST(fourClientsSpreadAcrossTheWorldStayConsistentOverAnUnreliableLink) {
    ServerConfig config = quietServerConfig();
    config.maxClients = 4;
    config.viewRadius = 1;
    config.chunksPerClientPerTick = 2;
    config.link.lossPercent = 25;
    config.link.duplicatePercent = 10;
    config.link.reorderPercent = 20;
    config.link.latencyTicks = 2;
    config.link.seed = 0xc0ffee1234ull;
    config.transport.retransmitTimeoutTicks = 8;
    World world;
    NetServer server(world, config);
    buildTerrain(world, -8, 8);

    const float spots[4][2] = {{8.0f, 8.0f}, {72.0f, 8.0f}, {8.0f, 72.0f}, {72.0f, 72.0f}};
    std::vector<std::unique_ptr<RemoteClient>> clients;
    TransportConfig transportConfig = config.transport;
    SessionConfig sessionConfig = clientConfigFor(config);
    for (int i = 0; i < 4; ++i) {
        int slot = server.acceptClient();
        CHECK_EQ(slot, i);
        placeAt(server, slot, spots[i][0], 8.0f, spots[i][1]);
        clients.push_back(std::unique_ptr<RemoteClient>(
            new RemoteClient(server, slot, transportConfig, sessionConfig, "player")));
    }

    Lcg rng(0x9911aabb);
    for (uint64_t tick = 0; tick < 500; ++tick) {
        if (tick > 100 && tick % 5 == 0) {
            int corner = rng.range(4);
            int baseX = static_cast<int>(spots[corner][0]);
            int baseZ = static_cast<int>(spots[corner][1]);
            server.setBlock(baseX + rng.range(8) - 4, rng.range(6), baseZ + rng.range(8) - 4,
                            Block::Glowstone);
        }
        server.update(tick);
        for (auto& client : clients) client->step(tick);
    }
    for (uint64_t tick = 500; tick < 800; ++tick) {
        server.update(tick);
        for (auto& client : clients) client->step(tick);
    }

    CHECK_EQ(server.activeClients(), 4u);
    for (int i = 0; i < 4; ++i) {
        NetSession* session = server.sessionFor(i);
        CHECK(session->phase() == SessionPhase::Play);
        const ClientView& view = session->view();
        CHECK_EQ(view.sent.size(), static_cast<std::size_t>(5));
        for (const Vec3i& coord : view.sent) {
            const Chunk* authoritative = world.chunkAt(coord);
            const Chunk* mirror = clients[static_cast<std::size_t>(i)]->world.chunkAt(coord);
            CHECK(mirror != nullptr);
            if (mirror == nullptr) continue;
            CHECK(authoritative->raw() == mirror->raw());
        }
        // Each client only ever holds the chunks it was told about.
        CHECK_EQ(clients[static_cast<std::size_t>(i)]->world.chunkCount(),
                 static_cast<std::size_t>(5));
    }
    CHECK(server.stats().editsApplied > 50u);
}
