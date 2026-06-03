#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Chunk.h"

namespace vf {

enum class NbtType : uint8_t {
    End = 0,
    Byte = 1,
    Short = 2,
    Int = 3,
    Long = 4,
    Float = 5,
    Double = 6,
    String = 7,
    ByteArray = 8,
    IntArray = 9,
    List = 10,
    Compound = 11,
};

class NbtTag;

using NbtList = std::vector<NbtTag>;
using NbtCompound = std::vector<std::pair<std::string, NbtTag>>;

// A single NBT-like tag: one of the scalar/array/container payloads above.
// Containers (List, Compound) hold NbtTag by value, so a whole tree round
// trips as one self-contained object.
class NbtTag {
public:
    NbtTag() = default;

    static NbtTag makeByte(int8_t v);
    static NbtTag makeShort(int16_t v);
    static NbtTag makeInt(int32_t v);
    static NbtTag makeLong(int64_t v);
    static NbtTag makeFloat(float v);
    static NbtTag makeDouble(double v);
    static NbtTag makeString(std::string v);
    static NbtTag makeByteArray(std::vector<int8_t> v);
    static NbtTag makeIntArray(std::vector<int32_t> v);
    static NbtTag makeList(NbtType elementType);
    static NbtTag makeCompound();

    NbtType kind() const { return type; }
    NbtType listElementType() const { return elementType; }

    int8_t asByte() const { return static_cast<int8_t>(integerValue); }
    int16_t asShort() const { return static_cast<int16_t>(integerValue); }
    int32_t asInt() const { return static_cast<int32_t>(integerValue); }
    int64_t asLong() const { return integerValue; }
    float asFloat() const { return static_cast<float>(floatingValue); }
    double asDouble() const { return floatingValue; }
    const std::string& asString() const { return stringValue; }
    const std::vector<int8_t>& asByteArray() const { return byteArrayValue; }
    const std::vector<int32_t>& asIntArray() const { return intArrayValue; }

    NbtList& asList() { return listValue; }
    const NbtList& asList() const { return listValue; }
    void listAppend(NbtTag tag) { listValue.push_back(std::move(tag)); }

    NbtCompound& asCompound() { return compoundValue; }
    const NbtCompound& asCompound() const { return compoundValue; }
    void put(const std::string& key, NbtTag tag);
    const NbtTag* find(const std::string& key) const;

private:
    NbtType type = NbtType::End;
    NbtType elementType = NbtType::End;
    int64_t integerValue = 0;
    double floatingValue = 0.0;
    std::string stringValue;
    std::vector<int8_t> byteArrayValue;
    std::vector<int32_t> intArrayValue;
    NbtList listValue;
    NbtCompound compoundValue;
};

// Big-endian binary encoding: tagType(1) + nameLen(u16) + name + payload,
// recursively. Compounds are terminated by an End tag; lists carry a shared
// element type and a u32 count instead of per-element headers.
std::vector<uint8_t> serializeNbt(const std::string& rootName, const NbtTag& root);
bool deserializeNbt(const uint8_t* data, std::size_t size, std::string& rootName, NbtTag& root);

// Palette-based section encoding (format v4): the distinct blocks used by a
// chunkVolume-sized section, followed by fixed-width bit-packed indices into
// that palette (LSB-first within each byte).
struct NbtPaletteSection {
    std::vector<Block> palette;
    std::vector<uint8_t> packedIndices;
    int bitsPerIndex = 0;
};

NbtPaletteSection encodeSectionPalette(const std::array<Block, chunkVolume>& blocks);
void decodeSectionPalette(const NbtPaletteSection& section, std::array<Block, chunkVolume>& outBlocks);

} // namespace vf
