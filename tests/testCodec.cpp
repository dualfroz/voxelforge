#include "testHarness.h"
#include "vf/Codec.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace vf;

namespace {

struct Lcg {
    uint64_t state;
    explicit Lcg(uint64_t seed) : state(seed) {}
    uint32_t next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(state >> 32);
    }
    uint8_t byte(int modulo) { return static_cast<uint8_t>(next() % static_cast<uint32_t>(modulo)); }
};

ByteBuffer roundTripCodec(const ByteBuffer& input) {
    ByteBuffer packed = compress(input);
    ByteBuffer restored;
    bool ok = decompress(packed.data(), packed.size(), restored);
    CHECK(ok);
    return restored;
}

}

VF_TEST(codecVarintUnsignedRoundTrip) {
    const uint64_t samples[] = {0, 1, 127, 128, 255, 300, 16383, 16384,
                                1u << 21, 0xFFFFFFFFull, 0x1234567890ABCDEFull,
                                0xFFFFFFFFFFFFFFFFull};
    for (uint64_t v : samples) {
        ByteWriter w;
        w.putVarU64(v);
        ByteReader r(w.bytes().data(), w.bytes().size());
        uint64_t got = 0;
        CHECK(r.getVarU64(got));
        CHECK_EQ(got, v);
        CHECK(r.exhausted());
    }
}

VF_TEST(codecZigzagRoundTrip) {
    const int64_t samples[] = {0, -1, 1, -2, 2, 63, -64, 2147483647LL,
                               -2147483648LL, 9223372036854775807LL,
                               -9223372036854775807LL - 1};
    for (int64_t v : samples) {
        ByteWriter w;
        w.putVarI64(v);
        ByteReader r(w.bytes().data(), w.bytes().size());
        int64_t got = 0;
        CHECK(r.getVarI64(got));
        CHECK_EQ(got, v);
    }
    for (int32_t v = -5000; v <= 5000; v += 7) {
        CHECK_EQ(zigzagDecode32(zigzagEncode32(v)), v);
    }
}

VF_TEST(codecMixedWriterReader) {
    ByteWriter w;
    w.putU8(0xAB);
    w.putU16(0xBEEF);
    w.putU32(0xDEADBEEF);
    w.putU64(0x0123456789ABCDEFull);
    w.putFloat(3.14159f);
    w.putDouble(2.718281828459045);
    w.putString("voxelforge");
    w.putVarI32(-123456);

    ByteReader r(w.bytes().data(), w.bytes().size());
    uint8_t a;
    uint16_t b;
    uint32_t c;
    uint64_t d;
    float e;
    double f;
    std::string g;
    int32_t h;
    CHECK(r.getU8(a) && a == 0xAB);
    CHECK(r.getU16(b) && b == 0xBEEF);
    CHECK(r.getU32(c) && c == 0xDEADBEEF);
    CHECK(r.getU64(d) && d == 0x0123456789ABCDEFull);
    CHECK(r.getFloat(e) && e == 3.14159f);
    CHECK(r.getDouble(f) && f == 2.718281828459045);
    CHECK(r.getString(g) && g == "voxelforge");
    CHECK(r.getVarI32(h) && h == -123456);
    CHECK(r.exhausted());
}

VF_TEST(codecReaderRejectsTruncated) {
    ByteWriter w;
    w.putU32(42);
    ByteReader r(w.bytes().data(), w.bytes().size());
    uint64_t big;
    CHECK(!r.getU64(big));
    CHECK(!r.ok());
}

VF_TEST(rleCompressesRuns) {
    ByteBuffer input(4096, 0x7);
    ByteBuffer encoded = rleEncode(input.data(), input.size());
    CHECK(encoded.size() < input.size() / 8);
    ByteBuffer decoded;
    CHECK(rleDecode(encoded.data(), encoded.size(), decoded));
    CHECK(decoded == input);
}

VF_TEST(rleHandlesMixedAndLiterals) {
    Lcg rng(0x1234);
    ByteBuffer input;
    for (int block = 0; block < 200; ++block) {
        if (block % 3 == 0) {
            uint8_t v = rng.byte(256);
            int len = 3 + static_cast<int>(rng.byte(200));
            input.insert(input.end(), static_cast<std::size_t>(len), v);
        } else {
            int len = 1 + static_cast<int>(rng.byte(64));
            for (int i = 0; i < len; ++i) input.push_back(rng.byte(256));
        }
    }
    ByteBuffer encoded = rleEncode(input.data(), input.size());
    ByteBuffer decoded;
    CHECK(rleDecode(encoded.data(), encoded.size(), decoded));
    CHECK(decoded == input);
}

