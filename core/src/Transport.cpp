#include "vf/Transport.h"

#include <algorithm>

namespace vf {

namespace {

std::size_t varUintSize(uint32_t value) {
    std::size_t bytes = 1;
    while (value >= 0x80u) {
        value >>= 7;
        ++bytes;
    }
    return bytes;
}

std::size_t fragmentEncodedSize(uint32_t messageId, uint32_t fragmentIndex, uint32_t fragmentCount,
                                std::size_t payload) {
    return 1 + varUintSize(messageId) + varUintSize(fragmentIndex) + varUintSize(fragmentCount) +
           varUintSize(static_cast<uint32_t>(payload)) + payload;
}

bool sequenceGreater(SequenceNumber a, SequenceNumber b) {
    return (a != b) && (static_cast<SequenceNumber>(a - b) < 0x8000u);
}

uint16_t sequenceDistance(SequenceNumber ahead, SequenceNumber behind) {
    return static_cast<uint16_t>(ahead - behind);
}

}

LoopbackLink::LoopbackLink(LinkQuality quality) : quality(quality), rngState(quality.seed | 1ull) {}

uint64_t LoopbackLink::nextRandom() {
    rngState += 0x9e3779b97f4a7c15ull;
    uint64_t z = rngState;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

bool LoopbackLink::roll(uint32_t percent) {
    if (percent == 0) return false;
    if (percent >= 100) return true;
    return (nextRandom() % 100ull) < percent;
}

void LoopbackLink::enqueue(int toEndpoint, const ByteBuffer& bytes, uint64_t deliveryTick) {
    PendingDatagram entry;
    entry.deliveryTick = deliveryTick;
    entry.order = orderCounter++;
    entry.toEndpoint = toEndpoint;
    entry.bytes = bytes;
    pending.push_back(std::move(entry));
}

void LoopbackLink::send(int fromEndpoint, const uint8_t* datagram, std::size_t size, uint64_t tick) {
    ByteBuffer bytes(datagram, datagram + size);
    send(fromEndpoint, bytes, tick);
}

void LoopbackLink::send(int fromEndpoint, const ByteBuffer& datagram, uint64_t tick) {
    ++sent;
    int toEndpoint = fromEndpoint == 0 ? 1 : 0;

    if (roll(quality.lossPercent)) {
        ++dropped;
        return;
    }

    uint64_t deliveryTick = tick + quality.latencyTicks;
    if (roll(quality.reorderPercent)) {
        deliveryTick += quality.reorderExtraTicks;
        ++delayed;
    }
    enqueue(toEndpoint, datagram, deliveryTick);

    if (roll(quality.duplicatePercent)) {
        ++duplicated;
        enqueue(toEndpoint, datagram, deliveryTick);
    }
}

void LoopbackLink::receive(int endpoint, uint64_t tick, std::vector<ByteBuffer>& out) {
    out.clear();
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const PendingDatagram& entry = pending[i];
        if (entry.toEndpoint == endpoint && entry.deliveryTick <= tick) ready.push_back(i);
    }
    std::stable_sort(ready.begin(), ready.end(), [this](std::size_t a, std::size_t b) {
        if (pending[a].deliveryTick != pending[b].deliveryTick)
            return pending[a].deliveryTick < pending[b].deliveryTick;
        return pending[a].order < pending[b].order;
    });
    out.reserve(ready.size());
    for (std::size_t index : ready) out.push_back(pending[index].bytes);

    std::vector<PendingDatagram> keep;
    keep.reserve(pending.size() - ready.size());
    std::vector<uint8_t> taken(pending.size(), 0);
    for (std::size_t index : ready) taken[index] = 1;
    for (std::size_t i = 0; i < pending.size(); ++i)
        if (!taken[i]) keep.push_back(std::move(pending[i]));
    pending.swap(keep);
}

void LoopbackLink::setQuality(const LinkQuality& newQuality) {
    quality = newQuality;
    rngState = newQuality.seed | 1ull;
}

void LoopbackLink::reset() {
    pending.clear();
    orderCounter = 0;
    rngState = quality.seed | 1ull;
    sent = dropped = duplicated = delayed = 0;
}

void Datagram::encode(ByteWriter& out) const {
    out.putU16(sequence);
    out.putU16(ackLatest);
    out.putU32(ackHistory);
    out.putU8(ackValid ? 1u : 0u);
    out.putU16(static_cast<uint16_t>(fragments.size()));
    for (const Fragment& piece : fragments) {
        out.putU8(piece.reliable ? 1u : 0u);
        out.putVarU32(piece.messageId);
        out.putVarU32(piece.fragmentIndex);
        out.putVarU32(piece.fragmentCount);
        out.putVarU32(static_cast<uint32_t>(piece.bytes.size()));
        out.putBytes(piece.bytes.data(), piece.bytes.size());
    }
}

bool Datagram::decode(const uint8_t* data, std::size_t size) {
    ByteReader reader(data, size);
    uint16_t count = 0;
    uint8_t ackFlag = 0;
    if (!reader.getU16(sequence) || !reader.getU16(ackLatest) || !reader.getU32(ackHistory) ||
        !reader.getU8(ackFlag) || !reader.getU16(count))
        return false;
    ackValid = ackFlag != 0;

    fragments.clear();
    fragments.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        Fragment piece;
        uint8_t flags = 0;
        uint32_t length = 0;
        if (!reader.getU8(flags) || !reader.getVarU32(piece.messageId) ||
            !reader.getVarU32(piece.fragmentIndex) || !reader.getVarU32(piece.fragmentCount) ||
            !reader.getVarU32(length))
            return false;
        if (length > reader.remaining()) return false;
        piece.reliable = (flags & 1u) != 0;
        piece.bytes.resize(length);
        if (length != 0 && !reader.getBytes(piece.bytes.data(), length)) return false;
        if (piece.fragmentCount == 0 || piece.fragmentIndex >= piece.fragmentCount) return false;
        fragments.push_back(std::move(piece));
    }
    return reader.ok();
}

