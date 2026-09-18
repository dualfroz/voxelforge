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
    float light = 1.0f;
    float ao = 1.0f;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::size_t quadCount() const { return vertices.size() / 4; }
    bool empty() const { return vertices.empty(); }
};

struct ChunkMesh {
    Mesh opaque;
    Mesh transparent;
    bool empty() const { return opaque.empty() && transparent.empty(); }
};

Mesh greedyMeshChunk(const World& world, const Vec3i& chunkCoord);

int culledFaceCount(const World& world, const Vec3i& chunkCoord);

int vertexAo(bool side1, bool side2, bool corner);

}
