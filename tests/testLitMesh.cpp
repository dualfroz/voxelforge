#include "testHarness.h"
#include "vf/LitMesh.h"

using namespace vf;

namespace {

LightField lightFor(const World& world) {
    return computeLight(world, {0, 0, 0}, {chunkSize, chunkSize, chunkSize});
}

float minAo(const Mesh& mesh) {
    float lo = 1.0f;
    for (const Vertex& v : mesh.vertices) lo = v.ao < lo ? v.ao : lo;
    return lo;
}

// Average light of the top face of the block at (5,5,5): its four vertices sit
// at y = 6 with an upward normal. Isolating it ignores the roof's own lit faces.
float coveredBlockTopLight(const Mesh& mesh) {
    float sum = 0.0f;
    int n = 0;
    for (const Vertex& v : mesh.vertices) {
        if (std::fabs(v.py - 6.0f) < 1e-3f && v.ny > 0.5f) {
            sum += v.light;
            ++n;
        }
    }
    return n > 0 ? sum / static_cast<float>(n) : -1.0f;
}

bool hasTile(const Mesh& mesh, int tile) {
    for (const Vertex& v : mesh.vertices)
        if (static_cast<int>(v.tile) == tile) return true;
    return false;
}

} // namespace

VF_TEST(litIsolatedBlockHasSixUnoccludedFaces) {
    World world;
    world.setBlock(5, 5, 5, Block::Stone);
    ChunkMesh mesh = meshChunkLit(world, {0, 0, 0}, lightFor(world));
    CHECK_EQ(mesh.opaque.quadCount(), static_cast<std::size_t>(6));
    CHECK(mesh.transparent.empty());
    CHECK_NEAR(minAo(mesh.opaque), 1.0f, 1e-4f);   // nothing occludes an isolated block
}

VF_TEST(litInnerCornerDarkensVertex) {
    World world;
    world.setBlock(5, 5, 5, Block::Stone);
    world.setBlock(6, 6, 5, Block::Stone);   // occludes one edge of the top face
    world.setBlock(5, 6, 6, Block::Stone);   // occludes the other edge
    ChunkMesh mesh = meshChunkLit(world, {0, 0, 0}, lightFor(world));
    CHECK_NEAR(minAo(mesh.opaque), 0.0f, 1e-4f);   // both sides set -> AO 0 at that corner
}

VF_TEST(litTopIsBrightRoofIsNot) {
    World open;
    open.setBlock(5, 5, 5, Block::Stone);
    ChunkMesh openMesh = meshChunkLit(open, {0, 0, 0}, lightFor(open));
    CHECK(coveredBlockTopLight(openMesh.opaque) > 0.99f);   // top face sees open sky

    World roofed;
    roofed.setBlock(5, 5, 5, Block::Stone);
    for (int x = 3; x <= 7; ++x)
        for (int z = 3; z <= 7; ++z)
            roofed.setBlock(x, 8, z, Block::Stone);
    ChunkMesh roofedMesh = meshChunkLit(roofed, {0, 0, 0}, lightFor(roofed));
    CHECK(coveredBlockTopLight(roofedMesh.opaque) < 0.99f);   // roof shadows this face
}

VF_TEST(litWaterGoesToTransparentPass) {
    World world;
    world.setBlock(2, 2, 2, Block::Stone);
    world.setBlock(5, 5, 5, Block::Water);
    ChunkMesh mesh = meshChunkLit(world, {0, 0, 0}, lightFor(world));
    CHECK(!mesh.opaque.empty());
    CHECK(!mesh.transparent.empty());
    CHECK(hasTile(mesh.opaque, faceTile(Block::Stone, Face::PosY)));
    CHECK(hasTile(mesh.transparent, faceTile(Block::Water, Face::PosY)));
    CHECK(!hasTile(mesh.opaque, faceTile(Block::Water, Face::PosY)));
}