uint64_t ReliableChannel::fragmentKey(uint32_t messageId, uint32_t fragmentIndex) {
    return (static_cast<uint64_t>(messageId) << 32) | static_cast<uint64_t>(fragmentIndex);
}

void ReliableChannel::queueMessage(const uint8_t* payload, std::size_t size,
                                   std::size_t fragmentPayload) {
    uint32_t messageId = nextOutgoingMessageId++;
    std::size_t total = size == 0 ? 1 : (size + fragmentPayload - 1) / fragmentPayload;
    for (std::size_t index = 0; index < total; ++index) {
        OutgoingFragment piece;
        piece.messageId = messageId;
        piece.fragmentIndex = static_cast<uint32_t>(index);
        piece.fragmentCount = static_cast<uint32_t>(total);
        std::size_t begin = index * fragmentPayload;
        std::size_t end = std::min(size, begin + fragmentPayload);
        piece.bytes.assign(payload + begin, payload + end);
        outgoing.emplace(fragmentKey(messageId, piece.fragmentIndex), std::move(piece));
    }
}

void ReliableChannel::collectReady(std::vector<uint64_t>& keys) const {
    keys.clear();
    for (const auto& entry : outgoing)
        if (!entry.second.inFlight) keys.push_back(entry.first);
}

std::size_t ReliableChannel::readyFragmentCount() const {
    std::size_t count = 0;
    for (const auto& entry : outgoing)
        if (!entry.second.inFlight) ++count;
    return count;
}

const ReliableChannel::OutgoingFragment* ReliableChannel::fragment(uint64_t key) const {
    auto it = outgoing.find(key);
    return it == outgoing.end() ? nullptr : &it->second;
}

void ReliableChannel::markInFlight(uint64_t key) {
    auto it = outgoing.find(key);
    if (it != outgoing.end()) it->second.inFlight = true;
}

void ReliableChannel::markAcked(uint64_t key) { outgoing.erase(key); }

void ReliableChannel::markLost(uint64_t key) {
    auto it = outgoing.find(key);
    if (it != outgoing.end()) it->second.inFlight = false;
}

