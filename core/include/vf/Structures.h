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
    Vec3i offset;
    Block block;
};

uint32_t structureHash(uint32_t seed, int x, int y, int z);

std::vector<StructureEdit> buildStructure(StructureKind kind, uint32_t seed);

void stampStructure(World& world, const Vec3i& base, const std::vector<StructureEdit>& edits,
                    int maxY);

}
