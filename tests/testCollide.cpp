#include "testHarness.h"
#include "vf/Collide.h"

using namespace vf;

namespace {

constexpr float tolerance = 1.0e-3f;

World stoneFloor(int size) {
    World world;
    for (int x = 0; x < size; ++x)
        for (int z = 0; z < size; ++z)
            world.setBlock(x, 0, z, Block::Stone);
    return world;
}

SweptBox unitBox(float x, float y, float z) { return SweptBox{{x, y, z}, {x + 1.0f, y + 1.0f, z + 1.0f}}; }

float landingHeight(const World& world, const Vec3& feet, float width) {
    SweptBox box = makeBoxAtFeet(feet, width, 1.8f);
    CollisionResult moved = resolveMove(world, box, Vec3{0.0f, -4.0f, 0.0f});
    return moved.box.min.y;
}

}

VF_TEST(collideBoxHelpersBuildAndMoveBoxes) {
    SweptBox box = makeBoxAtFeet({2.0f, 1.0f, 3.0f}, 0.6f, 1.8f);
    CHECK_NEAR(box.min.x, 1.7f, 1e-5f);
    CHECK_NEAR(box.max.z, 3.3f, 1e-5f);
    CHECK_NEAR(box.min.y, 1.0f, 1e-5f);
    CHECK_NEAR(box.max.y, 2.8f, 1e-5f);
    CHECK_NEAR(box.volume(), 0.6f * 0.6f * 1.8f, 1e-5f);
    CHECK_NEAR(box.centre().y, 1.9f, 1e-5f);

    SweptBox moved = translatedBox(box, {1.0f, -0.5f, 0.0f});
    CHECK_NEAR(moved.min.x, 2.7f, 1e-5f);
    CHECK_NEAR(moved.min.y, 0.5f, 1e-5f);

    SweptBox padded = inflatedBox(box, 0.25f);
    CHECK_NEAR(padded.min.x, 1.45f, 1e-5f);
    CHECK_NEAR(padded.max.y, 3.05f, 1e-5f);

    SweptBox envelope = motionEnvelope(box, {-1.0f, 2.0f, 0.0f});
    CHECK_NEAR(envelope.min.x, 0.7f, 1e-5f);
    CHECK_NEAR(envelope.max.x, 2.3f, 1e-5f);
    CHECK_NEAR(envelope.min.y, 1.0f, 1e-5f);
    CHECK_NEAR(envelope.max.y, 4.8f, 1e-5f);

    Aabb aabb = sweptBoxToAabb(box);
    SweptBox back = aabbToSweptBox(aabb);
    CHECK_NEAR(back.min.x, box.min.x, 1e-6f);
    CHECK_NEAR(back.max.y, box.max.y, 1e-6f);
}

VF_TEST(collideTouchingBoxesDoNotOverlap) {
    SweptBox a = unitBox(0.0f, 0.0f, 0.0f);
    CHECK(boxesOverlap(a, unitBox(0.5f, 0.5f, 0.5f)));
    CHECK(!boxesOverlap(a, unitBox(1.0f, 0.0f, 0.0f)));
    CHECK(!boxesOverlap(a, unitBox(0.0f, 1.0f, 0.0f)));
    CHECK(boxesTouchOrOverlap(a, unitBox(1.0f, 0.0f, 0.0f), 1.0e-3f));
    CHECK(!boxesTouchOrOverlap(a, unitBox(1.1f, 0.0f, 0.0f), 1.0e-3f));
}

VF_TEST(collideShapesFollowTheBlockType) {
    CHECK(collisionShapeOf(Block::Stone) == CollisionShape::Full);
    CHECK(collisionShapeOf(Block::Air) == CollisionShape::None);
    CHECK(collisionShapeOf(Block::Water) == CollisionShape::None);
    CHECK(collisionShapeOf(Block::Slab) == CollisionShape::SlabBottom);
    CHECK(collisionShapeOf(Block::Stairs) == CollisionShape::Stairs);
    CHECK(collisionShapeOf(Block::Fence) == CollisionShape::Fence);
    CHECK(collisionShapeOf(Block::Ladder) == CollisionShape::Ladder);
    CHECK(collisionShapeOf(Block::Vine) == CollisionShape::Vine);
    CHECK(collisionShapeOf(Block::SnowLayer) == CollisionShape::Carpet);

    CHECK(isClimbableShape(CollisionShape::Ladder));
    CHECK(isClimbableShape(CollisionShape::Vine));
    CHECK(!isClimbableShape(CollisionShape::Full));
    CHECK(!shapeBlocksMovement(CollisionShape::None));
    CHECK(!shapeBlocksMovement(CollisionShape::Ladder));
    CHECK(!shapeBlocksMovement(CollisionShape::Vine));
    CHECK(shapeBlocksMovement(CollisionShape::Carpet));
    CHECK(shapeBlocksMovement(CollisionShape::Fence));
}