bool ReliableChannel::accept(const Datagram::Fragment& piece) {
    if (piece.messageId < nextDeliverMessageId) return false;
    if (completed.count(piece.messageId) != 0) return false;

    auto it = incoming.find(piece.messageId);
    if (it == incoming.end()) {
        Assembly assembly;
        assembly.fragmentCount = piece.fragmentCount;
        assembly.parts.resize(piece.fragmentCount);
        assembly.have.assign(piece.fragmentCount, 0);
        it = incoming.emplace(piece.messageId, std::move(assembly)).first;
    }
    Assembly& assembly = it->second;
    if (piece.fragmentIndex >= assembly.fragmentCount) return false;
    if (assembly.have[piece.fragmentIndex]) return false;

    assembly.parts[piece.fragmentIndex] = piece.bytes;
    assembly.have[piece.fragmentIndex] = 1;
    ++assembly.received;

    if (assembly.received == assembly.fragmentCount) {
        ByteBuffer whole;
        std::size_t total = 0;
        for (const ByteBuffer& part : assembly.parts) total += part.size();
        whole.reserve(total);
        for (const ByteBuffer& part : assembly.parts)
            whole.insert(whole.end(), part.begin(), part.end());
        completed.emplace(piece.messageId, std::move(whole));
        incoming.erase(it);
        drainCompleted();
    }
    return true;
}

void ReliableChannel::drainCompleted() {
    for (;;) {
        auto it = completed.find(nextDeliverMessageId);
        if (it == completed.end()) return;
        delivered.push_back(std::move(it->second));
        completed.erase(it);
        ++nextDeliverMessageId;
    }
}

bool ReliableChannel::popDelivered(ByteBuffer& out) {
    if (delivered.empty()) return false;
    out = std::move(delivered.front());
    delivered.pop_front();
    return true;
}

void UnreliableChannel::queueMessage(const uint8_t* payload, std::size_t size,
                                     std::size_t fragmentPayload) {
    uint32_t messageId = nextOutgoingMessageId++;
    std::size_t total = size == 0 ? 1 : (size + fragmentPayload - 1) / fragmentPayload;
    for (std::size_t index = 0; index < total; ++index) {
        Datagram::Fragment piece;
        piece.reliable = false;
        piece.messageId = messageId;
        piece.fragmentIndex = static_cast<uint32_t>(index);
        piece.fragmentCount = static_cast<uint32_t>(total);
        std::size_t begin = index * fragmentPayload;
        std::size_t end = std::min(size, begin + fragmentPayload);
        piece.bytes.assign(payload + begin, payload + end);
        outgoing.push_back(std::move(piece));
    }
}

const Datagram::Fragment* UnreliableChannel::peekOutgoing() const {
    return outgoing.empty() ? nullptr : &outgoing.front();
}

void UnreliableChannel::dropOutgoing() { outgoing.pop_front(); }

bool UnreliableChannel::accept(const Datagram::Fragment& piece) {
    if (anyDelivered && piece.messageId <= highestDelivered) {
        ++stale;
        return false;
    }

    auto it = incoming.find(piece.messageId);
    if (it == incoming.end()) {
        Assembly assembly;
        assembly.fragmentCount = piece.fragmentCount;
        assembly.parts.resize(piece.fragmentCount);
        assembly.have.assign(piece.fragmentCount, 0);
        it = incoming.emplace(piece.messageId, std::move(assembly)).first;
    }
    Assembly& assembly = it->second;
    if (piece.fragmentIndex >= assembly.fragmentCount) return false;
    if (assembly.have[piece.fragmentIndex]) return false;

    assembly.parts[piece.fragmentIndex] = piece.bytes;
    assembly.have[piece.fragmentIndex] = 1;
    ++assembly.received;
    if (assembly.received != assembly.fragmentCount) return true;

    ByteBuffer whole;
    std::size_t total = 0;
    for (const ByteBuffer& part : assembly.parts) total += part.size();
    whole.reserve(total);
    for (const ByteBuffer& part : assembly.parts)
        whole.insert(whole.end(), part.begin(), part.end());
    delivered.push_back(std::move(whole));
    highestDelivered = piece.messageId;
    anyDelivered = true;
    incoming.erase(it);

    for (auto stalePart = incoming.begin(); stalePart != incoming.end();) {
        if (stalePart->first < highestDelivered) {
            ++stale;
            stalePart = incoming.erase(stalePart);
        } else {
            ++stalePart;
        }
    }
    return true;
}

