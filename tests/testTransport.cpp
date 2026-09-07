#include "testHarness.h"
#include "vf/Transport.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

ByteBuffer messageOf(uint32_t index, std::size_t size) {
    ByteBuffer payload;
    payload.reserve(size);
    uint32_t state = index * 2654435761u + 12345u;
    for (std::size_t i = 0; i < size; ++i) {
        state = state * 1664525u + 1013904223u;
        payload.push_back(static_cast<uint8_t>((state >> 16) & 0xffu));
    }
    // The first four bytes carry the index so the receiver can prove ordering.
    for (int byte = 0; byte < 4 && static_cast<std::size_t>(byte) < payload.size(); ++byte)
        payload[static_cast<std::size_t>(byte)] =
            static_cast<uint8_t>((index >> (8 * byte)) & 0xffu);
    return payload;
}

uint32_t indexOf(const ByteBuffer& payload) {
    uint32_t index = 0;
    for (int byte = 3; byte >= 0; --byte)
        index = (index << 8) | payload[static_cast<std::size_t>(byte)];
    return index;
}

// One tick of a two-endpoint exchange, always in the same order so a run is
// reproducible from the link seed alone.
void stepPair(Transport& a, Transport& b, uint64_t tick) {
    a.pump(tick);
    a.flush(tick);
    b.pump(tick);
    b.flush(tick);
}

}  // namespace

VF_TEST(loopbackLinkReplaysTheSameImpairmentsForTheSameSeed) {
    LinkQuality quality;
    quality.lossPercent = 30;
    quality.duplicatePercent = 15;
    quality.reorderPercent = 25;
    quality.latencyTicks = 2;
    quality.seed = 0xfeedfacecafebeefull;

    std::vector<std::string> runs;
    for (int attempt = 0; attempt < 2; ++attempt) {
        LoopbackLink link(quality);
        std::string trace;
        for (uint64_t tick = 0; tick < 60; ++tick) {
            ByteBuffer payload = messageOf(static_cast<uint32_t>(tick), 8);
            link.send(0, payload, tick);
            std::vector<ByteBuffer> arrived;
            link.receive(1, tick, arrived);
            for (const ByteBuffer& one : arrived) {
                trace += std::to_string(tick);
                trace += ':';
                trace += std::to_string(indexOf(one));
                trace += ' ';
            }
        }
        trace += "d";
        trace += std::to_string(link.droppedCount());
        trace += "u";
        trace += std::to_string(link.duplicatedCount());
        runs.push_back(trace);
    }
    CHECK(runs[0] == runs[1]);
    CHECK(runs[0].size() > 40u);

    LoopbackLink probe(quality);
    for (uint64_t tick = 0; tick < 200; ++tick) probe.send(0, messageOf(1, 4), tick);
    CHECK(probe.droppedCount() > 30u);
    CHECK(probe.duplicatedCount() > 10u);
    CHECK(probe.delayedCount() > 20u);
}

VF_TEST(reliableChannelDeliversEveryMessageExactlyOnceInOrderOverALossyLink) {
    LinkQuality quality;
    quality.lossPercent = 30;
    quality.duplicatePercent = 12;
    quality.reorderPercent = 25;
    quality.reorderExtraTicks = 4;
    quality.latencyTicks = 2;
    quality.seed = 0x1234abcd5678ef01ull;

    LoopbackLink link(quality);
    TransportConfig config;
    config.mtu = 256;
    config.retransmitTimeoutTicks = 8;
    config.sendWindow = 16;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    const uint32_t messageCount = 240;
    for (uint32_t i = 0; i < messageCount; ++i) {
        ByteBuffer payload = messageOf(i, 40 + (i % 90));
        sender.sendReliable(payload);
    }

    std::vector<uint32_t> order;
    for (uint64_t tick = 0; tick < 4000 && order.size() < messageCount; ++tick) {
        stepPair(sender, receiver, tick);
        ByteBuffer got;
        while (receiver.popReliable(got)) order.push_back(indexOf(got));
    }

    CHECK_EQ(order.size(), static_cast<std::size_t>(messageCount));
    bool inOrder = true;
    for (std::size_t i = 0; i < order.size(); ++i)
        if (order[i] != static_cast<uint32_t>(i)) inOrder = false;
    CHECK(inOrder);

    // Payloads survive intact, not just their headers.
    for (uint32_t i = 0; i < 5 && i < order.size(); ++i) CHECK_EQ(order[i], i);

    CHECK(link.droppedCount() > 20u);
    CHECK(link.duplicatedCount() > 5u);
    CHECK(sender.stats().retransmitted > 0u);
    CHECK(receiver.stats().duplicatesDropped + receiver.stats().staleDropped > 0u);
    CHECK_EQ(receiver.stats().messagesDelivered, static_cast<uint64_t>(messageCount));
}