VF_TEST(collideShapeBoxesHaveTheirGeometry) {
    Vec3i cell{2, 5, -3};

    ShapeBoxes full = shapeBoxesAt(CollisionShape::Full, cell);
    CHECK_EQ(full.count, 1);
    CHECK_NEAR(full.boxes[0].min.y, 5.0f, 1e-6f);
    CHECK_NEAR(full.boxes[0].max.y, 6.0f, 1e-6f);
    CHECK_NEAR(full.boxes[0].min.z, -3.0f, 1e-6f);

    ShapeBoxes slab = shapeBoxesAt(CollisionShape::SlabBottom, cell);
    CHECK_EQ(slab.count, 1);
    CHECK_NEAR(slab.boxes[0].max.y, 5.5f, 1e-6f);

    ShapeBoxes top = shapeBoxesAt(CollisionShape::SlabTop, cell);
    CHECK_EQ(top.count, 1);
    CHECK_NEAR(top.boxes[0].min.y, 5.5f, 1e-6f);
    CHECK_NEAR(top.boxes[0].max.y, 6.0f, 1e-6f);

    ShapeBoxes stairs = shapeBoxesAt(CollisionShape::Stairs, cell);
    CHECK_EQ(stairs.count, 2);
    CHECK_NEAR(stairs.boxes[0].max.y, 5.5f, 1e-6f);
    CHECK_NEAR(stairs.boxes[1].min.x, 2.5f, 1e-6f);
    CHECK_NEAR(stairs.boxes[1].min.y, 5.5f, 1e-6f);
    CHECK_NEAR(stairs.boxes[1].max.y, 6.0f, 1e-6f);

    ShapeBoxes fence = shapeBoxesAt(CollisionShape::Fence, cell);
    CHECK_EQ(fence.count, 1);
    CHECK_NEAR(fence.boxes[0].min.x, 2.375f, 1e-6f);
    CHECK_NEAR(fence.boxes[0].max.x, 2.625f, 1e-6f);
    CHECK_NEAR(fence.boxes[0].max.y, 5.0f + fencePostHeight, 1e-6f);

    ShapeBoxes carpet = shapeBoxesAt(CollisionShape::Carpet, cell);
    CHECK_EQ(carpet.count, 1);
    CHECK_NEAR(carpet.boxes[0].max.y, 5.0f + carpetHeight, 1e-6f);

    CHECK_EQ(shapeBoxesAt(CollisionShape::None, cell).count, 0);
    CHECK_EQ(shapeBoxesAt(CollisionShape::Ladder, cell).count, 0);
    CHECK_EQ(shapeBoxesAt(CollisionShape::Vine, cell).count, 0);
}

VF_TEST(collideSweepFindsEntryTimeAndNormal) {
    SweptBox mover = unitBox(0.0f, 0.0f, 0.0f);
    SweptBox obstacle = unitBox(2.0f, 0.0f, 0.0f);

    SweepHit hit = sweepBoxAgainstBox(mover, {4.0f, 0.0f, 0.0f}, obstacle);
    CHECK(hit.hit);
    CHECK_NEAR(hit.time, 0.25f, 1e-6f);
    CHECK(hit.axis == ContactAxis::X);
    CHECK_NEAR(hit.normal.x, -1.0f, 1e-6f);
    CHECK_NEAR(hit.normal.y, 0.0f, 1e-6f);

    SweepHit falling = sweepBoxAgainstBox(unitBox(0.0f, 3.0f, 0.0f), {0.0f, -4.0f, 0.0f},
                                          unitBox(0.0f, 0.0f, 0.0f));
    CHECK(falling.hit);
    CHECK_NEAR(falling.time, 0.5f, 1e-6f);
    CHECK(falling.axis == ContactAxis::Y);
    CHECK_NEAR(falling.normal.y, 1.0f, 1e-6f);
}

VF_TEST(collideSweepMissesWhatItDoesNotReach) {
    SweptBox mover = unitBox(0.0f, 0.0f, 0.0f);
    CHECK(!sweepBoxAgainstBox(mover, {0.5f, 0.0f, 0.0f}, unitBox(2.0f, 0.0f, 0.0f)).hit);
    CHECK(!sweepBoxAgainstBox(mover, {4.0f, 0.0f, 0.0f}, unitBox(2.0f, 2.0f, 0.0f)).hit);
    CHECK(!sweepBoxAgainstBox(mover, {1.0f, 0.0f, 0.0f}, unitBox(0.0f, 1.0f, 0.0f)).hit);
    CHECK(!sweepBoxAgainstBox(mover, {-4.0f, 0.0f, 0.0f}, unitBox(2.0f, 0.0f, 0.0f)).hit);
    CHECK(!sweepBoxAgainstBox(mover, {0.0f, 0.0f, 0.0f}, unitBox(1.0f, 0.0f, 0.0f)).hit);
}

