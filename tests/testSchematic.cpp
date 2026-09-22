#include "testHarness.h"
#include "vf/Codec.h"
#include "vf/EditHistory.h"
#include "vf/Schematic.h"
#include "vf/Selection.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

constexpr std::size_t bodyOffset = 12;
constexpr std::size_t paletteOffsetInBody = 26;

uint32_t readU32(const ByteBuffer& bytes, std::size_t at) {
    return (static_cast<uint32_t>(bytes[at]) << 24) | (static_cast<uint32_t>(bytes[at + 1]) << 16) |
           (static_cast<uint32_t>(bytes[at + 2]) << 8) | static_cast<uint32_t>(bytes[at + 3]);
}

void writeU32(ByteBuffer& bytes, std::size_t at, uint32_t value) {
    bytes[at] = static_cast<uint8_t>(value >> 24);
    bytes[at + 1] = static_cast<uint8_t>(value >> 16);
    bytes[at + 2] = static_cast<uint8_t>(value >> 8);
    bytes[at + 3] = static_cast<uint8_t>(value);
}

void refreshChecksum(ByteBuffer& bytes) {
    uint32_t bodyLength = readU32(bytes, 8);
    writeU32(bytes, bodyOffset + bodyLength, fnv1a32(bytes.data() + bodyOffset, bodyLength));
}

void patchBody(ByteBuffer& bytes, std::size_t offsetInBody, uint8_t value) {
    bytes[bodyOffset + offsetInBody] = value;
    refreshChecksum(bytes);
}

World terrainWorld(int spanX, int spanZ) {
    World world;
    for (int z = 0; z < spanZ; ++z)
        for (int x = 0; x < spanX; ++x) {
            int height = 2 + ((x * 3 + z * 5) % 3);
            for (int y = 0; y <= height; ++y) {
                Block block = Block::Stone;
                if (y == height) block = Block::Grass;
                else if (y > height - 2) block = Block::Dirt;
                world.setBlock(x, y, z, block);
            }
        }
    return world;
}

bool sameRegion(const World& left, const World& right, const Vec3i& lo, const Vec3i& hi) {
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x)
                if (left.blockAt(x, y, z) != right.blockAt(x, y, z)) return false;
    return true;
}

Schematic decodedCopy(const ByteBuffer& bytes, SchematicError& error) {
    Schematic loaded;
    error = Schematic::decode(bytes.data(), bytes.size(), loaded);
    return loaded;
}

}

VF_TEST(aSchematicRoundTripRebuildsTheExactSameBlocks) {
    World source = terrainWorld(9, 9);
    Selection region = Selection::box({0, 0, 0}, {8, 5, 8});
    Schematic saved = Schematic::capture(source, region, "ridge", "foksiu", 900u);

    CHECK(saved.header().dims == Vec3i(9, 6, 9));
    CHECK_EQ(saved.buffer().filledCount(), 9u * 6u * 9u);

    ByteBuffer bytes = saved.encode();
    SchematicError error = SchematicError::BadMagic;
    Schematic loaded = decodedCopy(bytes, error);
    CHECK(error == SchematicError::None);
    CHECK(loaded.buffer().sameContent(saved.buffer()));

    World rebuilt;
    std::size_t written = loaded.pasteInto(rebuilt, {0, 0, 0}, ClipboardBuffer::PasteMode::Overwrite);
    CHECK(written > 0u);
    CHECK(sameRegion(source, rebuilt, {0, 0, 0}, {8, 5, 8}));
}

VF_TEST(aSchematicCarriesItsMetadataThroughTheRoundTrip) {
    World source = terrainWorld(4, 4);
    Schematic saved = Schematic::capture(source, Selection::box({0, 0, 0}, {3, 3, 3}), "watchtower",
                                         "vulpes", 1234567890123ull);
    saved.setAnchor({1, 2, 3});

    ByteBuffer bytes = saved.encode();
    SchematicError error = SchematicError::BadMagic;
    Schematic loaded = decodedCopy(bytes, error);
    CHECK(error == SchematicError::None);
    CHECK(loaded.header().name == std::string("watchtower"));
    CHECK(loaded.header().author == std::string("vulpes"));
    CHECK_EQ(loaded.header().tick, 1234567890123ull);
    CHECK(loaded.header().anchor == Vec3i(1, 2, 3));
    CHECK(loaded.header().version == SchematicVersion::V2);
    CHECK(!loaded.migratedFromV1());
}

