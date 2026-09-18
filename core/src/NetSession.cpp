#include "vf/NetSession.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

Vec3i chunkOf(const Vec3& position) {
    int gx = static_cast<int>(std::floor(position.x));
    int gy = static_cast<int>(std::floor(position.y));
    int gz = static_cast<int>(std::floor(position.z));
    return Vec3i{floorDiv(gx, chunkSize), floorDiv(gy, chunkSize), floorDiv(gz, chunkSize)};
}

int64_t squaredDistance(const Vec3i& a, const Vec3i& b) {
    int64_t dx = a.x - b.x;
    int64_t dy = a.y - b.y;
    int64_t dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

}

ViewTracker::ViewTracker(int radius) : viewRadius(radius < 0 ? 0 : radius) {}

void ViewTracker::setRadius(int radius) { viewRadius = radius < 0 ? 0 : radius; }

void ViewTracker::clear() {
    resident.clear();
    centred = false;
    centreChunk = Vec3i{0, 0, 0};
}

void ViewTracker::update(const Vec3i& newCentre, std::vector<Vec3i>& entered,
                         std::vector<Vec3i>& left) {
    entered.clear();
    left.clear();

    std::vector<Vec3i> wanted;
    int64_t limit = static_cast<int64_t>(viewRadius) * static_cast<int64_t>(viewRadius);
    for (int dy = -viewRadius; dy <= viewRadius; ++dy) {
        for (int dz = -viewRadius; dz <= viewRadius; ++dz) {
            for (int dx = -viewRadius; dx <= viewRadius; ++dx) {
                Vec3i coord{newCentre.x + dx, newCentre.y + dy, newCentre.z + dz};
                if (squaredDistance(coord, newCentre) > limit) continue;
                wanted.push_back(coord);
            }
        }
    }

    std::stable_sort(wanted.begin(), wanted.end(),
                     [&newCentre](const Vec3i& a, const Vec3i& b) {
                         int64_t da = squaredDistance(a, newCentre);
                         int64_t db = squaredDistance(b, newCentre);
                         if (da != db) return da < db;
                         if (a.y != b.y) return a.y < b.y;
                         if (a.z != b.z) return a.z < b.z;
                         return a.x < b.x;
                     });

    std::unordered_set<Vec3i, Vec3iHash> next;
    next.reserve(wanted.size() * 2);
    for (const Vec3i& coord : wanted) {
        next.insert(coord);
        if (resident.count(coord) == 0) entered.push_back(coord);
    }
    for (const Vec3i& coord : resident)
        if (next.count(coord) == 0) left.push_back(coord);

    std::stable_sort(left.begin(), left.end(), [](const Vec3i& a, const Vec3i& b) {
        if (a.y != b.y) return a.y < b.y;
        if (a.z != b.z) return a.z < b.z;
        return a.x < b.x;
    });

    resident.swap(next);
    centreChunk = newCentre;
    centred = true;
}

void ClientView::queueChunk(const Vec3i& coord) {
    if (!queued.insert(coord).second) return;
    pending.push_back(coord);
}

bool ClientView::takeNextPending(Vec3i& out) {
    while (!pending.empty()) {
        Vec3i coord = pending.front();
        pending.pop_front();
        if (queued.erase(coord) == 0) continue;
        out = coord;
        return true;
    }
    return false;
}

void ClientView::forget(const Vec3i& coord) {
    sent.erase(coord);
    queued.erase(coord);
}

void ClientView::reset() {
    tracker.clear();
    pending.clear();
    queued.clear();
    sent.clear();
    centreChunk = Vec3i{0, 0, 0};
    position = Vec3{0.0f, 0.0f, 0.0f};
}

NetSession::NetSession(Transport& transport, Role role, SessionConfig config)
    : transport(&transport), role(role), config(config), clientView(config.viewRadius) {}

std::size_t NetSession::queuedByteCount() const {
    std::size_t total = 0;
    for (const QueuedPacket& packet : outbound) total += packet.frame.size();
    return total;
}

void NetSession::disconnect(const std::string& why) {
    if (currentPhase == SessionPhase::Disconnected) return;
    currentPhase = SessionPhase::Disconnected;
    reason = why;
    outbound.clear();
}