VF_TEST(reliableMessagesLargerThanTheMtuAreFragmentedAndReassembledByteForByte) {
    LinkQuality quality;
    quality.lossPercent = 20;
    quality.reorderPercent = 30;
    quality.reorderExtraTicks = 3;
    quality.latencyTicks = 1;
    quality.seed = 0x77aabbccddee0011ull;

    LoopbackLink link(quality);
    TransportConfig config;
    config.mtu = 128;
    config.retransmitTimeoutTicks = 5;
    config.sendWindow = 8;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    ByteBuffer huge = messageOf(9, 20000);
    CHECK(huge.size() > sender.maxFragmentPayload() * 100);
    sender.sendReliable(huge);
    CHECK(sender.unackedFragmentCount() > 100u);

    ByteBuffer got;
    bool arrived = false;
    uint64_t tick = 0;
    for (; tick < 6000 && !arrived; ++tick) {
        stepPair(sender, receiver, tick);
        if (receiver.popReliable(got)) arrived = true;
    }
    CHECK(arrived);
    // Let the last acks come back before asking whether anything is still unacked.
    for (uint64_t settle = 0; settle < 40; ++settle) stepPair(sender, receiver, tick + settle);
    CHECK_EQ(got.size(), huge.size());
    CHECK(got == huge);
    CHECK_EQ(sender.unackedFragmentCount(), static_cast<std::size_t>(0));

    ByteBuffer extra;
    CHECK(!receiver.popReliable(extra));
}

VF_TEST(unreliableChannelDropsAMessageOlderThanTheNewestOneAlreadyDelivered) {
    LinkQuality slow;
    slow.latencyTicks = 12;
    slow.seed = 0x2222333344445555ull;
    LoopbackLink link(slow);

    TransportConfig config;
    config.mtu = 256;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    sender.sendUnreliable(messageOf(0, 32));
    sender.flush(0);

    LinkQuality fast = slow;
    fast.latencyTicks = 1;
    link.setQuality(fast);

    sender.sendUnreliable(messageOf(1, 32));
    sender.flush(1);

    std::vector<uint32_t> received;
    for (uint64_t tick = 2; tick < 40; ++tick) {
        receiver.pump(tick);
        receiver.flush(tick);
        ByteBuffer got;
        while (receiver.popUnreliable(got)) received.push_back(indexOf(got));
    }

    CHECK_EQ(received.size(), static_cast<std::size_t>(1));
    CHECK_EQ(received[0], 1u);
    CHECK_EQ(receiver.stats().staleDropped, static_cast<uint64_t>(1));
    CHECK_EQ(receiver.unreliableChannel().highestDeliveredMessageId(), 1u);
}