VF_TEST(theIndexWidthShrinksWithTheNumberOfDistinctBlocks) {
    CHECK_EQ(Schematic::bitsForPalette(1u), 1u);
    CHECK_EQ(Schematic::bitsForPalette(2u), 1u);
    CHECK_EQ(Schematic::bitsForPalette(3u), 2u);
    CHECK_EQ(Schematic::bitsForPalette(4u), 2u);
    CHECK_EQ(Schematic::bitsForPalette(5u), 4u);
    CHECK_EQ(Schematic::bitsForPalette(16u), 4u);
    CHECK_EQ(Schematic::bitsForPalette(17u), 8u);

    ClipboardBuffer twoTone({8, 8, 8}, {0, 0, 0});
    for (int y = 0; y < 8; ++y)
        for (int z = 0; z < 8; ++z)
            for (int x = 0; x < 8; ++x) twoTone.put(x, y, z, Block::Stone);
    Schematic plain = Schematic::fromBuffer(twoTone, "plain", "foksiu", 1u);
    CHECK_EQ(plain.header().paletteSize, 2u);
    CHECK_EQ(plain.header().indexBits, 1u);

    ClipboardBuffer manyTone = twoTone;
    for (int i = 0; i < 20; ++i) manyTone.put(i % 8, i / 8, 0, static_cast<Block>(20 + i));
    Schematic mixed = Schematic::fromBuffer(manyTone, "mixed", "foksiu", 1u);
    CHECK_EQ(mixed.header().paletteSize, 22u);
    CHECK_EQ(mixed.header().indexBits, 8u);

    SchematicError error = SchematicError::BadMagic;
    Schematic reloaded = decodedCopy(mixed.encode(), error);
    CHECK(error == SchematicError::None);
    CHECK(reloaded.buffer().sameContent(manyTone));
    CHECK_EQ(reloaded.header().indexBits, 8u);
}

VF_TEST(rotatingFourTimesByNinetyDegreesReturnsTheOriginal) {
    World source = terrainWorld(7, 5);
    Schematic original = Schematic::capture(source, Selection::box({0, 0, 0}, {6, 3, 4}), "spin",
                                            "foksiu", 10u);
    original.setAnchor({2, 1, 3});

    Schematic turning = original;
    for (int step = 0; step < 4; ++step) turning.rotate(Rotation::Cw90);
    CHECK(turning.buffer().sameContent(original.buffer()));
    CHECK(turning.header().dims == original.header().dims);
    CHECK(turning.header().anchor == original.header().anchor);

    Schematic half = original;
    half.rotate(Rotation::Cw180);
    Schematic twoQuarters = original;
    twoQuarters.rotate(Rotation::Cw90);
    twoQuarters.rotate(Rotation::Cw90);
    CHECK(half.buffer().sameContent(twoQuarters.buffer()));

    Schematic backAgain = original;
    backAgain.rotate(Rotation::Cw90);
    backAgain.rotate(Rotation::Cw270);
    CHECK(backAgain.buffer().sameContent(original.buffer()));
}

VF_TEST(aQuarterTurnSwapsTheHorizontalDimensionsAndMovesTheMarker) {
    ClipboardBuffer buffer({4, 1, 2}, {0, 0, 0});
    for (int z = 0; z < 2; ++z)
        for (int x = 0; x < 4; ++x) buffer.put(x, 0, z, Block::Stone);
    buffer.put(0, 0, 0, Block::Glowstone);

    ClipboardBuffer turned = buffer.rotatedY(Rotation::Cw90);
    CHECK(turned.dims() == Vec3i(2, 1, 4));
    CHECK_EQ(turned.filledCount(), 8u);
    CHECK(turned.at(1, 0, 0) == Block::Glowstone);
    CHECK(turned.at(0, 0, 0) == Block::Stone);
}

