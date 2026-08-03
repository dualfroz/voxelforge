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

}  // namespace

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

