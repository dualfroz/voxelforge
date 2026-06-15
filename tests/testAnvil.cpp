#include "testHarness.h"
#include "vf/Anvil.h"

#include <cstdint>
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
};

Chunk patternChunk(int seed) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) {
                int v = (x * 5 + y * 13 + z * 7 + seed) % static_cast<int>(Block::Count);
                chunk.set(x, y, z, static_cast<Block>(v));
            }
    return chunk;
}

int32_t expectedHeight(const Chunk& chunk, int sectionY, int x, int z) {
    int32_t best = heightNone;
    for (int y = 0; y < chunkSize; ++y)
        if (chunk.at(x, y, z) != Block::Air) best = sectionY * chunkSize + y;
    return best;
}

}  // namespace

VF_TEST(bitPackerWidthForPalette) {
    CHECK_EQ(BitPacker::bitWidthForPalette(1), 1);
    CHECK_EQ(BitPacker::bitWidthForPalette(2), 1);
    CHECK_EQ(BitPacker::bitWidthForPalette(3), 2);
    CHECK_EQ(BitPacker::bitWidthForPalette(4), 2);
    CHECK_EQ(BitPacker::bitWidthForPalette(5), 3);
    CHECK_EQ(BitPacker::bitWidthForPalette(16), 4);
    CHECK_EQ(BitPacker::bitWidthForPalette(17), 5);
    CHECK_EQ(BitPacker::bitWidthForPalette(256), 8);
}

VF_TEST(bitPackerRoundTripsAllWidths) {
    Lcg rng(0xABCDEF);
    for (int bitWidth = 1; bitWidth <= 16; ++bitWidth) {
        uint32_t mask = bitWidth >= 32 ? 0xFFFFFFFFu : ((1u << bitWidth) - 1u);
        std::vector<uint32_t> values;
        for (int i = 0; i < 4096; ++i) values.push_back(rng.next() & mask);
        std::vector<uint64_t> words = BitPacker::pack(values, bitWidth);
        std::vector<uint32_t> back = BitPacker::unpack(words, bitWidth, values.size());
        CHECK(back == values);
    }
}

VF_TEST(bitPackerPackingIsCompact) {
    std::vector<uint32_t> values(4096, 3);
    std::vector<uint64_t> words = BitPacker::pack(values, 2);
    CHECK_EQ(words.size(), static_cast<std::size_t>(4096 / 32));
}

VF_TEST(anvilSectionRoundTrip) {
    Chunk chunk = patternChunk(11);
    AnvilSection section;
    section.sectionY = -3;
    section.encodeFrom(chunk);
    CHECK(section.palette.size() > 1);
    Chunk back;
    section.decodeTo(back);
    CHECK(back.raw() == chunk.raw());
}

VF_TEST(anvilSectionUniformChunk) {
    Chunk chunk;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z) chunk.set(x, y, z, Block::Stone);
    AnvilSection section;
    section.encodeFrom(chunk);
    CHECK_EQ(section.palette.size(), static_cast<std::size_t>(1));
    Chunk back;
    section.decodeTo(back);
    CHECK(back.raw() == chunk.raw());
}

VF_TEST(anvilRegionRoundTrip) {
    AnvilRegion region(2, -1);
    region.putChunk({64, 0, -33}, patternChunk(1));
    region.putChunk({64, 3, -33}, patternChunk(2));
    region.putChunk({70, -2, -40}, patternChunk(3));
    CHECK_EQ(region.sectionCount(), static_cast<std::size_t>(3));

    ByteBuffer bytes = region.serialize();
    AnvilRegion restored;
    CHECK(AnvilRegion::deserialize(bytes.data(), bytes.size(), restored));
    CHECK_EQ(restored.rx(), 2);
    CHECK_EQ(restored.rz(), -1);
    CHECK_EQ(restored.sectionCount(), static_cast<std::size_t>(3));

    Chunk out;
    CHECK(restored.getChunk({64, 3, -33}, out));
    CHECK(out.raw() == patternChunk(2).raw());
    CHECK(restored.getChunk({70, -2, -40}, out));
    CHECK(out.raw() == patternChunk(3).raw());
    CHECK(!restored.getChunk({99, 0, 0}, out));
}