VF_TEST(mirroringTwiceOnTheSameAxisReturnsTheOriginal) {
    ClipboardBuffer buffer({3, 2, 4}, {1, 0, 2});
    for (int y = 0; y < 2; ++y)
        for (int z = 0; z < 4; ++z)
            for (int x = 0; x < 3; ++x) buffer.put(x, y, z, Block::Planks);
    buffer.put(0, 0, 0, Block::Chest);

    ClipboardBuffer flipped = buffer.mirrored(MirrorAxis::X);
    CHECK(flipped.at(2, 0, 0) == Block::Chest);
    CHECK(flipped.at(0, 0, 0) == Block::Planks);
    CHECK(flipped.anchor() == Vec3i(1, 0, 2));

    CHECK(flipped.mirrored(MirrorAxis::X).sameContent(buffer));
    CHECK(buffer.mirrored(MirrorAxis::Y).mirrored(MirrorAxis::Y).sameContent(buffer));
    CHECK(buffer.mirrored(MirrorAxis::Z).mirrored(MirrorAxis::Z).sameContent(buffer));

    ClipboardBuffer alongZ = buffer.mirrored(MirrorAxis::Z);
    CHECK(alongZ.at(0, 0, 3) == Block::Chest);
    CHECK(alongZ.anchor() == Vec3i(1, 0, 1));
}

VF_TEST(trimmingDropsEmptyMarginsAndKeepsTheAnchorOnTheSameBlock) {
    ClipboardBuffer padded({8, 8, 8}, {5, 5, 5});
    for (int y = 4; y <= 5; ++y)
        for (int z = 4; z <= 6; ++z)
            for (int x = 5; x <= 6; ++x) padded.put(x, y, z, Block::Bricks);

    ClipboardBuffer tight = padded.trimmed();
    CHECK(tight.dims() == Vec3i(2, 2, 3));
    CHECK_EQ(tight.filledCount(), 12u);
    CHECK(tight.anchor() == Vec3i(0, 1, 1));
    CHECK(tight.at(0, 0, 0) == Block::Bricks);

    World empty;
    Schematic schematic = Schematic::fromBuffer(padded, "pad", "foksiu", 3u);
    schematic.trim();
    CHECK(schematic.header().dims == Vec3i(2, 2, 3));
    CHECK_EQ(schematic.pasteInto(empty, {0, 0, 0}, ClipboardBuffer::PasteMode::Overwrite), 12u);
}

VF_TEST(versionOneDataIsMigratedToVersionTwoWhenLoaded) {
    World source = terrainWorld(6, 6);
    Schematic saved = Schematic::capture(source, Selection::box({0, 0, 0}, {5, 4, 5}), "legacy",
                                         "foksiu", 777u);

    ByteBuffer oldBytes = saved.encode(SchematicVersion::V1);
    ByteBuffer newBytes = saved.encode(SchematicVersion::V2);
    CHECK(oldBytes.size() < newBytes.size());

    SchematicError error = SchematicError::BadMagic;
    Schematic loaded = decodedCopy(oldBytes, error);
    CHECK(error == SchematicError::None);
    CHECK(loaded.migratedFromV1());
    CHECK(loaded.header().version == SchematicVersion::V2);
    CHECK(loaded.header().name.empty());
    CHECK(loaded.header().author.empty());
    CHECK_EQ(loaded.header().tick, 0u);
    CHECK(loaded.buffer().sameContent(saved.buffer()));

    loaded.setName("legacy");
    loaded.setAuthor("foksiu");
    loaded.setTick(777u);
    SchematicError second = SchematicError::BadMagic;
    Schematic again = decodedCopy(loaded.encode(), second);
    CHECK(second == SchematicError::None);
    CHECK(!again.migratedFromV1());
    CHECK(again.header().name == std::string("legacy"));
}

