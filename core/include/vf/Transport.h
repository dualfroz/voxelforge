#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>
#include "Codec.h"

namespace vf {

// Datagram numbering is 16-bit and wraps, so every comparison between two
// sequence numbers has to be modular rather than arithmetic.
using SequenceNumber = uint16_t;

// Bit i means "the datagram numbered ackLatest - 1 - i also arrived", so one
// 32-bit word confirms 33 datagrams and a single surviving ack can recover the
// whole window that came before it.
using AckBitfield = uint32_t;

// Everything a hostile link does to a datagram, as integer percents and tick
// counts. The seed makes a run reproducible: the same sequence of sends always
// produces the same sequence of arrivals.
struct LinkQuality {
    uint32_t lossPercent = 0;
    uint32_t duplicatePercent = 0;
    uint32_t reorderPercent = 0;
    uint32_t latencyTicks = 1;
    uint32_t reorderExtraTicks = 3;
    uint64_t seed = 0x9e3779b97f4a7c15ull;
};

// A two-endpoint in-memory pipe: no sockets, no threads, no wall clock.
// Delivery is a tick-stamped queue and every impairment is drawn from a seeded
// stream, which is what makes the transport tests reproducible.
class LoopbackLink {
public:
    explicit LoopbackLink(LinkQuality quality = {});

    void send(int fromEndpoint, const uint8_t* datagram, std::size_t size, uint64_t tick);
    void send(int fromEndpoint, const ByteBuffer& datagram, uint64_t tick);

    // Every datagram addressed to `endpoint` whose delivery tick has arrived,
    // oldest first. Same-tick arrivals keep their send order.
    void receive(int endpoint, uint64_t tick, std::vector<ByteBuffer>& out);

    // Re-seeds the impairment stream, so a test can change the link mid-run.
    void setQuality(const LinkQuality& newQuality);
    const LinkQuality& settings() const { return quality; }
    void reset();

    uint64_t sentCount() const { return sent; }
    uint64_t droppedCount() const { return dropped; }
    uint64_t duplicatedCount() const { return duplicated; }
    uint64_t delayedCount() const { return delayed; }
    std::size_t inFlightCount() const { return pending.size(); }

private:
    struct PendingDatagram {
        uint64_t deliveryTick = 0;
        uint64_t order = 0;
        int toEndpoint = 0;
        ByteBuffer bytes;
    };

    uint64_t nextRandom();
    bool roll(uint32_t percent);
    void enqueue(int toEndpoint, const ByteBuffer& bytes, uint64_t deliveryTick);

    LinkQuality quality;
    uint64_t rngState;
    uint64_t orderCounter = 0;
    std::vector<PendingDatagram> pending;
    uint64_t sent = 0;
    uint64_t dropped = 0;
    uint64_t duplicated = 0;
    uint64_t delayed = 0;
};

// One unit on the wire: an ack header plus however many message fragments fit
// under the MTU. A fragment carries its own message identity so a datagram can
// be lost without losing track of what was in it.
struct Datagram {
    struct Fragment {
        bool reliable = false;
        uint32_t messageId = 0;
        uint32_t fragmentIndex = 0;
        uint32_t fragmentCount = 1;
        ByteBuffer bytes;
    };

    SequenceNumber sequence = 0;
    SequenceNumber ackLatest = 0;
    AckBitfield ackHistory = 0;
    // Until the sender has itself received something, ackLatest is meaningless
    // and must not be mistaken for an acknowledgement of sequence 0.
    bool ackValid = false;
    std::vector<Fragment> fragments;

    void encode(ByteWriter& out) const;
    bool decode(const uint8_t* data, std::size_t size);

    static std::size_t headerSize() { return 11; }
    static std::size_t maxFragmentOverhead() { return 21; }
};

struct TransportConfig {
    std::size_t mtu = 512;
    uint32_t retransmitTimeoutTicks = 8;
    uint32_t sendWindow = 32;
    uint32_t rttSmoothingPercent = 20;
};

struct TransportStats {
    uint64_t datagramsSent = 0;
    uint64_t datagramsReceived = 0;
    uint64_t datagramsLost = 0;
    uint64_t retransmitted = 0;
    uint64_t duplicatesDropped = 0;
    uint64_t staleDropped = 0;
    uint64_t messagesQueued = 0;
    uint64_t messagesDelivered = 0;
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    double smoothedRttTicks = 0.0;
};

// Ordered, exactly-once message stream. Outgoing fragments live until acked;
// incoming fragments are reassembled per message and then held back until
// every earlier message id has been handed over.
class ReliableChannel {
public:
    struct OutgoingFragment {
        uint32_t messageId = 0;
        uint32_t fragmentIndex = 0;
        uint32_t fragmentCount = 1;
        ByteBuffer bytes;
        bool inFlight = false;
    };

    void queueMessage(const uint8_t* payload, std::size_t size, std::size_t fragmentPayload);

