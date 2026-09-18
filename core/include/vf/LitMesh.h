#pragma once

#include "Mesh.h"
#include "Lighting.h"

namespace vf {

ChunkMesh meshChunkLit(const World& world, const Vec3i& chunkCoord, const LightField& light);

}
