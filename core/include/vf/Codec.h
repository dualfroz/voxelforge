#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vf {

using ByteBuffer = std::vector<uint8_t>;

// Growable byte writer with big-endian fixed ints plus LEB128 varints and
// zigzag signed varints. Shared codec surface for the region and wire formats.
class ByteWriter {
public:
    void putU8(uint8_t v) { buffer.push_back(v); }
    void putU16(uint16_t v);
    void putU32(uint32_t v);
    void putU64(uint64_t v);

    void putVarU32(uint32_t v);
    void putVarU64(uint64_t v);
    void putVarI32(int32_t v);
    void putVarI64(int64_t v);

    void putFloat(float v);
    void putDouble(double v);
    void putBytes(const uint8_t* p, std::size_t n);
    void putString(const std::string& s);

    std::size_t size() const { return buffer.size(); }
    const ByteBuffer& bytes() const { return buffer; }
    ByteBuffer& bytes() { return buffer; }

    // Reserve a placeholder byte and return its index so a header can be
    // patched once the trailing payload length is known.
    std::size_t reserveU8() {
        std::size_t at = buffer.size();
        buffer.push_back(0);
        return at;
    }
    void patchU8(std::size_t at, uint8_t v) { buffer[at] = v; }

private:
    ByteBuffer buffer;
};

// Cursor over an immutable byte span. Every read is bounds checked and sets a
// sticky failure flag; callers check ok() at the end rather than each read.
class ByteReader {
public:
    ByteReader(const uint8_t* data, std::size_t size) : data(data), size(size) {}

    bool getU8(uint8_t& out);
    bool getU16(uint16_t& out);
    bool getU32(uint32_t& out);
    bool getU64(uint64_t& out);

    bool getVarU32(uint32_t& out);
    bool getVarU64(uint64_t& out);
    bool getVarI32(int32_t& out);
    bool getVarI64(int64_t& out);

    bool getFloat(float& out);
    bool getDouble(double& out);
    bool getBytes(uint8_t* dst, std::size_t n);
    bool getString(std::string& out);

    std::size_t position() const { return cursor; }
    std::size_t remaining() const { return size - cursor; }
    bool exhausted() const { return cursor >= size; }
    bool ok() const { return !failed; }

private:
    bool need(std::size_t n);

    const uint8_t* data;
    std::size_t size;
    std::size_t cursor = 0;
    bool failed = false;
};

uint32_t zigzagEncode32(int32_t v);
int32_t zigzagDecode32(uint32_t v);
uint64_t zigzagEncode64(int64_t v);
int64_t zigzagDecode64(uint64_t v);

uint32_t fnv1a32(const uint8_t* data, std::size_t size);
uint64_t fnv1a64(const uint8_t* data, std::size_t size);
uint32_t adler32(const uint8_t* data, std::size_t size);

// Byte-oriented run-length codec. Literal tokens carry 1..128 bytes, run
// tokens carry a value repeated 3..130 times, so it never expands input by
// more than one byte per 128 literals.
ByteBuffer rleEncode(const uint8_t* data, std::size_t size);
bool rleDecode(const uint8_t* data, std::size_t size, ByteBuffer& out);

// LZ77 with a hash-chain match finder and a flag-byte token stream (literal or
// varint offset+length match). The raw length is embedded so decode stops
// exactly, and it reconstructs the input byte for byte.
ByteBuffer lz77Compress(const uint8_t* data, std::size_t size);
bool lz77Decompress(const uint8_t* data, std::size_t size, ByteBuffer& out);

enum class CodecMethod : uint8_t { Store = 0, Rle = 1, Lz77 = 2 };

// Frames a payload: method + varint raw length + fnv1a32 of the raw bytes +
// the smallest of {store, rle, lz77}. decompress verifies length and checksum.
ByteBuffer compress(const uint8_t* data, std::size_t size);
ByteBuffer compress(const ByteBuffer& in);
bool decompress(const uint8_t* data, std::size_t size, ByteBuffer& out);

} // namespace vf