bool UnreliableChannel::popDelivered(ByteBuffer& out) {
    if (delivered.empty()) return false;
    out = std::move(delivered.front());
    delivered.pop_front();
    return true;
}

Transport::Transport(LoopbackLink& link, int endpoint, TransportConfig config)
    : link(&link), endpoint(endpoint), config(config) {
    std::size_t floorMtu = Datagram::headerSize() + Datagram::maxFragmentOverhead() + 16;
    if (this->config.mtu < floorMtu) this->config.mtu = floorMtu;
    if (this->config.sendWindow == 0) this->config.sendWindow = 1;
    if (this->config.retransmitTimeoutTicks == 0) this->config.retransmitTimeoutTicks = 1;
}

std::size_t Transport::maxFragmentPayload() const {
    return config.mtu - Datagram::headerSize() - Datagram::maxFragmentOverhead();
}

void Transport::sendReliable(const uint8_t* payload, std::size_t size) {
    reliable.queueMessage(payload, size, maxFragmentPayload());
    ++statistics.messagesQueued;
}

void Transport::sendReliable(const ByteBuffer& payload) {
    sendReliable(payload.data(), payload.size());
}

void Transport::sendUnreliable(const uint8_t* payload, std::size_t size) {
    unreliable.queueMessage(payload, size, maxFragmentPayload());
    ++statistics.messagesQueued;
}

void Transport::sendUnreliable(const ByteBuffer& payload) {
    sendUnreliable(payload.data(), payload.size());
}

bool Transport::registerIncoming(SequenceNumber sequence) {
    if (!anyReceived) {
        anyReceived = true;
        remoteLatest = sequence;
        remoteHistory = 0;
        return true;
    }
    if (sequence == remoteLatest) return false;

    if (sequenceGreater(sequence, remoteLatest)) {
        uint16_t shift = sequenceDistance(sequence, remoteLatest);
        if (shift >= 32) {
            remoteHistory = 0;
        } else {
            remoteHistory = (remoteHistory << shift) | (1u << (shift - 1));
        }
        remoteLatest = sequence;
        return true;
    }

    uint16_t behind = sequenceDistance(remoteLatest, sequence);
    if (behind > 32) return false;
    uint32_t bit = 1u << (behind - 1);
    if ((remoteHistory & bit) != 0) return false;
    remoteHistory |= bit;
    return true;
}

