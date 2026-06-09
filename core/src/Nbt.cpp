#include "vf/Nbt.h"

#include <cstring>

namespace vf {

NbtTag NbtTag::makeByte(int8_t v) {
    NbtTag tag;
    tag.type = NbtType::Byte;
    tag.integerValue = v;
    return tag;
}

NbtTag NbtTag::makeShort(int16_t v) {
    NbtTag tag;
    tag.type = NbtType::Short;
    tag.integerValue = v;
    return tag;
}

NbtTag NbtTag::makeInt(int32_t v) {
    NbtTag tag;
    tag.type = NbtType::Int;
    tag.integerValue = v;
    return tag;
}

NbtTag NbtTag::makeLong(int64_t v) {
    NbtTag tag;
    tag.type = NbtType::Long;
    tag.integerValue = v;
    return tag;
}

NbtTag NbtTag::makeFloat(float v) {
    NbtTag tag;
    tag.type = NbtType::Float;
    tag.floatingValue = v;
    return tag;
}

NbtTag NbtTag::makeDouble(double v) {
    NbtTag tag;
    tag.type = NbtType::Double;
    tag.floatingValue = v;
    return tag;
}

NbtTag NbtTag::makeString(std::string v) {
    NbtTag tag;
    tag.type = NbtType::String;
    tag.stringValue = std::move(v);
    return tag;
}

NbtTag NbtTag::makeByteArray(std::vector<int8_t> v) {
    NbtTag tag;
    tag.type = NbtType::ByteArray;
    tag.byteArrayValue = std::move(v);
    return tag;
}

NbtTag NbtTag::makeIntArray(std::vector<int32_t> v) {
    NbtTag tag;
    tag.type = NbtType::IntArray;
    tag.intArrayValue = std::move(v);
    return tag;
}

NbtTag NbtTag::makeList(NbtType elementType) {
    NbtTag tag;
    tag.type = NbtType::List;
    tag.elementType = elementType;
    return tag;
}

NbtTag NbtTag::makeCompound() {
    NbtTag tag;
    tag.type = NbtType::Compound;
    return tag;
}

void NbtTag::put(const std::string& key, NbtTag tag) {
    for (auto& entry : compoundValue) {
        if (entry.first == key) {
            entry.second = std::move(tag);
            return;
        }
    }
    compoundValue.emplace_back(key, std::move(tag));
}

const NbtTag* NbtTag::find(const std::string& key) const {
    for (const auto& entry : compoundValue) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

namespace {

void writeU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void writeU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
}

void writeU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>((v >> shift) & 0xFFu));
}

void writeU64(std::vector<uint8_t>& out, uint64_t v) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<uint8_t>((v >> shift) & 0xFFu));
}

