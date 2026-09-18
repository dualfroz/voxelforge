#include "vf/Codec.h"

#include <cstring>

namespace vf {

void ByteWriter::putU16(uint16_t v) {
    buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    buffer.push_back(static_cast<uint8_t>(v & 0xFFu));
}

void ByteWriter::putU32(uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8)
        buffer.push_back(static_cast<uint8_t>((v >> shift) & 0xFFu));
}

void ByteWriter::putU64(uint64_t v) {
    for (int shift = 56; shift >= 0; shift -= 8)
        buffer.push_back(static_cast<uint8_t>((v >> shift) & 0xFFu));
}

void ByteWriter::putVarU32(uint32_t v) {
    while (v >= 0x80u) {
        buffer.push_back(static_cast<uint8_t>(v) | 0x80u);
        v >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(v));
}

void ByteWriter::putVarU64(uint64_t v) {
    while (v >= 0x80u) {
        buffer.push_back(static_cast<uint8_t>(v) | 0x80u);
        v >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(v));
}

void ByteWriter::putVarI32(int32_t v) { putVarU32(zigzagEncode32(v)); }
void ByteWriter::putVarI64(int64_t v) { putVarU64(zigzagEncode64(v)); }

void ByteWriter::putFloat(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    putU32(bits);
}

void ByteWriter::putDouble(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    putU64(bits);
}

void ByteWriter::putBytes(const uint8_t* p, std::size_t n) {
    buffer.insert(buffer.end(), p, p + n);
}

void ByteWriter::putString(const std::string& s) {
    putVarU32(static_cast<uint32_t>(s.size()));
    buffer.insert(buffer.end(), s.begin(), s.end());
}

bool ByteReader::need(std::size_t n) {
    if (failed || cursor + n > size) {
        failed = true;
        return false;
    }
    return true;
}

bool ByteReader::getU8(uint8_t& out) {
    if (!need(1)) return false;
    out = data[cursor++];
    return true;
}

bool ByteReader::getU16(uint16_t& out) {
    if (!need(2)) return false;
    out = static_cast<uint16_t>((data[cursor] << 8) | data[cursor + 1]);
    cursor += 2;
    return true;
}

bool ByteReader::getU32(uint32_t& out) {
    if (!need(4)) return false;
    out = 0;
    for (int i = 0; i < 4; ++i) out = (out << 8) | data[cursor + static_cast<std::size_t>(i)];
    cursor += 4;
    return true;
}

bool ByteReader::getU64(uint64_t& out) {
    if (!need(8)) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) out = (out << 8) | data[cursor + static_cast<std::size_t>(i)];
    cursor += 8;
    return true;
}

bool ByteReader::getVarU32(uint32_t& out) {
    out = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        uint8_t byte;
        if (!getU8(byte)) return false;
        out |= static_cast<uint32_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return true;
    }
    failed = true;
    return false;
}

bool ByteReader::getVarU64(uint64_t& out) {
    out = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        uint8_t byte;
        if (!getU8(byte)) return false;
        out |= static_cast<uint64_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return true;
    }
    failed = true;
    return false;
}

bool ByteReader::getVarI32(int32_t& out) {
    uint32_t raw;
    if (!getVarU32(raw)) return false;
    out = zigzagDecode32(raw);
    return true;
}

bool ByteReader::getVarI64(int64_t& out) {
    uint64_t raw;
    if (!getVarU64(raw)) return false;
    out = zigzagDecode64(raw);
    return true;
}

bool ByteReader::getFloat(float& out) {
    uint32_t bits;
    if (!getU32(bits)) return false;
    std::memcpy(&out, &bits, sizeof(out));
    return true;
}

bool ByteReader::getDouble(double& out) {
    uint64_t bits;
    if (!getU64(bits)) return false;
    std::memcpy(&out, &bits, sizeof(out));
    return true;
}

bool ByteReader::getBytes(uint8_t* dst, std::size_t n) {
    if (!need(n)) return false;
    std::memcpy(dst, data + cursor, n);
    cursor += n;
    return true;
}

bool ByteReader::getString(std::string& out) {
    uint32_t len;
    if (!getVarU32(len)) return false;
    if (!need(len)) return false;
    out.assign(reinterpret_cast<const char*>(data + cursor), len);
    cursor += len;
    return true;
}

uint32_t zigzagEncode32(int32_t v) {
    return (static_cast<uint32_t>(v) << 1) ^ static_cast<uint32_t>(v >> 31);
}

int32_t zigzagDecode32(uint32_t v) {
    return static_cast<int32_t>((v >> 1) ^ (0u - (v & 1u)));
}

