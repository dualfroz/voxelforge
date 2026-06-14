#include "testHarness.h"
#include "vf/Raycast.h"

using namespace vf;

VF_TEST(raycastHitsBlockAlongNegativeZ) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {0.5f, 0.5f, 5.5f}, {0, 0, -1}, 20.0f);
    CHECK(hit.hit);
    CHECK(hit.block == Vec3i(0, 0, 0));
    CHECK(hit.normal == Vec3i(0, 0, 1));   // entered the +z face
    CHECK_NEAR(hit.distance, 4.5f, 1e-3f);  // origin z=5.5 to the face at z=1
}

VF_TEST(raycastMissReturnsNoHit) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {0.5f, 2.5f, 0.5f}, {0, 1, 0}, 20.0f);
    CHECK(!hit.hit);
}

VF_TEST(raycastNormalGivesPlacementCell) {
    World world;
    world.setBlock(5, 0, 5, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {5.5f, 0.5f, 10.0f}, {0, 0, -1}, 20.0f);
    CHECK(hit.hit);
    CHECK(hit.block == Vec3i(5, 0, 5));
    Vec3i placement = hit.block + hit.normal;
    CHECK(placement == Vec3i(5, 0, 6));
    CHECK(!world.solidAt(placement.x, placement.y, placement.z));
}

VF_TEST(raycastFromInsideSolidHitsImmediately) {
    World world;
    world.setBlock(3, 3, 3, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {3.5f, 3.5f, 3.5f}, {1, 0, 0}, 20.0f);
    CHECK(hit.hit);
    CHECK(hit.block == Vec3i(3, 3, 3));
    CHECK_NEAR(hit.distance, 0.0f, 1e-6f);
}

VF_TEST(raycastRespectsMaxDistance) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {0.5f, 0.5f, 20.0f}, {0, 0, -1}, 5.0f);
    CHECK(!hit.hit);
}

VF_TEST(raycastDiagonalHitsExpectedFace) {
    World world;
    world.setBlock(4, 0, 0, Block::Stone);
    RaycastHit hit = raycastVoxels(world, {0.5f, 0.5f, 0.5f}, {1, 0, 0}, 20.0f);
    CHECK(hit.hit);
    CHECK(hit.block == Vec3i(4, 0, 0));
    CHECK(hit.normal == Vec3i(-1, 0, 0));
}
