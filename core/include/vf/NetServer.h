#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "NetSession.h"

namespace vf {

struct ServerConfig {
    uint32_t maxClients = 8;
    int viewRadius = 2;
    uint32_t chunksPerClientPerTick = 2;
    std::size_t bytesPerClientPerTick = 0;
    uint32_t keepAliveIntervalTicks = 20;
    uint32_t keepAliveTimeoutTicks = 100;
    std::size_t multiBlockThreshold = 4;
    int32_t protocolVersion = 47;
    uint64_t seedHash = 0;
    std::string levelType = "default";
    TransportConfig transport;
    LinkQuality link;
};

struct ServerStats {
    uint32_t connectedClients = 0;
    uint64_t bytesSentThisTick = 0;
    uint64_t totalBytesSent = 0;
    uint64_t chunksSent = 0;
    uint64_t blockUpdatesSent = 0;
    uint64_t queuedPackets = 0;
    uint64_t queuedBytes = 0;
    uint64_t acceptedConnections = 0;
    uint64_t rejectedConnections = 0;
    uint64_t timeouts = 0;
    uint64_t editsApplied = 0;
};

// One connected player: its own link, its own transport, its own session. The
// slot outlives the connection so the index handed out at accept stays valid
// until the slot is reused.
struct ClientSlot {
    bool active = false;
    int32_t entityId = 0;
    uint64_t lastServicedTick = 0;
    uint64_t lastBytesFlushed = 0;
    uint64_t lastChunksSent = 0;
    uint64_t lastBlockUpdates = 0;
    std::unique_ptr<LoopbackLink> link;
    std::unique_ptr<Transport> transport;
    std::unique_ptr<NetSession> session;
};

// Many clients over one authoritative world. Every block edit is applied once
// and then offered to each session, which decides on its own whether that
// client is close enough to care.
class NetServer {
public:
    NetServer(World& world, ServerConfig config = {});

    // The slot index, or -1 when every slot is taken.
    int acceptClient();
    void dropClient(int slot);

    LoopbackLink* linkFor(int slot);
    NetSession* sessionFor(int slot);
    const ClientSlot& slotAt(int slot) const { return slots[static_cast<std::size_t>(slot)]; }
    std::size_t slotCount() const { return slots.size(); }
    uint32_t activeClients() const;
    bool isActive(int slot) const;

    // Authoritative edit: the world, the dirty feed and every session's work
    // list move together, so no client can be told about a block the world
    // does not already have.
    void setBlock(int gx, int gy, int gz, Block block);
    void moveEntity(int32_t entityId, const Vec3& position, float yaw, float pitch, bool onGround);

    void update(uint64_t tick);

    World& world() { return *worldRef; }
    ServerChunkSync& chunkSync() { return sync; }
    const ServerConfig& settings() const { return config; }
    const ServerStats& stats() const { return statistics; }

private:
    void applyEdit(const BlockChange& change);
    void collectStats();
    void reap();

    World* worldRef;
    ServerConfig config;
    ServerChunkSync sync;
    std::vector<ClientSlot> slots;
    ServerStats statistics;
};

} // namespace vf
