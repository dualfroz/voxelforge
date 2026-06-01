#pragma once

#include <cstdint>
#include <vector>
#include "World.h"
#include "Math.h"

namespace vf {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tile;
    float light = 1.0f;   // baked sun+block light, 0..1
    float ao = 1.0f;      // ambient occlusion, 0..1 (1 = unoccluded)
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::size_t quadCount() const { return vertices.size() / 4; }
    bool empty() const { return vertices.empty(); }
};

// Opaque and transparent geometry for one chunk, kept apart so the renderer can
// draw the transparent pass last (depth write off, sorted back-to-front).
struct ChunkMesh {
    Mesh opaque;
    Mesh transparent;
    bool empty() const { return opaque.empty() && transparent.empty(); }
};

// Greedy meshing (Lysenko sweep) for one chunk. Faces are culled against
// neighbouring blocks in `world` (crossing chunk borders correctly) and
// coplanar faces of the same block type are merged into rectangles.
Mesh greedyMeshChunk(const World& world, const Vec3i& chunkCoord);

// Naive count of visible (culled but unmerged) faces for the same chunk.
// Used to prove greedy meshing actually reduces geometry.
int culledFaceCount(const World& world, const Vec3i& chunkCoord);

// Classic per-vertex ambient occlusion level (0 darkest .. 3 lightest) from the
// two edge-adjacent voxels and the diagonal corner voxel around one face vertex.
int vertexAo(bool side1, bool side2, bool corner);

} // namespace vf
