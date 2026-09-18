#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "Block.h"
#include "Chunk.h"
#include "Codec.h"
#include "World.h"

namespace vf {

constexpr int anvilRegionSpan = 32;
constexpr int32_t heightNone = -2147483647 - 1;

class BitPacker {
public:
    static int bitWidthForPalette(std::size_t paletteSize);
    static std::vector<uint64_t> pack(const std::vector<uint32_t>& values, int bitWidth);
    static std::vector<uint32_t> unpack(const std::vector<uint64_t>& words, int bitWidth,
                                        std::size_t count);
};

struct AnvilSection {
    int sectionY = 0;
    std::vector<Block> palette;
    int bitWidth = 1;
    std::vector<uint64_t> packed;

    void encodeFrom(const Chunk& chunk);
    void decodeTo(Chunk& chunk) const;
};

struct AnvilColumn {
    int localX = 0;
    int localZ = 0;
    std::vector<AnvilSection> sections;
    std::vector<int32_t> heightmap;

    const AnvilSection* section(int sectionY) const;
};

class AnvilRegion {
public:
    AnvilRegion() = default;
    AnvilRegion(int regionX, int regionZ) : regionX(regionX), regionZ(regionZ) {}

    int rx() const { return regionX; }
    int rz() const { return regionZ; }

    void putChunk(const Vec3i& chunkCoord, const Chunk& chunk);
    bool hasChunk(const Vec3i& chunkCoord) const;
    bool getChunk(const Vec3i& chunkCoord, Chunk& out) const;

    std::size_t columnCount() const { return columns.size(); }
    std::size_t sectionCount() const;
    int32_t heightAt(int localX, int localZ, int x, int z) const;

    ByteBuffer serialize() const;
    static bool deserialize(const uint8_t* data, std::size_t size, AnvilRegion& out);

    const std::vector<AnvilColumn>& allColumns() const { return columns; }

private:
    AnvilColumn& columnFor(int localX, int localZ);
    const AnvilColumn* findColumn(int localX, int localZ) const;

    int regionX = 0;
    int regionZ = 0;
    std::vector<AnvilColumn> columns;
};

std::vector<AnvilRegion> anvilRegionsFromWorld(const World& world);

ByteBuffer anvilSerializeWorld(const World& world);
bool anvilDeserializeWorld(const uint8_t* data, std::size_t size, World& out);

}
