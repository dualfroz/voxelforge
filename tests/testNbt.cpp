#include "testHarness.h"
#include "vf/Nbt.h"

#include <array>

using namespace vf;

namespace {

bool roundTrip(const std::string& name, const NbtTag& tag, std::string& outName, NbtTag& outTag) {
    std::vector<uint8_t> bytes = serializeNbt(name, tag);
    return deserializeNbt(bytes.data(), bytes.size(), outName, outTag);
}

} // namespace

VF_TEST(nbtScalarRoundTripPreservesValueAndType) {
    std::string name;
    NbtTag tag;

    CHECK(roundTrip("byteField", NbtTag::makeByte(-42), name, tag));
    CHECK_EQ(name, std::string("byteField"));
    CHECK(tag.kind() == NbtType::Byte);
    CHECK_EQ(tag.asByte(), static_cast<int8_t>(-42));

    CHECK(roundTrip("shortField", NbtTag::makeShort(-12345), name, tag));
    CHECK(tag.kind() == NbtType::Short);
    CHECK_EQ(tag.asShort(), static_cast<int16_t>(-12345));

    CHECK(roundTrip("intField", NbtTag::makeInt(-2000000000), name, tag));
    CHECK(tag.kind() == NbtType::Int);
    CHECK_EQ(tag.asInt(), -2000000000);

    CHECK(roundTrip("longField", NbtTag::makeLong(-9000000000000000000LL), name, tag));
    CHECK(tag.kind() == NbtType::Long);
    CHECK_EQ(tag.asLong(), -9000000000000000000LL);

    CHECK(roundTrip("floatField", NbtTag::makeFloat(3.14159f), name, tag));
    CHECK(tag.kind() == NbtType::Float);
    CHECK_NEAR(tag.asFloat(), 3.14159f, 1e-6f);

    CHECK(roundTrip("doubleField", NbtTag::makeDouble(-2.718281828), name, tag));
    CHECK(tag.kind() == NbtType::Double);
    CHECK_NEAR(tag.asDouble(), -2.718281828, 1e-12);

    CHECK(roundTrip("stringField", NbtTag::makeString("voxelforge"), name, tag));
    CHECK(tag.kind() == NbtType::String);
    CHECK_EQ(tag.asString(), std::string("voxelforge"));
}

VF_TEST(nbtEmptyStringRoundTrips) {
    std::string name;
    NbtTag tag;
    CHECK(roundTrip("empty", NbtTag::makeString(""), name, tag));
    CHECK_EQ(tag.asString(), std::string(""));
}

VF_TEST(nbtByteArrayRoundTripsExactBytes) {
    std::vector<int8_t> values = {1, -1, 0, 127, -128, 64, -64};
    std::string name;
    NbtTag tag;
    CHECK(roundTrip("bytes", NbtTag::makeByteArray(values), name, tag));
    CHECK(tag.kind() == NbtType::ByteArray);
    const std::vector<int8_t>& out = tag.asByteArray();
    CHECK_EQ(out.size(), values.size());
    for (std::size_t i = 0; i < values.size(); ++i) CHECK_EQ(out[i], values[i]);
}

VF_TEST(nbtIntArrayRoundTripsExactValues) {
    std::vector<int32_t> values = {0, 1, -1, 2147483647, -2147483647 - 1, 12345};
    std::string name;
    NbtTag tag;
    CHECK(roundTrip("ints", NbtTag::makeIntArray(values), name, tag));
    CHECK(tag.kind() == NbtType::IntArray);
    const std::vector<int32_t>& out = tag.asIntArray();
    CHECK_EQ(out.size(), values.size());
    for (std::size_t i = 0; i < values.size(); ++i) CHECK_EQ(out[i], values[i]);
}