void writeString(std::vector<uint8_t>& out, const std::string& s) {
    writeU16(out, static_cast<uint16_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

void writePayload(std::vector<uint8_t>& out, const NbtTag& tag);

void writeNamedTag(std::vector<uint8_t>& out, const std::string& name, const NbtTag& tag) {
    out.push_back(static_cast<uint8_t>(tag.kind()));
    writeString(out, name);
    writePayload(out, tag);
}

void writePayload(std::vector<uint8_t>& out, const NbtTag& tag) {
    switch (tag.kind()) {
        case NbtType::Byte:
            writeU8(out, static_cast<uint8_t>(tag.asByte()));
            break;
        case NbtType::Short:
            writeU16(out, static_cast<uint16_t>(tag.asShort()));
            break;
        case NbtType::Int:
            writeU32(out, static_cast<uint32_t>(tag.asInt()));
            break;
        case NbtType::Long:
            writeU64(out, static_cast<uint64_t>(tag.asLong()));
            break;
        case NbtType::Float: {
            float f = tag.asFloat();
            uint32_t bits;
            std::memcpy(&bits, &f, sizeof(bits));
            writeU32(out, bits);
            break;
        }
        case NbtType::Double: {
            double d = tag.asDouble();
            uint64_t bits;
            std::memcpy(&bits, &d, sizeof(bits));
            writeU64(out, bits);
            break;
        }
        case NbtType::String:
            writeString(out, tag.asString());
            break;
        case NbtType::ByteArray: {
            const std::vector<int8_t>& arr = tag.asByteArray();
            writeU32(out, static_cast<uint32_t>(arr.size()));
            for (int8_t v : arr) writeU8(out, static_cast<uint8_t>(v));
            break;
        }
        case NbtType::IntArray: {
            const std::vector<int32_t>& arr = tag.asIntArray();
            writeU32(out, static_cast<uint32_t>(arr.size()));
            for (int32_t v : arr) writeU32(out, static_cast<uint32_t>(v));
            break;
        }
        case NbtType::List: {
            out.push_back(static_cast<uint8_t>(tag.listElementType()));
            const NbtList& list = tag.asList();
            writeU32(out, static_cast<uint32_t>(list.size()));
            for (const NbtTag& element : list) writePayload(out, element);
            break;
        }
        case NbtType::Compound: {
            for (const auto& entry : tag.asCompound()) writeNamedTag(out, entry.first, entry.second);
            out.push_back(static_cast<uint8_t>(NbtType::End));
            break;
        }
        case NbtType::End:
            break;
    }
}

struct NbtReader {
    const uint8_t* data;
    std::size_t size;
    std::size_t cursor = 0;

    bool remaining(std::size_t n) const { return cursor + n <= size; }

    bool readU8(uint8_t& out) {
        if (!remaining(1)) return false;
        out = data[cursor++];
        return true;
    }

    bool readU16(uint16_t& out) {
        if (!remaining(2)) return false;
        out = static_cast<uint16_t>((data[cursor] << 8) | data[cursor + 1]);
        cursor += 2;
        return true;
    }

    bool readU32(uint32_t& out) {
        if (!remaining(4)) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) out = (out << 8) | data[cursor + static_cast<std::size_t>(i)];
        cursor += 4;
        return true;
    }

    bool readU64(uint64_t& out) {
        if (!remaining(8)) return false;
        out = 0;
        for (int i = 0; i < 8; ++i) out = (out << 8) | data[cursor + static_cast<std::size_t>(i)];
        cursor += 8;
        return true;
    }

    bool readString(std::string& out) {
        uint16_t len = 0;
        if (!readU16(len)) return false;
        if (!remaining(len)) return false;
        out.assign(reinterpret_cast<const char*>(data + cursor), len);
        cursor += len;
        return true;
    }
};

bool readPayload(NbtReader& reader, NbtType type, NbtTag& out);

bool readNamedTag(NbtReader& reader, std::string& name, NbtTag& out) {
    uint8_t rawType = 0;
    if (!reader.readU8(rawType)) return false;
    if (!reader.readString(name)) return false;
    return readPayload(reader, static_cast<NbtType>(rawType), out);
}

bool readPayload(NbtReader& reader, NbtType type, NbtTag& out) {
    switch (type) {
        case NbtType::Byte: {
            uint8_t v;
            if (!reader.readU8(v)) return false;
            out = NbtTag::makeByte(static_cast<int8_t>(v));
            return true;
        }
        case NbtType::Short: {
            uint16_t v;
            if (!reader.readU16(v)) return false;
            out = NbtTag::makeShort(static_cast<int16_t>(v));
            return true;
        }
        case NbtType::Int: {
            uint32_t v;
            if (!reader.readU32(v)) return false;
            out = NbtTag::makeInt(static_cast<int32_t>(v));
            return true;
        }
        case NbtType::Long: {
            uint64_t v;
            if (!reader.readU64(v)) return false;
            out = NbtTag::makeLong(static_cast<int64_t>(v));
            return true;
        }
        case NbtType::Float: {
            uint32_t bits;
            if (!reader.readU32(bits)) return false;
            float f;
            std::memcpy(&f, &bits, sizeof(f));
            out = NbtTag::makeFloat(f);
            return true;
        }
        case NbtType::Double: {
            uint64_t bits;
            if (!reader.readU64(bits)) return false;
            double d;
            std::memcpy(&d, &bits, sizeof(d));
            out = NbtTag::makeDouble(d);
            return true;
        }
        case NbtType::String: {
            std::string s;
            if (!reader.readString(s)) return false;
            out = NbtTag::makeString(std::move(s));
            return true;
        }
        case NbtType::ByteArray: {
            uint32_t count;
            if (!reader.readU32(count)) return false;
            std::vector<int8_t> arr;
            arr.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                uint8_t v;
                if (!reader.readU8(v)) return false;
                arr.push_back(static_cast<int8_t>(v));
            }
            out = NbtTag::makeByteArray(std::move(arr));
            return true;
        }
        case NbtType::IntArray: {
            uint32_t count;
            if (!reader.readU32(count)) return false;
            std::vector<int32_t> arr;
            arr.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t v;
                if (!reader.readU32(v)) return false;
                arr.push_back(static_cast<int32_t>(v));
            }
            out = NbtTag::makeIntArray(std::move(arr));
            return true;
        }
        case NbtType::List: {
            uint8_t rawElementType;
            if (!reader.readU8(rawElementType)) return false;
            NbtType elementType = static_cast<NbtType>(rawElementType);
            uint32_t count;
            if (!reader.readU32(count)) return false;
            out = NbtTag::makeList(elementType);
            for (uint32_t i = 0; i < count; ++i) {
                NbtTag element;
                if (!readPayload(reader, elementType, element)) return false;
                out.listAppend(std::move(element));
            }
            return true;
        }
        case NbtType::Compound: {
            out = NbtTag::makeCompound();
            for (;;) {
                uint8_t rawChildType;
                if (!reader.readU8(rawChildType)) return false;
                NbtType childType = static_cast<NbtType>(rawChildType);
                if (childType == NbtType::End) return true;
                std::string childName;
                if (!reader.readString(childName)) return false;
                NbtTag child;
                if (!readPayload(reader, childType, child)) return false;
                out.put(childName, std::move(child));
            }
        }
        case NbtType::End:
            return true;
    }
    return false;
}

} // namespace

