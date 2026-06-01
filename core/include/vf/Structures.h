#pragma once

#include <cstdint>
#include <vector>
#include "World.h"

namespace vf {

enum class StructureKind : uint8_t {
    OakTree = 0,
    PineTree = 1,
    Hut = 2,
};

struct StructureEdit {
    Vec3i offset;   // relative to the structure base (first cell above ground)
    Block block;
};

// Deterministic 32-bit mix of a seed and a coordinate; used to seed per-structure
// variation so the same seed and position always yield the same build.
uint32_t structureHash(uint32_t seed, int x, int y, int z);

// Builds the block edits for a structure. Pure: identical (kind, seed) always
// produce the identical edit list, in the same order.
std::vector<StructureEdit> buildStructure(StructureKind kind, uint32_t seed);

// Writes edits into the world at base + offset. Leaves yield to existing solid
// blocks (they only fill air); trunk, planks, roof and the lantern overwrite.
void stampStructure(World& world, const Vec3i& base, const std::vector<StructureEdit>& edits,
                    int maxY);

} // namespace vf
