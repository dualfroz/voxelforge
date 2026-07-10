#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Block.h"
#include "Codec.h"
#include "EditHistory.h"
#include "Math.h"
#include "Selection.h"
#include "World.h"

namespace vf {

// Quarter turns around the vertical axis, clockwise seen from above.
enum class Rotation : uint8_t {
    None = 0,
    Cw90 = 1,
    Cw180 = 2,
    Cw270 = 3,
};

enum class MirrorAxis : uint8_t {
    X = 0,
    Y = 1,
    Z = 2,
};

// On-disk format revisions. v1 is dimensions plus a palette-indexed block
// stream; v2 adds metadata and the void mask that lets a non-box selection keep
// its holes. The reader accepts both and hands back v2 either way.
enum class SchematicVersion : uint8_t {
    V1 = 1,
    V2 = 2,
};

enum class SchematicError : uint8_t {
    None = 0,
    BadMagic = 1,
    UnsupportedVersion = 2,
    Truncated = 3,
    BadChecksum = 4,
    DimensionMismatch = 5,
    BadPalette = 6,
};

// A dense block volume with a per-cell presence flag. Presence is what makes a
// sphere copied out of a world paste back as a sphere instead of as its
// bounding box.
class ClipboardBuffer {
public:
    enum class PasteMode : uint8_t {
        Overwrite = 0,
        IntoAirOnly = 1,
        ReplaceExisting = 2,
    };

    ClipboardBuffer() = default;
    ClipboardBuffer(const Vec3i& dims, const Vec3i& anchor);

    static ClipboardBuffer copyFromWorld(const World& world, const Selection& selection);

    const Vec3i& dims() const { return size; }
    const Vec3i& anchor() const { return handle; }
    void setAnchor(const Vec3i& anchor) { handle = anchor; }

    std::size_t volume() const;
    std::size_t filledCount() const { return present.count(); }
    bool empty() const { return volume() == 0u; }

    bool inBounds(int x, int y, int z) const;
    Block at(int x, int y, int z) const;
    bool filledAt(int x, int y, int z) const;
    void put(int x, int y, int z, Block block, bool filled = true);
    void clearCell(int x, int y, int z);

    ClipboardBuffer rotatedY(Rotation rotation) const;
    ClipboardBuffer mirrored(MirrorAxis axis) const;
    ClipboardBuffer trimmed() const;

    bool sameContent(const ClipboardBuffer& other) const;

    const std::vector<Block>& raw() const { return blocks; }
    const SelectionMask& presence() const { return present; }

private:
    std::size_t indexOf(int x, int y, int z) const;

    Vec3i size{0, 0, 0};
    Vec3i handle{0, 0, 0};
    std::vector<Block> blocks;
    SelectionMask present;
};

// Everything the reader needs before it touches the block stream. paletteSize
// and indexBits are kept in sync with the buffer, so they describe what the
// next encode() would emit.
struct SchematicHeader {
    SchematicVersion version = SchematicVersion::V2;
    Vec3i dims{0, 0, 0};
    Vec3i anchor{0, 0, 0};
    std::string name;
    std::string author;
    uint64_t tick = 0;
    uint16_t paletteSize = 0;
    uint8_t indexBits = 0;
    bool hasVoidMask = false;
};

// A portable construction: header plus block volume, encoded into a byte buffer
// and nothing else. No path, no stream, no clock - the creation time is a game
// tick handed in by the caller.
class Schematic {
public:
    Schematic() = default;

    static Schematic capture(const World& world, const Selection& selection,
                             const std::string& name, const std::string& author, uint64_t tick);
    static Schematic fromBuffer(const ClipboardBuffer& buffer, const std::string& name,
                                const std::string& author, uint64_t tick);

    const SchematicHeader& header() const { return meta; }
    const ClipboardBuffer& buffer() const { return cells; }
    bool migratedFromV1() const { return migrated; }

    void setName(const std::string& name) { meta.name = name; }
    void setAuthor(const std::string& author) { meta.author = author; }
    void setTick(uint64_t tick) { meta.tick = tick; }
    void setAnchor(const Vec3i& anchor);

    ByteBuffer encode() const;
    ByteBuffer encode(SchematicVersion version) const;
    static SchematicError decode(const uint8_t* data, std::size_t size, Schematic& out);

    void rotate(Rotation rotation);
    void mirror(MirrorAxis axis);
    void trim();

    std::size_t pasteInto(World& world, const Vec3i& target, ClipboardBuffer::PasteMode mode) const;
    std::size_t pasteInto(World& world, EditHistory& history, const Vec3i& target,
                          ClipboardBuffer::PasteMode mode, uint64_t tick) const;

    static const char* errorText(SchematicError error);
    static uint8_t bitsForPalette(std::size_t paletteSize);

private:
    void refreshHeaderFromBuffer();

    SchematicHeader meta;
    ClipboardBuffer cells;
    bool migrated = false;
};

} // namespace vf