VF_TEST(nbtListOfIntRoundTrips) {
    NbtTag list = NbtTag::makeList(NbtType::Int);
    for (int32_t v : {10, -20, 30, -40, 50}) list.listAppend(NbtTag::makeInt(v));

    std::string name;
    NbtTag tag;
    CHECK(roundTrip("intList", list, name, tag));
    CHECK(tag.kind() == NbtType::List);
    CHECK(tag.listElementType() == NbtType::Int);
    CHECK_EQ(tag.asList().size(), static_cast<std::size_t>(5));
    CHECK_EQ(tag.asList()[0].asInt(), 10);
    CHECK_EQ(tag.asList()[3].asInt(), -40);
}

VF_TEST(nbtListOfStringRoundTrips) {
    NbtTag list = NbtTag::makeList(NbtType::String);
    list.listAppend(NbtTag::makeString("oak"));
    list.listAppend(NbtTag::makeString("spruce"));
    list.listAppend(NbtTag::makeString("jungle"));

    std::string name;
    NbtTag tag;
    CHECK(roundTrip("trees", list, name, tag));
    CHECK_EQ(tag.asList().size(), static_cast<std::size_t>(3));
    CHECK_EQ(tag.asList()[1].asString(), std::string("spruce"));
}

VF_TEST(nbtEmptyListRoundTrips) {
    NbtTag list = NbtTag::makeList(NbtType::Compound);
    std::string name;
    NbtTag tag;
    CHECK(roundTrip("nothing", list, name, tag));
    CHECK(tag.kind() == NbtType::List);
    CHECK(tag.asList().empty());
}

VF_TEST(nbtNestedCompoundRoundTrips) {
    NbtTag position = NbtTag::makeCompound();
    position.put("x", NbtTag::makeInt(120));
    position.put("y", NbtTag::makeInt(64));
    position.put("z", NbtTag::makeInt(-90));

    NbtTag inventory = NbtTag::makeList(NbtType::Compound);
    NbtTag item = NbtTag::makeCompound();
    item.put("id", NbtTag::makeShort(9));
    item.put("count", NbtTag::makeByte(32));
    inventory.listAppend(item);

    NbtTag root = NbtTag::makeCompound();
    root.put("position", position);
    root.put("inventory", inventory);
    root.put("health", NbtTag::makeFloat(18.5f));

    std::string name;
    NbtTag out;
    CHECK(roundTrip("player", root, name, out));
    CHECK_EQ(name, std::string("player"));
    CHECK(out.kind() == NbtType::Compound);

    const NbtTag* outPosition = out.find("position");
    CHECK(outPosition != nullptr);
    CHECK_EQ(outPosition->find("x")->asInt(), 120);
    CHECK_EQ(outPosition->find("y")->asInt(), 64);
    CHECK_EQ(outPosition->find("z")->asInt(), -90);

    const NbtTag* outInventory = out.find("inventory");
    CHECK(outInventory != nullptr);
    CHECK_EQ(outInventory->asList().size(), static_cast<std::size_t>(1));
    CHECK_EQ(outInventory->asList()[0].find("id")->asShort(), static_cast<int16_t>(9));
    CHECK_EQ(outInventory->asList()[0].find("count")->asByte(), static_cast<int8_t>(32));

    CHECK_NEAR(out.find("health")->asFloat(), 18.5f, 1e-6f);
    CHECK(out.find("missing") == nullptr);
}

VF_TEST(nbtPutOverwritesExistingKeyInsteadOfAppending) {
    NbtTag compound = NbtTag::makeCompound();
    compound.put("score", NbtTag::makeInt(10));
    compound.put("score", NbtTag::makeInt(20));
    CHECK_EQ(compound.asCompound().size(), static_cast<std::size_t>(1));
    CHECK_EQ(compound.find("score")->asInt(), 20);
}

VF_TEST(nbtFindReturnsNullptrForMissingKey) {
    NbtTag compound = NbtTag::makeCompound();
    compound.put("present", NbtTag::makeByte(1));
    CHECK(compound.find("present") != nullptr);
    CHECK(compound.find("absent") == nullptr);
}

