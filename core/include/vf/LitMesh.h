#pragma once

#include "Mesh.h"
#include "Lighting.h"

namespace vf {

// Face-culled mesh for one chunk carrying per-vertex ambient occlusion and
// baked light sampled from `light`. Opaque and transparent (water) faces are
// written to separate sub-meshes. Faces are culled against neighbours across
// chunk borders exactly like the greedy mesher.
ChunkMesh meshChunkLit(const World& world, const Vec3i& chunkCoord, const LightField& light);

} // namespace vf