std::vector<uint8_t> serializeNbt(const std::string& rootName, const NbtTag& root) {
    std::vector<uint8_t> out;
    writeNamedTag(out, rootName, root);
    return out;
}

bool deserializeNbt(const uint8_t* data, std::size_t size, std::string& rootName, NbtTag& root) {
    NbtReader reader{data, size};
    return readNamedTag(reader, rootName, root);
}

NbtPaletteSection encodeSectionPalette(const std::array<Block, chunkVolume>& blocks) {
    NbtPaletteSection section;
    std::array<int, static_cast<std::size_t>(Block::Count)> indexOf;
    indexOf.fill(-1);

    std::vector<int> indices;
    indices.reserve(blocks.size());
    for (Block block : blocks) {
        int& slot = indexOf[static_cast<std::size_t>(block)];
        if (slot < 0) {
            slot = static_cast<int>(section.palette.size());
            section.palette.push_back(block);
        }
        indices.push_back(slot);
    }

    int bits = 1;
    while ((1 << bits) < static_cast<int>(section.palette.size())) ++bits;
    section.bitsPerIndex = bits;

    std::size_t totalBits = static_cast<std::size_t>(bits) * indices.size();
    section.packedIndices.assign((totalBits + 7) / 8, 0);

    std::size_t bitCursor = 0;
    for (int index : indices) {
        for (int b = 0; b < bits; ++b) {
            if ((index >> b) & 1) {
                std::size_t byteIndex = bitCursor / 8;
                int bitOffset = static_cast<int>(bitCursor % 8);
                section.packedIndices[byteIndex] |= static_cast<uint8_t>(1u << bitOffset);
            }
            ++bitCursor;
        }
    }

    return section;
}

void decodeSectionPalette(const NbtPaletteSection& section, std::array<Block, chunkVolume>& outBlocks) {
    std::size_t bitCursor = 0;
    for (std::size_t i = 0; i < outBlocks.size(); ++i) {
        int index = 0;
        for (int b = 0; b < section.bitsPerIndex; ++b) {
            std::size_t byteIndex = bitCursor / 8;
            int bitOffset = static_cast<int>(bitCursor % 8);
            if (section.packedIndices[byteIndex] & static_cast<uint8_t>(1u << bitOffset))
                index |= (1 << b);
            ++bitCursor;
        }
        outBlocks[i] = section.palette[static_cast<std::size_t>(index)];
    }
}

} // namespace vf
