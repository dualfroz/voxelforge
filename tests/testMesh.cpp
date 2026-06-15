#include "testHarness.h"
#include "vf/Mesh.h"

#include <cmath>

using namespace vf;

namespace {

float quadAreaSum(const Mesh& mesh) {
    float sum = 0.0f;
    for (std::size_t q = 0; q < mesh.quadCount(); ++q) {
        const Vertex& corner = mesh.vertices[q * 4 + 2];  // uv here holds (width, height)
        sum += corner.u * corner.v;
    }
    return sum;
}

bool hasNormal(const Mesh& mesh, float nx, float ny, float nz) {
    for (const Vertex& v : mesh.vertices)
        if (std::fabs(v.nx - nx) < 1e-5f && std::fabs(v.ny - ny) < 1e-5f &&
            std::fabs(v.nz - nz) < 1e-5f)
            return true;
    return false;
}

bool hasTile(const Mesh& mesh, int tile) {
    for (const Vertex& v : mesh.vertices)
        if (static_cast<int>(v.tile) == tile) return true;
    return false;
}

} // namespace

VF_TEST(meshSingleBlockHasSixFaces) {
    World world;
    world.setBlock(1, 1, 1, Block::Stone);
    Mesh mesh = greedyMeshChunk(world, {0, 0, 0});
    CHECK_EQ(mesh.quadCount(), static_cast<std::size_t>(6));
    CHECK_EQ(mesh.indices.size(), static_cast<std::size_t>(36));
    CHECK_EQ(culledFaceCount(world, {0, 0, 0}), 6);
    CHECK(hasNormal(mesh, 1, 0, 0));
    CHECK(hasNormal(mesh, -1, 0, 0));
    CHECK(hasNormal(mesh, 0, 1, 0));
    CHECK(hasNormal(mesh, 0, -1, 0));
    CHECK(hasNormal(mesh, 0, 0, 1));
    CHECK(hasNormal(mesh, 0, 0, -1));
}

VF_TEST(meshInteriorFacesAreCulled) {
    World world;
    for (int x = 0; x < chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z)
                world.setBlock(x, y, z, Block::Stone);

    Mesh mesh = greedyMeshChunk(world, {0, 0, 0});
    int culled = culledFaceCount(world, {0, 0, 0});
    CHECK_EQ(culled, 6 * chunkSize * chunkSize);       // only the outer shell
    CHECK_EQ(mesh.quadCount(), static_cast<std::size_t>(6));  // each side merges to one quad
    CHECK(mesh.quadCount() < static_cast<std::size_t>(culled));
    CHECK_NEAR(quadAreaSum(mesh), static_cast<float>(culled), 1e-3f);
}

VF_TEST(meshGreedyPreservesVisibleArea) {
    World world;
    world.setBlock(1, 1, 1, Block::Stone);
    world.setBlock(2, 1, 1, Block::Stone);
    world.setBlock(3, 1, 1, Block::Stone);

    Mesh mesh = greedyMeshChunk(world, {0, 0, 0});
    int culled = culledFaceCount(world, {0, 0, 0});
    CHECK_EQ(culled, 3 * 6 - 2 * 2);   // two shared faces hidden on both sides
    CHECK_NEAR(quadAreaSum(mesh), static_cast<float>(culled), 1e-3f);
    CHECK(mesh.quadCount() <= static_cast<std::size_t>(culled));
}

VF_TEST(meshCullsAcrossChunkBoundary) {
    World world;
    for (int x = 0; x < 2 * chunkSize; ++x)
        for (int y = 0; y < chunkSize; ++y)
            for (int z = 0; z < chunkSize; ++z)
                world.setBlock(x, y, z, Block::Stone);

    Mesh left = greedyMeshChunk(world, {0, 0, 0});
    int culled = culledFaceCount(world, {0, 0, 0});
    CHECK_EQ(culled, 5 * chunkSize * chunkSize);   // +x boundary hidden by neighbour chunk
    CHECK_EQ(left.quadCount(), static_cast<std::size_t>(5));
    CHECK(!hasNormal(left, 1, 0, 0));              // no +x face at the shared border
    CHECK_NEAR(quadAreaSum(left), static_cast<float>(culled), 1e-3f);
}

VF_TEST(meshGrassIsSided) {
    World world;
    world.setBlock(4, 4, 4, Block::Grass);
    Mesh mesh = greedyMeshChunk(world, {0, 0, 0});
    CHECK(hasTile(mesh, faceTile(Block::Grass, Face::PosY)));  // top
    CHECK(hasTile(mesh, faceTile(Block::Grass, Face::NegY)));  // bottom
    CHECK(hasTile(mesh, faceTile(Block::Grass, Face::PosX)));  // side
    CHECK(faceTile(Block::Grass, Face::PosY) != faceTile(Block::Grass, Face::PosX));
}

VF_TEST(meshEmptyChunkProducesNothing) {
    World world;
    world.ensureChunk({0, 0, 0});
    Mesh mesh = greedyMeshChunk(world, {0, 0, 0});
    CHECK(mesh.empty());
    CHECK_EQ(mesh.quadCount(), static_cast<std::size_t>(0));
}