VF_TEST(unreliableMessagesAreNeverRetransmittedWhileReliableOnesAre) {
    LinkQuality quality;
    quality.lossPercent = 50;
    quality.latencyTicks = 1;
    quality.seed = 0x5a5a5a5a5a5a5a5aull;

    LoopbackLink lossyUnreliable(quality);
    TransportConfig config;
    config.mtu = 256;
    config.retransmitTimeoutTicks = 4;
    Transport unreliableSender(lossyUnreliable, 0, config);
    Transport unreliableReceiver(lossyUnreliable, 1, config);
    for (uint32_t i = 0; i < 40; ++i) unreliableSender.sendUnreliable(messageOf(i, 24));

    std::size_t unreliableArrived = 0;
    for (uint64_t tick = 0; tick < 200; ++tick) {
        stepPair(unreliableSender, unreliableReceiver, tick);
        ByteBuffer got;
        while (unreliableReceiver.popUnreliable(got)) ++unreliableArrived;
    }
    CHECK(unreliableArrived < 40u);
    CHECK_EQ(unreliableSender.stats().retransmitted, static_cast<uint64_t>(0));

    LoopbackLink lossyReliable(quality);
    Transport reliableSender(lossyReliable, 0, config);
    Transport reliableReceiver(lossyReliable, 1, config);
    for (uint32_t i = 0; i < 40; ++i) reliableSender.sendReliable(messageOf(i, 24));

    std::size_t reliableArrived = 0;
    for (uint64_t tick = 0; tick < 600; ++tick) {
        stepPair(reliableSender, reliableReceiver, tick);
        ByteBuffer got;
        while (reliableReceiver.popReliable(got)) ++reliableArrived;
    }
    CHECK_EQ(reliableArrived, static_cast<std::size_t>(40));
    CHECK(reliableSender.stats().retransmitted > 0u);
    CHECK(reliableSender.stats().datagramsLost > 0u);
}

VF_TEST(duplicatedDatagramsAreCountedAndNeverDeliveredTwice) {
    LinkQuality quality;
    quality.duplicatePercent = 100;
    quality.latencyTicks = 1;
    quality.seed = 0x0badc0de0badc0deull;

    LoopbackLink link(quality);
    TransportConfig config;
    config.mtu = 256;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    for (uint32_t i = 0; i < 30; ++i) sender.sendReliable(messageOf(i, 60));

    std::vector<uint32_t> order;
    for (uint64_t tick = 0; tick < 200; ++tick) {
        stepPair(sender, receiver, tick);
        ByteBuffer got;
        while (receiver.popReliable(got)) order.push_back(indexOf(got));
    }

    CHECK_EQ(order.size(), static_cast<std::size_t>(30));
    for (std::size_t i = 0; i < order.size(); ++i) CHECK_EQ(order[i], static_cast<uint32_t>(i));
    CHECK(receiver.stats().duplicatesDropped > 0u);
    CHECK_EQ(link.duplicatedCount(), link.sentCount() - link.droppedCount());
    CHECK(receiver.stats().datagramsReceived > receiver.stats().duplicatesDropped);
}

VF_TEST(theSendWindowCapsHowManyDatagramsCanBeUnacknowledgedAtOnce) {
    LinkQuality quality;
    quality.latencyTicks = 6;
    quality.seed = 0x1010101020202020ull;

    LoopbackLink link(quality);
    TransportConfig config;
    config.mtu = 96;
    config.sendWindow = 4;
    config.retransmitTimeoutTicks = 40;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    for (uint32_t i = 0; i < 60; ++i) sender.sendReliable(messageOf(i, 50));

    std::size_t peakInFlight = 0;
    std::size_t arrived = 0;
    for (uint64_t tick = 0; tick < 900; ++tick) {
        stepPair(sender, receiver, tick);
        if (sender.inFlightDatagramCount() > peakInFlight)
            peakInFlight = sender.inFlightDatagramCount();
        ByteBuffer got;
        while (receiver.popReliable(got)) ++arrived;
    }
    CHECK_EQ(peakInFlight, static_cast<std::size_t>(4));
    CHECK_EQ(arrived, static_cast<std::size_t>(60));
}

