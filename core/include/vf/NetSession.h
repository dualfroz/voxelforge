#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ChunkSync.h"
#include "Math.h"
#include "Protocol.h"
#include "Transport.h"
#include "World.h"

namespace vf {

enum class SessionPhase : uint8_t {
    Handshake = 0,
    Login = 1,
    Play = 2,
    Disconnected = 3,
};

// Which chunks belong around a centre, and what changed the last time that
// centre moved. The diff is what the server acts on: a client entering a chunk
// costs a snapshot, a client leaving it costs nothing but a forget.
class ViewTracker {
public:
    explicit ViewTracker(int radius = 2);

    void setRadius(int radius);
    int radius() const { return viewRadius; }

    // `entered` comes back nearest first, so the chunk under the player is
    // streamed before the rim of the sphere.
    void update(const Vec3i& newCentre, std::vector<Vec3i>& entered, std::vector<Vec3i>& left);

    bool contains(const Vec3i& coord) const { return resident.count(coord) != 0; }
    std::size_t size() const { return resident.size(); }
    const std::unordered_set<Vec3i, Vec3iHash>& chunks() const { return resident; }
    bool hasCentre() const { return centred; }
    const Vec3i& centre() const { return centreChunk; }
    void clear();

private:
    int viewRadius;
    Vec3i centreChunk;
    bool centred = false;
    std::unordered_set<Vec3i, Vec3iHash> resident;
};

// The server's picture of one client: where it is, which chunks it should
// hold, which it has actually been sent, and which are still waiting to go out.
struct ClientView {
    Vec3 position;
    Vec3i centreChunk;
    ViewTracker tracker;
    std::deque<Vec3i> pending;
    std::unordered_set<Vec3i, Vec3iHash> queued;
    std::unordered_set<Vec3i, Vec3iHash> sent;

    explicit ClientView(int radius = 2) : tracker(radius) {}

    bool hasChunk(const Vec3i& coord) const { return sent.count(coord) != 0; }
    void queueChunk(const Vec3i& coord);
    bool takeNextPending(Vec3i& out);
    void forget(const Vec3i& coord);
    std::size_t pendingCount() const { return queued.size(); }
    void reset();
};

struct SessionConfig {
    int32_t protocolVersion = 47;
    int viewRadius = 2;
    uint32_t chunksPerTick = 2;
    uint32_t keepAliveIntervalTicks = 20;
    uint32_t keepAliveTimeoutTicks = 100;
    std::size_t multiBlockThreshold = 4;
    std::size_t bandwidthBytesPerTick = 0;
    uint8_t maxPlayers = 8;
    uint8_t gameMode = 0;
    int32_t dimension = 0;
    uint64_t seedHash = 0;
    std::string levelType = "default";
};

struct SessionStats {
    uint64_t chunksSent = 0;
    uint64_t blockChangesSent = 0;
    uint64_t multiBlockChangesSent = 0;
    uint64_t entityMovesSent = 0;
    uint64_t keepAlivesSent = 0;
    uint64_t packetsFlushed = 0;
    uint64_t packetsReceived = 0;
    uint64_t packetsRejected = 0;
    uint64_t chunksApplied = 0;
    uint64_t blockChangesApplied = 0;
    uint64_t multiBlockChangesApplied = 0;
    uint64_t entityMovesApplied = 0;
    uint64_t bytesQueued = 0;
    uint64_t bytesFlushed = 0;
};

// One connection, seen from either end. The phase machine gates which packets
// are legal; anything out of place ends the session with a reason instead of
// being fed to a decoder that was never meant to see it.
class NetSession {
public:
    enum class Role : uint8_t { Client = 0, Server = 1 };

    NetSession(Transport& transport, Role role, SessionConfig config = {});

    void attachWorld(World& world) { worldRef = &world; }
    void attachChunkSync(ServerChunkSync& sync) { syncRef = &sync; }
    void setEntityId(int32_t id) { ownEntityId = id; }

    // Client side: opens the handshake and moves straight into login.
    void beginHandshake(const std::string& playerName, uint64_t uuidHi, uint64_t uuidLo);

    void receive(uint64_t tick);
    void send(uint64_t tick);
    void update(uint64_t tick);

    SessionPhase phase() const { return currentPhase; }
    bool timedOut() const { return keepAliveExpired; }
    const std::string& disconnectReason() const { return reason; }
    void disconnect(const std::string& why);

    ClientView& view() { return clientView; }
    const ClientView& view() const { return clientView; }

    // Server side: this chunk has entries in the shared ServerChunkSync that
    // still have to be turned into packets for this particular client.
    void markChunkDirty(const Vec3i& coord);
    void sendEntityMove(int32_t movedEntityId, const Vec3& position, float yaw, float pitch,
                        bool onGround);
    void sendPlayerState(const PlayerState& state);
    const std::vector<BlockChange>& requestedEdits() const { return edits; }
    void clearRequestedEdits() { edits.clear(); }
    const std::string& playerName() const { return name; }
    uint64_t uuidHigh() const { return uuidHi; }
    uint64_t uuidLow() const { return uuidLo; }

    // Client side.
    void requestBlockEdit(int32_t x, int32_t y, int32_t z, Block block);
    const PlayerState& playerState() const { return latestPlayerState; }
    bool entityMove(int32_t movedEntityId, EntityMove& out) const;
    std::size_t trackedEntityCount() const { return entities.size(); }
    int32_t entityId() const { return ownEntityId; }

    const SessionStats& stats() const { return statistics; }
    std::size_t queuedPacketCount() const { return outbound.size(); }
    std::size_t queuedByteCount() const;
    uint64_t lastKeepAliveTick() const { return lastKeepAliveHeard; }
    Role side() const { return role; }

private:
    struct QueuedPacket {
        ByteBuffer frame;
        bool reliable = true;
    };

    void enqueue(PacketId id, const PacketWriter& body, bool reliable);
    void handleMessage(const ByteBuffer& message, uint64_t tick);
    void handleFrame(const Frame& frame, uint64_t tick);
    bool legalHere(PacketId id) const;
    void enterPlay(uint64_t tick);
    void recentre(const Vec3& position);
    void streamChunks();
    void flushBlockChanges();
    void serviceKeepAlive(uint64_t tick);
    void flushOutbound();

    Transport* transport;
    Role role;
    SessionConfig config;

    SessionPhase currentPhase = SessionPhase::Handshake;
    std::string reason;
    bool keepAliveExpired = false;
    bool handshakeStarted = false;

    World* worldRef = nullptr;
    ServerChunkSync* syncRef = nullptr;
    ClientChunkSync clientSync;

    std::string name;
    uint64_t uuidHi = 0;
    uint64_t uuidLo = 0;
    int32_t ownEntityId = 0;

    ClientView clientView;
    std::vector<Vec3i> dirtyChunks;
    std::unordered_set<Vec3i, Vec3iHash> dirtyLookup;
    std::vector<BlockChange> edits;
    std::unordered_map<int32_t, EntityMove> entities;
    PlayerState latestPlayerState;

    std::deque<QueuedPacket> outbound;
    uint64_t lastKeepAliveSent = 0;
    uint64_t lastKeepAliveHeard = 0;
    int64_t pendingKeepAliveToken = 0;
    int64_t acceptedKeepAliveToken = -1;

    SessionStats statistics;
};

} // namespace vf