void Transport::acknowledge(SequenceNumber sequence, uint64_t tick) {
    for (std::size_t i = 0; i < inFlight.size(); ++i) {
        if (inFlight[i].sequence != sequence) continue;
        sampleRtt(tick - inFlight[i].sentTick);
        for (uint64_t key : inFlight[i].reliableKeys) reliable.markAcked(key);
        inFlight.erase(inFlight.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

void Transport::applyAcks(SequenceNumber ackLatest, AckBitfield ackHistory, uint64_t tick) {
    acknowledge(ackLatest, tick);
    for (int bit = 0; bit < 32; ++bit) {
        if ((ackHistory & (1u << bit)) == 0) continue;
        acknowledge(static_cast<SequenceNumber>(ackLatest - 1 - bit), tick);
    }
}

void Transport::sampleRtt(uint64_t sample) {
    double value = static_cast<double>(sample);
    if (statistics.smoothedRttTicks <= 0.0) {
        statistics.smoothedRttTicks = value;
        return;
    }
    double weight = static_cast<double>(config.rttSmoothingPercent) / 100.0;
    statistics.smoothedRttTicks += (value - statistics.smoothedRttTicks) * weight;
}

void Transport::pump(uint64_t tick) {
    link->receive(endpoint, tick, arrivals);
    for (const ByteBuffer& raw : arrivals) {
        ++statistics.datagramsReceived;
        statistics.bytesReceived += raw.size();

        Datagram datagram;
        if (!datagram.decode(raw.data(), raw.size())) continue;
        if (!registerIncoming(datagram.sequence)) {
            ++statistics.duplicatesDropped;
            continue;
        }
        if (datagram.ackValid) applyAcks(datagram.ackLatest, datagram.ackHistory, tick);
        if (!datagram.fragments.empty()) ackPending = true;

        for (const Datagram::Fragment& piece : datagram.fragments) {
            bool accepted = piece.reliable ? reliable.accept(piece) : unreliable.accept(piece);
            if (!accepted) ++statistics.staleDropped;
        }
    }
}

bool Transport::buildDatagram(Datagram& out) {
    out.fragments.clear();
    std::size_t used = Datagram::headerSize();

    std::vector<uint64_t> ready;
    reliable.collectReady(ready);
    for (uint64_t key : ready) {
        const ReliableChannel::OutgoingFragment* piece = reliable.fragment(key);
        std::size_t cost = fragmentEncodedSize(piece->messageId, piece->fragmentIndex,
                                               piece->fragmentCount, piece->bytes.size());
        if (used + cost > config.mtu) break;
        Datagram::Fragment wire;
        wire.reliable = true;
        wire.messageId = piece->messageId;
        wire.fragmentIndex = piece->fragmentIndex;
        wire.fragmentCount = piece->fragmentCount;
        wire.bytes = piece->bytes;
        out.fragments.push_back(std::move(wire));
        used += cost;
        reliable.markInFlight(key);
    }

    while (const Datagram::Fragment* piece = unreliable.peekOutgoing()) {
        std::size_t cost = fragmentEncodedSize(piece->messageId, piece->fragmentIndex,
                                               piece->fragmentCount, piece->bytes.size());
        if (used + cost > config.mtu) break;
        out.fragments.push_back(*piece);
        used += cost;
        unreliable.dropOutgoing();
    }

    return !out.fragments.empty();
}

void Transport::flush(uint64_t tick) {
    for (std::size_t i = 0; i < inFlight.size();) {
        if (tick - inFlight[i].sentTick < config.retransmitTimeoutTicks) {
            ++i;
            continue;
        }
        ++statistics.datagramsLost;
        for (uint64_t key : inFlight[i].reliableKeys) {
            if (reliable.fragment(key) == nullptr) continue;
            reliable.markLost(key);
            ++statistics.retransmitted;
        }
        inFlight.erase(inFlight.begin() + static_cast<std::ptrdiff_t>(i));
    }

    bool sentAnything = false;
    while (inFlight.size() < config.sendWindow) {
        Datagram datagram;
        if (!buildDatagram(datagram)) break;

        datagram.sequence = nextSequence++;
        datagram.ackLatest = remoteLatest;
        datagram.ackHistory = remoteHistory;
        datagram.ackValid = anyReceived;

        InFlightDatagram record;
        record.sequence = datagram.sequence;
        record.sentTick = tick;
        for (const Datagram::Fragment& piece : datagram.fragments)
            if (piece.reliable)
                record.reliableKeys.push_back(
                    ReliableChannel::fragmentKey(piece.messageId, piece.fragmentIndex));
        inFlight.push_back(std::move(record));

        ByteWriter writer;
        datagram.encode(writer);
        ++statistics.datagramsSent;
        statistics.bytesSent += writer.size();
        link->send(endpoint, writer.bytes(), tick);
        sentAnything = true;
    }

    if (!sentAnything && ackPending) {
        Datagram datagram;
        datagram.sequence = nextSequence++;
        datagram.ackLatest = remoteLatest;
        datagram.ackHistory = remoteHistory;
        datagram.ackValid = anyReceived;
        ByteWriter writer;
        datagram.encode(writer);
        ++statistics.datagramsSent;
        statistics.bytesSent += writer.size();
        link->send(endpoint, writer.bytes(), tick);
    }
    ackPending = false;
}

void Transport::update(uint64_t tick) {
    pump(tick);
    flush(tick);
}

bool Transport::popReliable(ByteBuffer& out) {
    if (!reliable.popDelivered(out)) return false;
    ++statistics.messagesDelivered;
    return true;
}

bool Transport::popUnreliable(ByteBuffer& out) {
    if (!unreliable.popDelivered(out)) return false;
    ++statistics.messagesDelivered;
    return true;
}

std::size_t Transport::pendingOutgoingCount() const {
    return reliable.readyFragmentCount() + unreliable.outgoingFragmentCount();
}

}