    void collectReady(std::vector<uint64_t>& keys) const;
    const OutgoingFragment* fragment(uint64_t key) const;
    void markInFlight(uint64_t key);
    void markAcked(uint64_t key);
    void markLost(uint64_t key);

    std::size_t unackedFragmentCount() const { return outgoing.size(); }
    std::size_t readyFragmentCount() const;

    // False when the fragment belongs to a message already handed over, which
    // is what a retransmission of an already-delivered message looks like.
    bool accept(const Datagram::Fragment& fragment);
    bool popDelivered(ByteBuffer& out);
    std::size_t deliveredWaiting() const { return delivered.size(); }
    uint32_t nextExpectedMessageId() const { return nextDeliverMessageId; }
    std::size_t reorderBufferSize() const { return completed.size(); }

    static uint64_t fragmentKey(uint32_t messageId, uint32_t fragmentIndex);

private:
    struct Assembly {
        uint32_t fragmentCount = 0;
        uint32_t received = 0;
        std::vector<ByteBuffer> parts;
        std::vector<uint8_t> have;
    };

    void drainCompleted();

    std::map<uint64_t, OutgoingFragment> outgoing;
    uint32_t nextOutgoingMessageId = 0;

    std::map<uint32_t, Assembly> incoming;
    std::map<uint32_t, ByteBuffer> completed;
    uint32_t nextDeliverMessageId = 0;
    std::deque<ByteBuffer> delivered;
};

// Unordered, send-once stream for state that ages badly. A message older than
// the newest one already delivered is thrown away instead of queued, because a
// stale entity position is worse than no update at all.
class UnreliableChannel {
public:
    void queueMessage(const uint8_t* payload, std::size_t size, std::size_t fragmentPayload);

    const Datagram::Fragment* peekOutgoing() const;
    void dropOutgoing();
    std::size_t outgoingFragmentCount() const { return outgoing.size(); }

    bool accept(const Datagram::Fragment& fragment);
    bool popDelivered(ByteBuffer& out);
    std::size_t deliveredWaiting() const { return delivered.size(); }
    uint32_t highestDeliveredMessageId() const { return highestDelivered; }
    uint64_t staleDroppedCount() const { return stale; }

private:
    struct Assembly {
        uint32_t fragmentCount = 0;
        uint32_t received = 0;
        std::vector<ByteBuffer> parts;
        std::vector<uint8_t> have;
    };

    std::deque<Datagram::Fragment> outgoing;
    uint32_t nextOutgoingMessageId = 0;

    std::map<uint32_t, Assembly> incoming;
    uint32_t highestDelivered = 0;
    bool anyDelivered = false;
    std::deque<ByteBuffer> delivered;
    uint64_t stale = 0;
};

// Reliability layered over the unreliable datagram link: sequencing, acks with
// a 32-entry bitfield, timeout-driven retransmission, a bounded send window and
// the two message channels. Time is the tick handed in from outside; the class
// never reads a clock.
class Transport {
public:
    Transport(LoopbackLink& link, int endpoint, TransportConfig config = {});

    void sendReliable(const uint8_t* payload, std::size_t size);
    void sendReliable(const ByteBuffer& payload);
    void sendUnreliable(const uint8_t* payload, std::size_t size);
    void sendUnreliable(const ByteBuffer& payload);

    // Reads arrivals and applies their acks.
    void pump(uint64_t tick);
    // Expires unacked datagrams, then packs and sends whatever fits.
    void flush(uint64_t tick);
    void update(uint64_t tick);

    bool popReliable(ByteBuffer& out);
    bool popUnreliable(ByteBuffer& out);

    const TransportStats& stats() const { return statistics; }
    const TransportConfig& settings() const { return config; }
    std::size_t maxFragmentPayload() const;
    std::size_t inFlightDatagramCount() const { return inFlight.size(); }
    std::size_t unackedFragmentCount() const { return reliable.unackedFragmentCount(); }
    std::size_t pendingOutgoingCount() const;
    ReliableChannel& reliableChannel() { return reliable; }
    UnreliableChannel& unreliableChannel() { return unreliable; }

private:
    struct InFlightDatagram {
        SequenceNumber sequence = 0;
        uint64_t sentTick = 0;
        std::vector<uint64_t> reliableKeys;
    };

    bool registerIncoming(SequenceNumber sequence);
    void applyAcks(SequenceNumber ackLatest, AckBitfield ackHistory, uint64_t tick);
    void acknowledge(SequenceNumber sequence, uint64_t tick);
    bool buildDatagram(Datagram& out);
    void sampleRtt(uint64_t sample);

    LoopbackLink* link;
    int endpoint;
    TransportConfig config;

    ReliableChannel reliable;
    UnreliableChannel unreliable;

    SequenceNumber nextSequence = 0;
    std::vector<InFlightDatagram> inFlight;

    SequenceNumber remoteLatest = 0;
    AckBitfield remoteHistory = 0;
    bool anyReceived = false;
    bool ackPending = false;

    std::vector<ByteBuffer> arrivals;
    TransportStats statistics;
};

} // namespace vf