VF_TEST(theVoidMaskKeepsHolesInVersionTwoAndIsLostInVersionOne) {
    World stoneBlock;
    for (int y = 0; y < 9; ++y)
        for (int z = 0; z < 9; ++z)
            for (int x = 0; x < 9; ++x) stoneBlock.setBlock(x, y, z, Block::Stone);

    Selection ball = Selection::sphere({4, 4, 4}, 4);
    Schematic saved = Schematic::capture(stoneBlock, ball, "ball", "foksiu", 5u);
    CHECK(saved.header().hasVoidMask);
    CHECK(saved.buffer().filledCount() < saved.buffer().volume());

    SchematicError error = SchematicError::BadMagic;
    Schematic loaded = decodedCopy(saved.encode(), error);
    CHECK(error == SchematicError::None);
    CHECK(loaded.header().hasVoidMask);
    CHECK_EQ(loaded.buffer().filledCount(), saved.buffer().filledCount());
    CHECK(!loaded.buffer().filledAt(0, 0, 0));

    SchematicError legacyError = SchematicError::BadMagic;
    Schematic legacy = decodedCopy(saved.encode(SchematicVersion::V1), legacyError);
    CHECK(legacyError == SchematicError::None);
    CHECK(!legacy.header().hasVoidMask);
    CHECK_EQ(legacy.buffer().filledCount(), legacy.buffer().volume());
    CHECK(legacy.buffer().at(0, 0, 0) == Block::Air);

    World target;
    loaded.pasteInto(target, {0, 0, 0}, ClipboardBuffer::PasteMode::Overwrite);
    CHECK(target.blockAt(4, 4, 4) == Block::Stone);
    CHECK(target.blockAt(0, 0, 0) == Block::Air);
    CHECK(target.blockAt(8, 8, 8) == Block::Air);
}

VF_TEST(aBufferWithTheWrongMagicNumberIsRejected) {
    World source = terrainWorld(3, 3);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {2, 2, 2}), "m", "f", 1u).encode();
    bytes[0] = 0x00;

    Schematic loaded;
    CHECK(Schematic::decode(bytes.data(), bytes.size(), loaded) == SchematicError::BadMagic);

    ByteBuffer garbage = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    CHECK(Schematic::decode(garbage.data(), garbage.size(), loaded) == SchematicError::BadMagic);
}

VF_TEST(anUnknownFormatVersionIsRejected) {
    World source = terrainWorld(3, 3);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {2, 2, 2}), "v", "f", 1u).encode();
    writeU32(bytes, 4, 7u);

    Schematic loaded;
    CHECK(Schematic::decode(bytes.data(), bytes.size(), loaded) ==
          SchematicError::UnsupportedVersion);

    writeU32(bytes, 4, 0u);
    CHECK(Schematic::decode(bytes.data(), bytes.size(), loaded) ==
          SchematicError::UnsupportedVersion);
}

VF_TEST(aTruncatedBufferIsRejectedAtEveryCutPoint) {
    World source = terrainWorld(5, 5);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {4, 4, 4}), "cut", "f", 1u).encode();
    CHECK(bytes.size() > 40u);

    Schematic loaded;
    for (std::size_t keep : {std::size_t(0), std::size_t(3), std::size_t(11), bytes.size() - 5u,
                             bytes.size() - 1u}) {
        ByteBuffer cut(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(keep));
        SchematicError error = Schematic::decode(cut.data(), cut.size(), loaded);
        CHECK(error == SchematicError::Truncated);
    }

    ByteBuffer lying = bytes;
    writeU32(lying, 8, readU32(lying, 8) + 64u);
    CHECK(Schematic::decode(lying.data(), lying.size(), loaded) == SchematicError::Truncated);
}

