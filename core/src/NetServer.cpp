#include "vf/NetServer.h"

#include <cmath>

namespace vf {

namespace {

Vec3i chunkOfPosition(const Vec3& position) {
    int gx = static_cast<int>(std::floor(position.x));
    int gy = static_cast<int>(std::floor(position.y));
    int gz = static_cast<int>(std::floor(position.z));
    return Vec3i{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
}

}  // namespace

NetServer::NetServer(World& world, ServerConfig config) : worldRef(&world), config(config) {
    if (this->config.maxClients == 0) this->config.maxClients = 1;
    slots.resize(this->config.maxClients);
}

uint32_t NetServer::activeClients() const {
    uint32_t count = 0;
    for (const ClientSlot& slot : slots)
        if (slot.active) ++count;
    return count;
}

bool NetServer::isActive(int slot) const {
    if (slot < 0 || static_cast<std::size_t>(slot) >= slots.size()) return false;
    return slots[static_cast<std::size_t>(slot)].active;
}

int NetServer::acceptClient() {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        ClientSlot& slot = slots[i];
        if (slot.active) continue;

        SessionConfig sessionConfig;
        sessionConfig.protocolVersion = config.protocolVersion;
        sessionConfig.viewRadius = config.viewRadius;
        sessionConfig.chunksPerTick = config.chunksPerClientPerTick;
        sessionConfig.keepAliveIntervalTicks = config.keepAliveIntervalTicks;
        sessionConfig.keepAliveTimeoutTicks = config.keepAliveTimeoutTicks;
        sessionConfig.multiBlockThreshold = config.multiBlockThreshold;
        sessionConfig.bandwidthBytesPerTick = config.bytesPerClientPerTick;
        sessionConfig.maxPlayers = static_cast<uint8_t>(config.maxClients);
        sessionConfig.seedHash = config.seedHash;
        sessionConfig.levelType = config.levelType;

        slot.active = true;
        slot.entityId = static_cast<int32_t>(i) + 1;
        slot.lastBytesFlushed = 0;
        slot.lastChunksSent = 0;
        slot.lastBlockUpdates = 0;
        slot.link.reset(new LoopbackLink(config.link));
        slot.transport.reset(new Transport(*slot.link, 0, config.transport));
        slot.session.reset(new NetSession(*slot.transport, NetSession::Role::Server, sessionConfig));
        slot.session->attachWorld(*worldRef);
        slot.session->attachChunkSync(sync);
        slot.session->setEntityId(slot.entityId);

        ++statistics.acceptedConnections;
        return static_cast<int>(i);
    }
    ++statistics.rejectedConnections;
    return -1;
}

void NetServer::dropClient(int slot) {
    if (!isActive(slot)) return;
    ClientSlot& entry = slots[static_cast<std::size_t>(slot)];
    entry.active = false;
    entry.session.reset();
    entry.transport.reset();
    entry.link.reset();
}

LoopbackLink* NetServer::linkFor(int slot) {
    if (!isActive(slot)) return nullptr;
    return slots[static_cast<std::size_t>(slot)].link.get();
}

NetSession* NetServer::sessionFor(int slot) {
    if (!isActive(slot)) return nullptr;
    return slots[static_cast<std::size_t>(slot)].session.get();
}

void NetServer::applyEdit(const BlockChange& change) {
    worldRef->setBlock(change.x, change.y, change.z, change.block);
    Vec3i coord{floorDiv(change.x, chunkSize), floorDiv(change.y, chunkSize),
                floorDiv(change.z, chunkSize)};
    sync.markDirty(coord, floorMod(change.x, chunkSize), floorMod(change.y, chunkSize),
                   floorMod(change.z, chunkSize));
    for (ClientSlot& slot : slots)
        if (slot.active) slot.session->markChunkDirty(coord);
    ++statistics.editsApplied;
}

void NetServer::setBlock(int gx, int gy, int gz, Block block) {
    BlockChange change;
    change.x = gx;
    change.y = gy;
    change.z = gz;
    change.block = block;
    applyEdit(change);
}

void NetServer::moveEntity(int32_t entityId, const Vec3& position, float yaw, float pitch,
                           bool onGround) {
    Vec3i coord = chunkOfPosition(position);
    for (ClientSlot& slot : slots) {
        if (!slot.active) continue;
        if (slot.session->phase() != SessionPhase::Play) continue;
        // An entity outside the client's view costs nothing to skip and cannot
        // be drawn there anyway.
        if (!slot.session->view().tracker.contains(coord)) continue;
        slot.session->sendEntityMove(entityId, position, yaw, pitch, onGround);
    }
}

void NetServer::collectStats() {
    statistics.bytesSentThisTick = 0;
    statistics.queuedPackets = 0;
    statistics.queuedBytes = 0;
    for (ClientSlot& slot : slots) {
        if (!slot.active) continue;
        const SessionStats& sessionStats = slot.session->stats();

        uint64_t bytes = sessionStats.bytesFlushed - slot.lastBytesFlushed;
        slot.lastBytesFlushed = sessionStats.bytesFlushed;
        statistics.bytesSentThisTick += bytes;
        statistics.totalBytesSent += bytes;

        uint64_t chunks = sessionStats.chunksSent - slot.lastChunksSent;
        slot.lastChunksSent = sessionStats.chunksSent;
        statistics.chunksSent += chunks;

        uint64_t updates = sessionStats.blockChangesSent + sessionStats.multiBlockChangesSent;
        statistics.blockUpdatesSent += updates - slot.lastBlockUpdates;
        slot.lastBlockUpdates = updates;

        statistics.queuedPackets += slot.session->queuedPacketCount();
        statistics.queuedBytes += slot.session->queuedByteCount();
    }
}

void NetServer::reap() {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        ClientSlot& slot = slots[i];
        if (!slot.active) continue;
        if (slot.session->phase() != SessionPhase::Disconnected) continue;
        if (slot.session->timedOut()) ++statistics.timeouts;
        dropClient(static_cast<int>(i));
    }
}

void NetServer::update(uint64_t tick) {
    for (ClientSlot& slot : slots) {
        if (!slot.active) continue;
        slot.session->receive(tick);
        slot.lastServicedTick = tick;
    }

    // Upstream edits land in the world before any session builds its outgoing
    // packets, so every client sees the same authoritative state this tick.
    for (ClientSlot& slot : slots) {
        if (!slot.active) continue;
        for (const BlockChange& change : slot.session->requestedEdits()) applyEdit(change);
        slot.session->clearRequestedEdits();
    }

    for (ClientSlot& slot : slots) {
        if (!slot.active) continue;
        slot.session->send(tick);
    }

    sync.clearAll();
    collectStats();
    reap();
    statistics.connectedClients = activeClients();
}

} // namespace vf