VF_TEST(rleEmptyRoundTrip) {
    ByteBuffer encoded = rleEncode(nullptr, 0);
    ByteBuffer decoded;
    CHECK(rleDecode(encoded.data(), encoded.size(), decoded));
    CHECK(decoded.empty());
}

VF_TEST(lz77CompressesRepetition) {
    ByteBuffer input;
    const char* phrase = "the quick brown fox jumps over the lazy dog. ";
    for (int i = 0; i < 400; ++i)
        for (const char* p = phrase; *p; ++p) input.push_back(static_cast<uint8_t>(*p));

    ByteBuffer encoded = lz77Compress(input.data(), input.size());
    CHECK(encoded.size() < input.size() / 4);
    ByteBuffer decoded;
    CHECK(lz77Decompress(encoded.data(), encoded.size(), decoded));
    CHECK(decoded == input);
}

VF_TEST(lz77HandlesOverlappingRuns) {
    ByteBuffer input(1000, 0x42);
    input[0] = 0x1;
    ByteBuffer encoded = lz77Compress(input.data(), input.size());
    ByteBuffer decoded;
    CHECK(lz77Decompress(encoded.data(), encoded.size(), decoded));
    CHECK(decoded == input);
}

VF_TEST(lz77RandomDataRoundTrip) {
    Lcg rng(0xBEEF);
    for (int trial = 0; trial < 20; ++trial) {
        ByteBuffer input;
        int len = static_cast<int>(rng.next() % 5000);
        for (int i = 0; i < len; ++i) input.push_back(rng.byte(256));
        ByteBuffer encoded = lz77Compress(input.data(), input.size());
        ByteBuffer decoded;
        CHECK(lz77Decompress(encoded.data(), encoded.size(), decoded));
        CHECK(decoded == input);
    }
}

VF_TEST(lz77EmptyAndTiny) {
    for (std::size_t n = 0; n < 8; ++n) {
        ByteBuffer input(n, static_cast<uint8_t>(n + 1));
        ByteBuffer encoded = lz77Compress(input.data(), input.size());
        ByteBuffer decoded;
        CHECK(lz77Decompress(encoded.data(), encoded.size(), decoded));
        CHECK(decoded == input);
    }
}

VF_TEST(lz77RejectsCorruptStream) {
    ByteBuffer input(500, 0x9);
    ByteBuffer encoded = lz77Compress(input.data(), input.size());
    ByteBuffer decoded;
    CHECK(lz77Decompress(encoded.data(), encoded.size(), decoded));
    ByteBuffer truncated(encoded.begin(), encoded.begin() + encoded.size() / 2);
    ByteBuffer partial;
    bool ok = lz77Decompress(truncated.data(), truncated.size(), partial);
    CHECK(!(ok && partial == input));
}

VF_TEST(codecPicksBestAndRoundTrips) {
    ByteBuffer runs(8000, 0x11);
    ByteBuffer restored = roundTripCodec(runs);
    CHECK(restored == runs);
    ByteBuffer packed = compress(runs);
    CHECK(static_cast<CodecMethod>(packed[0]) != CodecMethod::Store);

    Lcg rng(0x55AA);
    ByteBuffer noise;
    for (int i = 0; i < 4000; ++i) noise.push_back(rng.byte(256));
    ByteBuffer packedNoise = compress(noise);
    CHECK(roundTripCodec(noise) == noise);
    CHECK(packedNoise.size() <= noise.size() + 8);
}

VF_TEST(codecDetectsChecksumTamper) {
    ByteBuffer input;
    for (int i = 0; i < 300; ++i) input.push_back(static_cast<uint8_t>(i * 7));
    ByteBuffer packed = compress(input);
    packed.back() ^= 0xFF;
    ByteBuffer out;
    bool ok = decompress(packed.data(), packed.size(), out);
    CHECK(!(ok && out == input));
}

VF_TEST(checksumFunctionsAreStableAndDistinct) {
    const uint8_t a[] = {1, 2, 3, 4, 5};
    const uint8_t b[] = {1, 2, 3, 4, 6};
    CHECK(fnv1a32(a, 5) == fnv1a32(a, 5));
    CHECK(fnv1a32(a, 5) != fnv1a32(b, 5));
    CHECK(fnv1a64(a, 5) != fnv1a64(b, 5));
    CHECK(adler32(a, 5) != adler32(b, 5));
    CHECK_EQ(adler32(nullptr, 0), 1u);
}