uint64_t zigzagEncode64(int64_t v) {
    return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63);
}

int64_t zigzagDecode64(uint64_t v) {
    return static_cast<int64_t>((v >> 1) ^ (0ull - (v & 1ull)));
}

uint32_t fnv1a32(const uint8_t* data, std::size_t size) {
    uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

uint64_t fnv1a64(const uint8_t* data, std::size_t size) {
    uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

uint32_t adler32(const uint8_t* data, std::size_t size) {
    const uint32_t modulo = 65521u;
    uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % modulo;
        b = (b + a) % modulo;
    }
    return (b << 16) | a;
}

namespace {

constexpr uint8_t rleRunFlag = 0x80;
constexpr int rleMinRun = 3;
constexpr int rleMaxRun = 130;
constexpr int rleMaxLiteral = 128;

}

ByteBuffer rleEncode(const uint8_t* data, std::size_t size) {
    ByteBuffer out;
    std::size_t i = 0;
    while (i < size) {
        std::size_t runEnd = i + 1;
        while (runEnd < size && data[runEnd] == data[i] &&
               runEnd - i < static_cast<std::size_t>(rleMaxRun))
            ++runEnd;
        std::size_t runLen = runEnd - i;
        if (runLen >= static_cast<std::size_t>(rleMinRun)) {
            out.push_back(static_cast<uint8_t>(rleRunFlag | (runLen - rleMinRun)));
            out.push_back(data[i]);
            i = runEnd;
            continue;
        }
        std::size_t litStart = i;
        while (i < size) {
            std::size_t look = i + 1;
            while (look < size && data[look] == data[i] &&
                   look - i < static_cast<std::size_t>(rleMinRun))
                ++look;
            if (look - i >= static_cast<std::size_t>(rleMinRun)) break;
            ++i;
            if (i - litStart == static_cast<std::size_t>(rleMaxLiteral)) break;
        }
        std::size_t litLen = i - litStart;
        out.push_back(static_cast<uint8_t>(litLen - 1));
        out.insert(out.end(), data + litStart, data + i);
    }
    return out;
}

bool rleDecode(const uint8_t* data, std::size_t size, ByteBuffer& out) {
    out.clear();
    std::size_t i = 0;
    while (i < size) {
        uint8_t control = data[i++];
        if (control & rleRunFlag) {
            std::size_t runLen = static_cast<std::size_t>(control & 0x7Fu) + rleMinRun;
            if (i >= size) return false;
            uint8_t value = data[i++];
            out.insert(out.end(), runLen, value);
        } else {
            std::size_t litLen = static_cast<std::size_t>(control) + 1;
            if (i + litLen > size) return false;
            out.insert(out.end(), data + i, data + i + litLen);
            i += litLen;
        }
    }
    return true;
}

namespace {

constexpr int lzMinMatch = 4;
constexpr int lzHashBits = 15;
constexpr int lzHashSize = 1 << lzHashBits;
constexpr int lzMaxChain = 128;
constexpr uint32_t lzMaxOffset = 1u << 20;

uint32_t hash4(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return (v * 2654435761u) >> (32 - lzHashBits);
}

void putVarU32Raw(ByteBuffer& out, uint32_t v) {
    while (v >= 0x80u) {
        out.push_back(static_cast<uint8_t>(v) | 0x80u);
        v >>= 7;
    }
    out.push_back(static_cast<uint8_t>(v));
}

bool getVarU32Raw(const uint8_t* data, std::size_t size, std::size_t& pos, uint32_t& out) {
    out = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        if (pos >= size) return false;
        uint8_t byte = data[pos++];
        out |= static_cast<uint32_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return true;
    }
    return false;
}

}