VF_TEST(nbtDeserializeRejectsTruncatedData) {
    NbtTag tag = NbtTag::makeInt(999);
    std::vector<uint8_t> bytes = serializeNbt("value", tag);
    CHECK(bytes.size() > 2);

    std::string name;
    NbtTag out;
    for (std::size_t cut = 0; cut < bytes.size(); ++cut) {
        bool ok = deserializeNbt(bytes.data(), cut, name, out);
        CHECK(!ok);
    }
    CHECK(deserializeNbt(bytes.data(), bytes.size(), name, out));
}

VF_TEST(nbtPaletteRoundTripsUniformSection) {
    std::array<Block, chunkVolume> blocks;
    blocks.fill(Block::Stone);

    NbtPaletteSection section = encodeSectionPalette(blocks);
    CHECK_EQ(section.palette.size(), static_cast<std::size_t>(1));
    CHECK_EQ(section.bitsPerIndex, 1);

    std::array<Block, chunkVolume> decoded;
    decoded.fill(Block::Air);
    decodeSectionPalette(section, decoded);
    CHECK(decoded == blocks);
}

VF_TEST(nbtPaletteRoundTripsMixedSection) {
    std::array<Block, chunkVolume> blocks;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        switch (i % 5) {
            case 0: blocks[i] = Block::Air; break;
            case 1: blocks[i] = Block::Stone; break;
            case 2: blocks[i] = Block::Dirt; break;
            case 3: blocks[i] = Block::Grass; break;
            default: blocks[i] = Block::Water; break;
        }
    }

    NbtPaletteSection section = encodeSectionPalette(blocks);
    CHECK_EQ(section.palette.size(), static_cast<std::size_t>(5));
    CHECK_EQ(section.bitsPerIndex, 3);

    std::array<Block, chunkVolume> decoded;
    decodeSectionPalette(section, decoded);
    CHECK(decoded == blocks);
}

VF_TEST(nbtPaletteBitsPerIndexScalesWithPaletteSize) {
    auto bitsForPaletteSize = [](int distinctBlocks) {
        std::array<Block, chunkVolume> blocks;
        for (std::size_t i = 0; i < blocks.size(); ++i)
            blocks[i] = static_cast<Block>(static_cast<int>(i) % distinctBlocks);
        NbtPaletteSection section = encodeSectionPalette(blocks);
        return section.bitsPerIndex;
    };

    CHECK_EQ(bitsForPaletteSize(1), 1);
    CHECK_EQ(bitsForPaletteSize(2), 1);
    CHECK_EQ(bitsForPaletteSize(3), 2);
    CHECK_EQ(bitsForPaletteSize(4), 2);
    CHECK_EQ(bitsForPaletteSize(5), 3);
    CHECK_EQ(bitsForPaletteSize(9), 4);
    CHECK_EQ(bitsForPaletteSize(20), 5);
}

VF_TEST(nbtPalettePackedIndicesSizeMatchesExpectedByteCount) {
    std::array<Block, chunkVolume> blocks;
    for (std::size_t i = 0; i < blocks.size(); ++i)
        blocks[i] = static_cast<Block>(static_cast<int>(i) % 9);

    NbtPaletteSection section = encodeSectionPalette(blocks);
    std::size_t expectedBits = static_cast<std::size_t>(section.bitsPerIndex) * blocks.size();
    std::size_t expectedBytes = (expectedBits + 7) / 8;
    CHECK_EQ(section.packedIndices.size(), expectedBytes);
}

VF_TEST(nbtPalettePreservesFirstSeenOrder) {
    std::array<Block, chunkVolume> blocks;
    blocks.fill(Block::Stone);
    blocks[0] = Block::Water;
    blocks[1] = Block::Grass;
    blocks[2] = Block::Stone;

    NbtPaletteSection section = encodeSectionPalette(blocks);
    CHECK_EQ(section.palette.size(), static_cast<std::size_t>(3));
    CHECK(section.palette[0] == Block::Water);
    CHECK(section.palette[1] == Block::Grass);
    CHECK(section.palette[2] == Block::Stone);
}