void NetSession::enqueue(PacketId id, const PacketWriter& body, bool reliable) {
    ByteWriter stream;
    writeFramed(stream, id, body);
    QueuedPacket packet;
    packet.frame = stream.bytes();
    packet.reliable = reliable;
    statistics.bytesQueued += packet.frame.size();
    outbound.push_back(std::move(packet));
}

void NetSession::beginHandshake(const std::string& playerName, uint64_t high, uint64_t low) {
    name = playerName;
    uuidHi = high;
    uuidLo = low;

    Handshake hello;
    hello.protocolVersion = config.protocolVersion;
    hello.serverAddress = "loopback";
    hello.serverPort = 0;
    hello.nextState = 2;
    PacketWriter helloBody;
    hello.encode(helloBody);
    enqueue(PacketId::Handshake, helloBody, true);

    LoginStart login;
    login.name = playerName;
    login.uuidHi = high;
    login.uuidLo = low;
    PacketWriter loginBody;
    login.encode(loginBody);
    enqueue(PacketId::LoginStart, loginBody, true);

    handshakeStarted = true;
    currentPhase = SessionPhase::Login;
}

bool NetSession::legalHere(PacketId id) const {
    if (role == Role::Server) {
        switch (currentPhase) {
            case SessionPhase::Handshake:
                return id == PacketId::Handshake;
            case SessionPhase::Login:
                return id == PacketId::LoginStart;
            case SessionPhase::Play:
                return id == PacketId::PlayerState || id == PacketId::KeepAlive ||
                       id == PacketId::BlockChange;
            case SessionPhase::Disconnected:
                return false;
        }
        return false;
    }

    switch (currentPhase) {
        case SessionPhase::Handshake:
            return false;
        case SessionPhase::Login:
            return id == PacketId::JoinGame;
        case SessionPhase::Play:
            return id == PacketId::ChunkData || id == PacketId::BlockChange ||
                   id == PacketId::MultiBlockChange || id == PacketId::EntityMove ||
                   id == PacketId::PlayerState || id == PacketId::KeepAlive;
        case SessionPhase::Disconnected:
            return false;
    }
    return false;
}

void NetSession::enterPlay(uint64_t tick) {
    currentPhase = SessionPhase::Play;
    lastKeepAliveHeard = tick;
    lastKeepAliveSent = tick;
    if (role == Role::Server) recentre(clientView.position);
}

void NetSession::recentre(const Vec3& position) {
    clientView.position = position;
    Vec3i centre = chunkOf(position);
    if (clientView.tracker.hasCentre() && clientView.tracker.centre() == centre) return;

    std::vector<Vec3i> entered;
    std::vector<Vec3i> left;
    clientView.tracker.update(centre, entered, left);
    clientView.centreChunk = centre;
    for (const Vec3i& coord : entered) clientView.queueChunk(coord);
    for (const Vec3i& coord : left) clientView.forget(coord);
}

void NetSession::markChunkDirty(const Vec3i& coord) {
    if (!dirtyLookup.insert(coord).second) return;
    dirtyChunks.push_back(coord);
}

void NetSession::sendEntityMove(int32_t movedEntityId, const Vec3& position, float yaw, float pitch,
                                bool onGround) {
    if (currentPhase != SessionPhase::Play) return;
    EntityMove move;
    move.entityId = movedEntityId;
    move.x = position.x;
    move.y = position.y;
    move.z = position.z;
    move.yaw = yaw;
    move.pitch = pitch;
    move.onGround = onGround;
    PacketWriter body;
    move.encode(body);
    enqueue(PacketId::EntityMove, body, false);
    ++statistics.entityMovesSent;
}

void NetSession::sendPlayerState(const PlayerState& state) {
    if (currentPhase != SessionPhase::Play) return;
    PacketWriter body;
    state.encode(body);
    enqueue(PacketId::PlayerState, body, role == Role::Server);
}

void NetSession::requestBlockEdit(int32_t x, int32_t y, int32_t z, Block block) {
    if (currentPhase != SessionPhase::Play) return;
    BlockChange change;
    change.x = x;
    change.y = y;
    change.z = z;
    change.block = block;
    PacketWriter body;
    change.encode(body);
    enqueue(PacketId::BlockChange, body, true);
}