VF_TEST(aFlippedByteInTheBodyIsCaughtByTheChecksum) {
    World source = terrainWorld(5, 5);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {4, 4, 4}), "bit", "f", 1u).encode();

    Schematic loaded;
    for (std::size_t offset : {bodyOffset, bodyOffset + 20u, bytes.size() - 6u}) {
        ByteBuffer damaged = bytes;
        damaged[offset] = static_cast<uint8_t>(damaged[offset] ^ 0x40u);
        CHECK(Schematic::decode(damaged.data(), damaged.size(), loaded) ==
              SchematicError::BadChecksum);
    }

    ByteBuffer wrongChecksum = bytes;
    writeU32(wrongChecksum, wrongChecksum.size() - 4u, 0xdeadbeefu);
    CHECK(Schematic::decode(wrongChecksum.data(), wrongChecksum.size(), loaded) ==
          SchematicError::BadChecksum);
}

VF_TEST(dimensionsThatDoNotMatchThePayloadAreRejected) {
    World source = terrainWorld(5, 5);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {4, 4, 4}), "dim", "f", 1u).encode();

    Schematic control;
    CHECK(Schematic::decode(bytes.data(), bytes.size(), control) == SchematicError::None);
    CHECK(control.header().dims == Vec3i(5, 5, 5));

    ByteBuffer widened = bytes;
    patchBody(widened, 3u, 6u);
    Schematic loaded;
    CHECK(Schematic::decode(widened.data(), widened.size(), loaded) ==
          SchematicError::DimensionMismatch);

    ByteBuffer negative = bytes;
    negative[bodyOffset] = 0xffu;
    refreshChecksum(negative);
    CHECK(Schematic::decode(negative.data(), negative.size(), loaded) ==
          SchematicError::DimensionMismatch);
}

VF_TEST(aPaletteThatDoesNotDescribeRealBlocksIsRejected) {
    World source = terrainWorld(5, 5);
    ByteBuffer bytes =
        Schematic::capture(source, Selection::box({0, 0, 0}, {4, 4, 4}), "pal", "f", 1u).encode();

    Schematic loaded;
    ByteBuffer unknownBlock = bytes;
    patchBody(unknownBlock, paletteOffsetInBody, 250u);
    CHECK(Schematic::decode(unknownBlock.data(), unknownBlock.size(), loaded) ==
          SchematicError::BadPalette);

    ByteBuffer emptyPalette = bytes;
    patchBody(emptyPalette, 25u, 0u);
    CHECK(Schematic::decode(emptyPalette.data(), emptyPalette.size(), loaded) ==
          SchematicError::BadPalette);
}

VF_TEST(pasteModesDecideWhichExistingBlocksSurvive) {
    ClipboardBuffer stamp({3, 1, 3}, {0, 0, 0});
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 3; ++x) stamp.put(x, 0, z, Block::Planks);
    Schematic schematic = Schematic::fromBuffer(stamp, "floor", "foksiu", 2u);

    World overwrite;
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 3; ++x) overwrite.setBlock(x, 0, z, Block::Stone);
    CHECK_EQ(schematic.pasteInto(overwrite, {0, 0, 0}, ClipboardBuffer::PasteMode::Overwrite), 9u);
    CHECK(overwrite.blockAt(1, 0, 1) == Block::Planks);

    World intoAir;
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 3; ++x) intoAir.setBlock(x, 0, z, Block::Stone);
    CHECK_EQ(schematic.pasteInto(intoAir, {0, 0, 0}, ClipboardBuffer::PasteMode::IntoAirOnly), 0u);
    CHECK(intoAir.blockAt(1, 0, 1) == Block::Stone);
    CHECK_EQ(schematic.pasteInto(intoAir, {0, 1, 0}, ClipboardBuffer::PasteMode::IntoAirOnly), 9u);
    CHECK(intoAir.blockAt(1, 1, 1) == Block::Planks);

    World replaceOnly;
    replaceOnly.setBlock(0, 0, 0, Block::Stone);
    replaceOnly.setBlock(2, 0, 2, Block::Stone);
    CHECK_EQ(schematic.pasteInto(replaceOnly, {0, 0, 0},
                                 ClipboardBuffer::PasteMode::ReplaceExisting),
             2u);
    CHECK(replaceOnly.blockAt(0, 0, 0) == Block::Planks);
    CHECK(replaceOnly.blockAt(1, 0, 1) == Block::Air);
}

