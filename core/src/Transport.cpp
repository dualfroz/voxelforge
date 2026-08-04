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

// Modular "is a newer than b" over the 16-bit sequence space.
bool sequenceGreater(SequenceNumber a, SequenceNumber b) {
    return (a != b) && (static_cast<SequenceNumber>(a - b) < 0x8000u);
}

uint16_t sequenceDistance(SequenceNumber ahead, SequenceNumber behind) {
    return static_cast<uint16_t>(ahead - behind);
}

}  // namespace

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