VF_TEST(anvilRegionHeightmapCorrect) {
    AnvilRegion region(0, 0);
    Chunk chunk;
    chunk.set(1, 5, 2, Block::Stone);
    chunk.set(1, 9, 2, Block::Dirt);
    chunk.set(3, 0, 3, Block::Grass);
    region.putChunk({0, 4, 0}, chunk);

    ByteBuffer bytes = region.serialize();
    AnvilRegion restored;
    CHECK(AnvilRegion::deserialize(bytes.data(), bytes.size(), restored));

    CHECK_EQ(restored.heightAt(0, 0, 1, 2), 4 * chunkSize + 9);
    CHECK_EQ(restored.heightAt(0, 0, 3, 3), 4 * chunkSize + 0);
    CHECK_EQ(restored.heightAt(0, 0, 5, 5), heightNone);
    CHECK_EQ(restored.heightAt(0, 0, 1, 2), expectedHeight(chunk, 4, 1, 2));
}

VF_TEST(anvilRegionRejectsCorrupt) {
    AnvilRegion region(0, 0);
    region.putChunk({0, 0, 0}, patternChunk(9));
    ByteBuffer bytes = region.serialize();

    AnvilRegion out;
    bytes[0] ^= 0xFF;
    CHECK(!AnvilRegion::deserialize(bytes.data(), bytes.size(), out));
    bytes[0] ^= 0xFF;
    CHECK(!AnvilRegion::deserialize(bytes.data(), bytes.size() / 2, out));
}

VF_TEST(anvilWorldRoundTripAcrossRegions) {
    World world;
    world.setBlock(1, 1, 1, Block::Stone);
    world.setBlock(5, 70, 9, Block::Glowstone);
    world.setBlock(40 * chunkSize + 2, 3, 4, Block::Grass);
    world.setBlock(-1, 40, -1, Block::Sand);
    world.setBlock(-50 * chunkSize, -30, -50 * chunkSize, Block::Obsidian);
    for (int i = 0; i < chunkSize; ++i) world.setBlock(i, 200, i, Block::Ice);

    ByteBuffer blob = anvilSerializeWorld(world);
    World restored;
    CHECK(anvilDeserializeWorld(blob.data(), blob.size(), restored));

    CHECK(restored.blockAt(1, 1, 1) == Block::Stone);
    CHECK(restored.blockAt(5, 70, 9) == Block::Glowstone);
    CHECK(restored.blockAt(40 * chunkSize + 2, 3, 4) == Block::Grass);
    CHECK(restored.blockAt(-1, 40, -1) == Block::Sand);
    CHECK(restored.blockAt(-50 * chunkSize, -30, -50 * chunkSize) == Block::Obsidian);
    for (int i = 0; i < chunkSize; ++i) CHECK(restored.blockAt(i, 200, i) == Block::Ice);
}

VF_TEST(anvilWorldEveryBlockPreserved) {
    World world;
    Lcg rng(0x5151);
    for (int cx = 0; cx < 3; ++cx)
        for (int cz = 0; cz < 3; ++cz) {
            Chunk& chunk = world.ensureChunk({cx, 0, cz});
            for (int i = 0; i < 40; ++i) {
                int x = static_cast<int>(rng.next() % chunkSize);
                int y = static_cast<int>(rng.next() % chunkSize);
                int z = static_cast<int>(rng.next() % chunkSize);
                Block b = static_cast<Block>(rng.next() % static_cast<uint32_t>(Block::Count));
                chunk.set(x, y, z, b);
            }
        }

    ByteBuffer blob = anvilSerializeWorld(world);
    World restored;
    CHECK(anvilDeserializeWorld(blob.data(), blob.size(), restored));

    bool identical = true;
    for (const auto& entry : world.allChunks()) {
        Chunk* back = restored.chunkAt(entry.first);
        if (!back || back->raw() != entry.second->raw()) identical = false;
    }
    CHECK(identical);
    CHECK_EQ(restored.chunkCount(), world.chunkCount());
}