VF_TEST(theAnchorDecidesWhereAPastedSchematicLands) {
    ClipboardBuffer stamp({3, 3, 3}, {1, 1, 1});
    for (int y = 0; y < 3; ++y)
        for (int z = 0; z < 3; ++z)
            for (int x = 0; x < 3; ++x) stamp.put(x, y, z, Block::Sandstone);
    Schematic schematic = Schematic::fromBuffer(stamp, "cube", "foksiu", 4u);

    World world;
    schematic.pasteInto(world, {50, 50, 50}, ClipboardBuffer::PasteMode::Overwrite);
    CHECK(world.blockAt(50, 50, 50) == Block::Sandstone);
    CHECK(world.blockAt(49, 49, 49) == Block::Sandstone);
    CHECK(world.blockAt(51, 51, 51) == Block::Sandstone);
    CHECK(world.blockAt(52, 50, 50) == Block::Air);
    CHECK(world.blockAt(48, 50, 50) == Block::Air);
}

VF_TEST(pastingThroughTheHistoryIsOneUndoableBatch) {
    World world = terrainWorld(6, 6);
    Selection region = Selection::box({0, 0, 0}, {5, 4, 5});
    Schematic saved = Schematic::capture(world, region, "copy", "foksiu", 1u);

    EditHistory history(1u << 20);
    std::size_t before = 0;
    for (int y = 0; y <= 4; ++y)
        for (int z = 0; z <= 5; ++z)
            for (int x = 0; x <= 5; ++x)
                if (!isAir(world.blockAt(x, y, z))) ++before;

    std::size_t written = saved.pasteInto(world, history, {20, 0, 20},
                                          ClipboardBuffer::PasteMode::Overwrite, 5u);
    CHECK(written > 0u);
    CHECK_EQ(history.stats().batchCount, 1u);
    CHECK(world.blockAt(20, 0, 20) == world.blockAt(0, 0, 0));

    CHECK(history.undo(world));
    for (int y = 0; y <= 4; ++y)
        for (int z = 0; z <= 5; ++z)
            for (int x = 0; x <= 5; ++x) CHECK(isAir(world.blockAt(20 + x, y, 20 + z)));

    std::size_t after = 0;
    for (int y = 0; y <= 4; ++y)
        for (int z = 0; z <= 5; ++z)
            for (int x = 0; x <= 5; ++x)
                if (!isAir(world.blockAt(x, y, z))) ++after;
    CHECK_EQ(after, before);
}

VF_TEST(anEmptySchematicSurvivesTheRoundTrip) {
    Schematic nothing = Schematic::fromBuffer(ClipboardBuffer{}, "void", "foksiu", 9u);
    CHECK(nothing.header().dims == Vec3i(0, 0, 0));

    ByteBuffer bytes = nothing.encode();
    Schematic loaded;
    CHECK(Schematic::decode(bytes.data(), bytes.size(), loaded) == SchematicError::None);
    CHECK(loaded.buffer().empty());
    CHECK(loaded.header().name == std::string("void"));

    World world;
    CHECK_EQ(loaded.pasteInto(world, {0, 0, 0}, ClipboardBuffer::PasteMode::Overwrite), 0u);
    CHECK_EQ(world.chunkCount(), 0u);
}

VF_TEST(reEncodingADecodedSchematicProducesTheSameBytes) {
    World source = terrainWorld(7, 7);
    Schematic saved = Schematic::capture(source, Selection::sphere({3, 3, 3}, 3), "stable",
                                         "foksiu", 42u);
    ByteBuffer first = saved.encode();

    Schematic loaded;
    CHECK(Schematic::decode(first.data(), first.size(), loaded) == SchematicError::None);
    ByteBuffer second = loaded.encode();
    CHECK(first == second);

    CHECK(std::string(Schematic::errorText(SchematicError::None)) == std::string("ok"));
    CHECK(std::string(Schematic::errorText(SchematicError::BadPalette)) !=
          std::string(Schematic::errorText(SchematicError::BadChecksum)));
}