VF_TEST(smoothedRoundTripTimeConvergesOnTheLinkLatency) {
    LinkQuality quality;
    quality.latencyTicks = 3;
    quality.seed = 0x9999888877776666ull;

    LoopbackLink link(quality);
    TransportConfig config;
    config.mtu = 256;
    config.retransmitTimeoutTicks = 30;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    CHECK_EQ(sender.stats().smoothedRttTicks, 0.0);
    for (uint32_t i = 0; i < 40; ++i) sender.sendReliable(messageOf(i, 20));
    for (uint64_t tick = 0; tick < 120; ++tick) {
        stepPair(sender, receiver, tick);
        ByteBuffer got;
        while (receiver.popReliable(got)) {
        }
    }
    CHECK_NEAR(sender.stats().smoothedRttTicks, 6.0, 0.001);
    CHECK_EQ(sender.stats().datagramsLost, static_cast<uint64_t>(0));
}

VF_TEST(theAckBitfieldConfirmsDatagramsWhoseOwnAckWasLost) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    quality.seed = 0x4242424242424242ull;
    LoopbackLink link(quality);

    TransportConfig config;
    config.mtu = 96;
    config.sendWindow = 32;
    config.retransmitTimeoutTicks = 1000;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    for (uint32_t i = 0; i < 12; ++i) sender.sendReliable(messageOf(i, 50));

    // Twelve datagrams leave, and the sender never pumps, so not a single ack
    // has been seen when the burst is complete.
    sender.flush(0);
    CHECK_EQ(sender.inFlightDatagramCount(), static_cast<std::size_t>(12));
    CHECK_EQ(sender.stats().datagramsSent, static_cast<uint64_t>(12));
    CHECK_EQ(sender.stats().smoothedRttTicks, 0.0);

    // A single hand-built ack: latest is sequence 11, and the history bitfield
    // covers sequences 10 down to 0. Nothing else ever reaches the sender.
    receiver.pump(1);
    Datagram carrier;
    carrier.sequence = 0;
    carrier.ackLatest = 11;
    carrier.ackHistory = 0x7ffu;
    carrier.ackValid = true;
    ByteWriter wire;
    carrier.encode(wire);
    link.send(1, wire.bytes(), 1);

    sender.pump(2);
    CHECK_EQ(sender.inFlightDatagramCount(), static_cast<std::size_t>(0));
    CHECK_EQ(sender.unackedFragmentCount(), static_cast<std::size_t>(0));
    CHECK_EQ(sender.stats().retransmitted, static_cast<uint64_t>(0));
    CHECK(sender.stats().smoothedRttTicks > 0.0);

    // The whole burst still arrived exactly once, in order.
    std::vector<uint32_t> order;
    for (uint64_t tick = 2; tick < 30; ++tick) {
        receiver.pump(tick);
        ByteBuffer got;
        while (receiver.popReliable(got)) order.push_back(indexOf(got));
    }
    CHECK_EQ(order.size(), static_cast<std::size_t>(12));
    for (std::size_t i = 0; i < order.size(); ++i) CHECK_EQ(order[i], static_cast<uint32_t>(i));
}

VF_TEST(aTransportWithNothingToSendStillAcknowledgesWithoutAnAckStorm) {
    LinkQuality quality;
    quality.latencyTicks = 1;
    quality.seed = 0x0f0f0f0f0f0f0f0full;
    LoopbackLink link(quality);

    TransportConfig config;
    config.mtu = 256;
    config.retransmitTimeoutTicks = 20;
    Transport sender(link, 0, config);
    Transport receiver(link, 1, config);

    sender.sendReliable(messageOf(0, 40));
    for (uint64_t tick = 0; tick < 30; ++tick) stepPair(sender, receiver, tick);

    ByteBuffer got;
    CHECK(receiver.popReliable(got));
    CHECK_EQ(indexOf(got), 0u);
    CHECK_EQ(sender.unackedFragmentCount(), static_cast<std::size_t>(0));
    // One payload datagram plus at most a handful of bare acks, not one per tick.
    CHECK(receiver.stats().datagramsSent <= 3u);
    CHECK(sender.stats().datagramsSent <= 3u);
}