bool NetSession::entityMove(int32_t movedEntityId, EntityMove& out) const {
    auto it = entities.find(movedEntityId);
    if (it == entities.end()) return false;
    out = it->second;
    return true;
}

void NetSession::handleMessage(const ByteBuffer& message, uint64_t tick) {
    std::vector<Frame> frames;
    std::size_t consumed = 0;
    splitFrames(message.data(), message.size(), frames, consumed);
    for (const Frame& frame : frames) {
        if (currentPhase == SessionPhase::Disconnected) return;
        handleFrame(frame, tick);
    }
}

void NetSession::handleFrame(const Frame& frame, uint64_t tick) {
    ++statistics.packetsReceived;
    if (!legalHere(frame.id)) {
        ++statistics.packetsRejected;
        disconnect("packet out of phase");
        return;
    }

    PacketReader reader(frame.payload.data(), frame.payload.size());
    switch (frame.id) {
        case PacketId::Handshake: {
            Handshake hello;
            if (!hello.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed handshake");
                return;
            }
            if (hello.protocolVersion != config.protocolVersion) {
                ++statistics.packetsRejected;
                disconnect("protocol version mismatch");
                return;
            }
            if (hello.nextState != 2) {
                ++statistics.packetsRejected;
                disconnect("unsupported next state");
                return;
            }
            currentPhase = SessionPhase::Login;
            return;
        }
        case PacketId::LoginStart: {
            LoginStart login;
            if (!login.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed login");
                return;
            }
            name = login.name;
            uuidHi = login.uuidHi;
            uuidLo = login.uuidLo;

            JoinGame join;
            join.entityId = ownEntityId;
            join.gameMode = config.gameMode;
            join.dimension = config.dimension;
            join.seedHash = config.seedHash;
            join.maxPlayers = config.maxPlayers;
            join.levelType = config.levelType;
            join.viewDistance = config.viewRadius;
            join.reducedDebugInfo = false;
            PacketWriter body;
            join.encode(body);
            enqueue(PacketId::JoinGame, body, true);
            enterPlay(tick);
            return;
        }
        case PacketId::JoinGame: {
            JoinGame join;
            if (!join.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed join");
                return;
            }
            ownEntityId = join.entityId;
            clientView.tracker.setRadius(join.viewDistance);
            enterPlay(tick);
            return;
        }
        case PacketId::ChunkData: {
            ChunkData snapshot;
            if (!snapshot.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed chunk data");
                return;
            }
            clientSync.applyFull(snapshot, *worldRef);
            ++statistics.chunksApplied;
            return;
        }
        case PacketId::BlockChange: {
            BlockChange change;
            if (!change.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed block change");
                return;
            }
            if (role == Role::Server) {
                edits.push_back(change);
                return;
            }
            clientSync.applyBlockChange(change, *worldRef);
            ++statistics.blockChangesApplied;
            return;
        }
        case PacketId::MultiBlockChange: {
            MultiBlockChange delta;
            if (!delta.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed multi block change");
                return;
            }
            clientSync.applyDelta(delta, *worldRef);
            ++statistics.multiBlockChangesApplied;
            return;
        }
        case PacketId::EntityMove: {
            EntityMove move;
            if (!move.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed entity move");
                return;
            }
            entities[move.entityId] = move;
            ++statistics.entityMovesApplied;
            return;
        }
        case PacketId::PlayerState: {
            PlayerState state;
            if (!state.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed player state");
                return;
            }
            if (role == Role::Server) {
                recentre(Vec3{state.x, state.y, state.z});
                return;
            }
            latestPlayerState = state;
            return;
        }
        case PacketId::KeepAlive: {
            KeepAlive beat;
            if (!beat.decode(reader)) {
                ++statistics.packetsRejected;
                disconnect("malformed keep alive");
                return;
            }
            if (role == Role::Server) {
                if (beat.token > pendingKeepAliveToken) return;
                if (beat.token <= acceptedKeepAliveToken) return;
                acceptedKeepAliveToken = beat.token;
                lastKeepAliveHeard = tick;
                return;
            }
            lastKeepAliveHeard = tick;
            PacketWriter body;
            beat.encode(body);
            enqueue(PacketId::KeepAlive, body, true);
            return;
        }
    }
}