VF_TEST(collideBroadPhaseGathersBlockingCellsOnly) {
    World world = stoneFloor(4);
    world.setBlock(1, 1, 2, Block::Ladder);
    SweptBox box = makeBoxAtFeet({1.5f, 1.0f, 1.5f}, 0.6f, 1.8f);

    BroadPhase broadPhase;
    broadPhase.gather(world, box, {0.0f, -0.5f, 1.0f});
    CHECK(broadPhase.size() > 0u);
    CHECK_NEAR(broadPhase.envelope().min.y, 0.5f - 1.0e-3f, 1e-5f);
    CHECK_NEAR(broadPhase.envelope().max.z, 2.8f + 1.0e-3f, 1e-5f);

    bool sawFloor = false;
    for (const BroadPhaseCell& entry : broadPhase.cells()) {
        CHECK(shapeBlocksMovement(entry.shape));
        CHECK(entry.block != Block::Ladder);
        if (entry.cell == Vec3i{1, 0, 1}) {
            sawFloor = true;
            CHECK(entry.block == Block::Stone);
            CHECK(entry.shape == CollisionShape::Full);
            CHECK_EQ(entry.solids.count, 1);
        }
    }
    CHECK(sawFloor);

    broadPhase.clear();
    CHECK_EQ(broadPhase.size(), 0u);
}

VF_TEST(collideSweepAgainstWorldReportsTheStruckCell) {
    World world = stoneFloor(4);
    BroadPhase broadPhase;
    SweptBox box = makeBoxAtFeet({1.5f, 3.0f, 2.5f}, 0.6f, 1.8f);

    SweepHit hit = sweepBoxAgainstWorld(world, box, {0.0f, -5.0f, 0.0f}, broadPhase);
    CHECK(hit.hit);
    CHECK_NEAR(hit.time, 0.4f, 1e-5f);
    CHECK((hit.cell == Vec3i{1, 0, 2}));
    CHECK(hit.block == Block::Stone);
    CHECK_NEAR(hit.normal.y, 1.0f, 1e-6f);

    CHECK(!sweepBoxAgainstWorld(world, box, {0.0f, 1.0f, 0.0f}, broadPhase).hit);
    CHECK(!sweepBoxAgainstWorld(world, box, {0.0f, 0.0f, 0.0f}, broadPhase).hit);
}

VF_TEST(collideResolveMoveLandsOnTheFloor) {
    World world = stoneFloor(4);
    SweptBox box = makeBoxAtFeet({1.5f, 3.0f, 1.5f}, 0.6f, 1.8f);

    CollisionResult moved = resolveMove(world, box, {0.0f, -5.0f, 0.0f});
    CHECK(moved.firstHit.hit);
    CHECK(moved.onGround);
    CHECK(moved.touchedY);
    CHECK(!moved.touchedX);
    CHECK(!moved.hitCeiling);
    CHECK_NEAR(moved.box.min.y, 1.0f + contactSkin, 1e-4f);
    CHECK_NEAR(moved.appliedMotion.y, -2.0f, tolerance);
    CHECK_NEAR(moved.remainingMotion.y, 0.0f, 1e-6f);
    CHECK(!boxOverlapsWorld(world, moved.box));

    Vec3 velocity = clipVelocity({0.3f, -9.0f, 0.2f}, moved);
    CHECK_NEAR(velocity.x, 0.3f, 1e-6f);
    CHECK_NEAR(velocity.y, 0.0f, 1e-6f);
    CHECK_NEAR(velocity.z, 0.2f, 1e-6f);
}

VF_TEST(collideResolveMoveSlidesAlongAWall) {
    World world;
    for (int y = 1; y <= 2; ++y)
        for (int z = 0; z <= 5; ++z)
            world.setBlock(3, y, z, Block::Stone);
    SweptBox box = makeBoxAtFeet({2.0f, 1.0f, 2.5f}, 0.6f, 1.8f);

    CollisionResult moved = resolveMove(world, box, {1.0f, 0.0f, 0.5f});
    CHECK(moved.touchedX);
    CHECK(!moved.touchedZ);
    CHECK(!moved.onGround);
    CHECK(moved.firstHit.axis == ContactAxis::X);
    CHECK_NEAR(moved.firstHit.time, 0.7f, 1e-4f);
    CHECK_NEAR(moved.box.max.x, 3.0f - contactSkin, 1e-4f);
    CHECK_NEAR(moved.box.min.z, 2.7f, 1e-4f);
    CHECK_EQ(moved.passes, 2);
    CHECK(!boxOverlapsWorld(world, moved.box));

    Vec3 velocity = clipVelocity({4.0f, 0.0f, 2.0f}, moved);
    CHECK_NEAR(velocity.x, 0.0f, 1e-6f);
    CHECK_NEAR(velocity.z, 2.0f, 1e-6f);
}