ByteBuffer lz77Compress(const uint8_t* data, std::size_t size) {
    ByteBuffer out;
    putVarU32Raw(out, static_cast<uint32_t>(size));
    if (size == 0) return out;

    std::vector<int32_t> head(lzHashSize, -1);
    std::vector<int32_t> prev(size, -1);

    std::size_t pos = 0;
    std::size_t flagIndex = out.size();
    out.push_back(0);
    int flagBit = 0;

    auto insert = [&](std::size_t at) {
        if (at + lzMinMatch <= size) {
            uint32_t h = hash4(data + at);
            prev[at] = head[h];
            head[h] = static_cast<int32_t>(at);
        }
    };

    while (pos < size) {
        std::size_t bestLen = 0;
        std::size_t bestOffset = 0;
        if (pos + lzMinMatch <= size) {
            uint32_t h = hash4(data + pos);
            int32_t candidate = head[h];
            int chain = 0;
            std::size_t maxLen = size - pos;
            while (candidate >= 0 && chain < lzMaxChain) {
                std::size_t cand = static_cast<std::size_t>(candidate);
                if (pos - cand > lzMaxOffset) break;
                std::size_t len = 0;
                while (len < maxLen && data[cand + len] == data[pos + len]) ++len;
                if (len > bestLen) {
                    bestLen = len;
                    bestOffset = pos - cand;
                    if (len == maxLen) break;
                }
                candidate = prev[cand];
                ++chain;
            }
        }

        if (bestLen >= static_cast<std::size_t>(lzMinMatch)) {
            out[flagIndex] = static_cast<uint8_t>(out[flagIndex] | (1u << flagBit));
            putVarU32Raw(out, static_cast<uint32_t>(bestOffset));
            putVarU32Raw(out, static_cast<uint32_t>(bestLen - lzMinMatch));
            std::size_t end = pos + bestLen;
            while (pos < end) {
                insert(pos);
                ++pos;
            }
        } else {
            out.push_back(data[pos]);
            insert(pos);
            ++pos;
        }

        if (++flagBit == 8) {
            flagBit = 0;
            if (pos < size) {
                flagIndex = out.size();
                out.push_back(0);
            }
        }
    }
    return out;
}

bool lz77Decompress(const uint8_t* data, std::size_t size, ByteBuffer& out) {
    out.clear();
    std::size_t pos = 0;
    uint32_t rawLen;
    if (!getVarU32Raw(data, size, pos, rawLen)) return false;
    out.reserve(rawLen);

    while (out.size() < rawLen) {
        if (pos >= size) return false;
        uint8_t flag = data[pos++];
        for (int bit = 0; bit < 8 && out.size() < rawLen; ++bit) {
            if (flag & (1u << bit)) {
                uint32_t offset, extra;
                if (!getVarU32Raw(data, size, pos, offset)) return false;
                if (!getVarU32Raw(data, size, pos, extra)) return false;
                std::size_t len = static_cast<std::size_t>(extra) + lzMinMatch;
                if (offset == 0 || offset > out.size()) return false;
                if (out.size() + len > rawLen) return false;
                std::size_t from = out.size() - offset;
                for (std::size_t i = 0; i < len; ++i) out.push_back(out[from + i]);
            } else {
                if (pos >= size) return false;
                out.push_back(data[pos++]);
            }
        }
    }
    return out.size() == rawLen;
}

ByteBuffer compress(const uint8_t* data, std::size_t size) {
    ByteBuffer rle = rleEncode(data, size);
    ByteBuffer lz = lz77Compress(data, size);

    CodecMethod method = CodecMethod::Store;
    const ByteBuffer* payload = nullptr;
    std::size_t best = size;
    if (rle.size() < best) {
        best = rle.size();
        method = CodecMethod::Rle;
        payload = &rle;
    }
    if (lz.size() < best) {
        method = CodecMethod::Lz77;
        payload = &lz;
    }

    ByteWriter head;
    head.putU8(static_cast<uint8_t>(method));
    head.putVarU32(static_cast<uint32_t>(size));
    head.putU32(fnv1a32(data, size));

    ByteBuffer out = head.bytes();
    if (method == CodecMethod::Store)
        out.insert(out.end(), data, data + size);
    else
        out.insert(out.end(), payload->begin(), payload->end());
    return out;
}

ByteBuffer compress(const ByteBuffer& in) { return compress(in.data(), in.size()); }

bool decompress(const uint8_t* data, std::size_t size, ByteBuffer& out) {
    ByteReader reader(data, size);
    uint8_t rawMethod;
    uint32_t rawLen;
    uint32_t checksum;
    if (!reader.getU8(rawMethod)) return false;
    if (!reader.getVarU32(rawLen)) return false;
    if (!reader.getU32(checksum)) return false;

    const uint8_t* payload = data + reader.position();
    std::size_t payloadSize = reader.remaining();

    switch (static_cast<CodecMethod>(rawMethod)) {
        case CodecMethod::Store:
            if (payloadSize != rawLen) return false;
            out.assign(payload, payload + payloadSize);
            break;
        case CodecMethod::Rle:
            if (!rleDecode(payload, payloadSize, out)) return false;
            break;
        case CodecMethod::Lz77:
            if (!lz77Decompress(payload, payloadSize, out)) return false;
            break;
        default:
            return false;
    }

    if (out.size() != rawLen) return false;
    return fnv1a32(out.data(), out.size()) == checksum;
}

}