void NetSession::streamChunks() {
    uint32_t budget = config.chunksPerTick;
    Vec3i coord;
    while (budget > 0 && clientView.takeNextPending(coord)) {
        if (!clientView.tracker.contains(coord)) continue;
        ChunkData snapshot;
        if (!syncRef->buildFull(*worldRef, coord, snapshot)) continue;
        PacketWriter body;
        snapshot.encode(body);
        enqueue(PacketId::ChunkData, body, true);
        clientView.sent.insert(coord);
        ++statistics.chunksSent;
        --budget;
    }
}

void NetSession::flushBlockChanges() {
    for (const Vec3i& coord : dirtyChunks) {
        if (!clientView.hasChunk(coord)) {
            if (clientView.tracker.contains(coord)) clientView.queueChunk(coord);
            continue;
        }
        MultiBlockChange delta;
        if (!syncRef->buildDelta(*worldRef, coord, delta)) continue;

        if (delta.changes.size() > config.multiBlockThreshold) {
            PacketWriter body;
            delta.encode(body);
            enqueue(PacketId::MultiBlockChange, body, true);
            ++statistics.multiBlockChangesSent;
            continue;
        }
        for (const MultiBlockChange::Change& change : delta.changes) {
            int local = change.localIndex;
            int lx = local % chunkSize;
            int lz = (local / chunkSize) % chunkSize;
            int ly = local / (chunkSize * chunkSize);
            BlockChange single;
            single.x = coord.x * chunkSize + lx;
            single.y = coord.y * chunkSize + ly;
            single.z = coord.z * chunkSize + lz;
            single.block = change.block;
            PacketWriter body;
            single.encode(body);
            enqueue(PacketId::BlockChange, body, true);
            ++statistics.blockChangesSent;
        }
    }
    dirtyChunks.clear();
    dirtyLookup.clear();
}

void NetSession::serviceKeepAlive(uint64_t tick) {
    if (currentPhase != SessionPhase::Play) return;

    if (tick - lastKeepAliveHeard >= config.keepAliveTimeoutTicks) {
        keepAliveExpired = true;
        disconnect("keep alive timeout");
        return;
    }
    if (role != Role::Server) return;
    if (tick - lastKeepAliveSent < config.keepAliveIntervalTicks) return;

    KeepAlive beat;
    beat.token = static_cast<int64_t>(tick);
    PacketWriter body;
    beat.encode(body);
    enqueue(PacketId::KeepAlive, body, true);
    pendingKeepAliveToken = beat.token;
    lastKeepAliveSent = tick;
    ++statistics.keepAlivesSent;
}

void NetSession::flushOutbound() {
    std::size_t budget = config.bandwidthBytesPerTick;
    std::size_t used = 0;
    while (!outbound.empty()) {
        const QueuedPacket& head = outbound.front();
        if (budget != 0 && used != 0 && used + head.frame.size() > budget) break;
        if (head.reliable)
            transport->sendReliable(head.frame);
        else
            transport->sendUnreliable(head.frame);
        used += head.frame.size();
        statistics.bytesFlushed += head.frame.size();
        ++statistics.packetsFlushed;
        outbound.pop_front();
        if (budget != 0 && used >= budget) break;
    }
}

void NetSession::receive(uint64_t tick) {
    transport->pump(tick);
    ByteBuffer message;
    while (transport->popReliable(message)) handleMessage(message, tick);
    while (transport->popUnreliable(message)) handleMessage(message, tick);
}

void NetSession::send(uint64_t tick) {
    if (currentPhase == SessionPhase::Disconnected) return;
    serviceKeepAlive(tick);
    if (currentPhase == SessionPhase::Disconnected) return;

    if (role == Role::Server && currentPhase == SessionPhase::Play) {
        streamChunks();
        flushBlockChanges();
    }
    flushOutbound();
    transport->flush(tick);
}

void NetSession::update(uint64_t tick) {
    receive(tick);
    send(tick);
}

}