VF_TEST(collideResolveMoveStopsAtTheCeiling) {
    World world;
    world.setBlock(0, 4, 0, Block::Stone);
    SweptBox box = makeBoxAtFeet({0.5f, 1.0f, 0.5f}, 0.6f, 1.8f);

    CollisionResult moved = resolveMove(world, box, {0.0f, 3.0f, 0.0f});
    CHECK(moved.hitCeiling);
    CHECK(!moved.onGround);
    CHECK_NEAR(moved.box.max.y, 4.0f - contactSkin, 1e-4f);
}

VF_TEST(collideResolveMoveRespectsPartialShapes) {
    World world;
    world.setBlock(0, 0, 0, Block::Slab);
    world.setBlock(2, 0, 0, Block::Stairs);
    world.setBlock(4, 0, 0, Block::Fence);
    world.setBlock(6, 0, 0, Block::SnowLayer);

    CHECK_NEAR(landingHeight(world, {0.5f, 2.0f, 0.5f}, 0.6f), 0.5f + contactSkin, 1e-4f);
    CHECK_NEAR(landingHeight(world, {2.25f, 2.0f, 0.5f}, 0.3f), 0.5f + contactSkin, 1e-4f);
    CHECK_NEAR(landingHeight(world, {2.75f, 2.0f, 0.5f}, 0.3f), 1.0f + contactSkin, 1e-4f);
    CHECK_NEAR(landingHeight(world, {4.5f, 3.0f, 0.5f}, 0.6f), fencePostHeight + contactSkin, 1e-4f);
    CHECK_NEAR(landingHeight(world, {6.5f, 2.0f, 0.5f}, 0.6f), carpetHeight + contactSkin, 1e-4f);
}

VF_TEST(collideFencePostBlocksFromTheCellAbove) {
    World world;
    world.setBlock(0, 0, 0, Block::Fence);
    SweptBox box = makeBoxAtFeet({-1.0f, 1.0f, 0.5f}, 0.6f, 1.8f);

    CollisionResult moved = resolveMove(world, box, {2.0f, 0.0f, 0.0f});
    CHECK(moved.touchedX);
    CHECK(moved.firstHit.block == Block::Fence);
    CHECK((moved.firstHit.cell == Vec3i{0, 0, 0}));
    CHECK_NEAR(moved.box.max.x, 0.375f - contactSkin, 1e-4f);
}

VF_TEST(collideClimbablesDoNotBlock) {
    World world;
    world.setBlock(1, 1, 0, Block::Ladder);
    world.setBlock(1, 2, 0, Block::Vine);
    SweptBox box = makeBoxAtFeet({0.0f, 1.0f, 0.5f}, 0.6f, 1.8f);

    CollisionResult moved = resolveMove(world, box, {2.0f, 0.0f, 0.0f});
    CHECK(!moved.firstHit.hit);
    CHECK(!moved.touchedX);
    CHECK_EQ(moved.passes, 1);
    CHECK_NEAR(moved.box.min.x, 1.7f, 1e-5f);
    CHECK_NEAR(moved.appliedMotion.x, 2.0f, 1e-5f);
}

VF_TEST(collideGroundProbesAndSupportHeight) {
    World world = stoneFloor(4);

    CHECK(restsOnGround(world, makeBoxAtFeet({1.5f, 1.0005f, 1.5f}, 0.6f, 1.8f)));
    CHECK(!restsOnGround(world, makeBoxAtFeet({1.5f, 1.01f, 1.5f}, 0.6f, 1.8f)));
    CHECK(!restsOnGround(world, makeBoxAtFeet({8.5f, 1.0f, 8.5f}, 0.6f, 1.8f)));

    CHECK(boxOverlapsWorld(world, makeBoxAtFeet({1.5f, 0.5f, 1.5f}, 0.6f, 1.8f)));
    CHECK(!boxOverlapsWorld(world, makeBoxAtFeet({1.5f, 1.0f, 1.5f}, 0.6f, 1.8f)));

    CHECK_NEAR(supportHeightBelow(world, makeBoxAtFeet({1.5f, 3.0f, 1.5f}, 0.6f, 1.8f), 5.0f), 1.0f,
               1e-4f);
    CHECK_NEAR(supportHeightBelow(world, makeBoxAtFeet({1.5f, 9.0f, 1.5f}, 0.6f, 1.8f), 5.0f), 4.0f,
               1e-4f);
}
