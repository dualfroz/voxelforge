#pragma once

#include <cstdint>
#include <vector>

#include "Block.h"
#include "Math.h"
#include "Physics.h"
#include "World.h"

namespace vf {

// Swept AABB collision against the voxel grid.
//
// This sits ABOVE Physics.h rather than replacing it. Physics.h moves a body one
// axis at a time and snaps it out of whatever voxel it ended up inside, which is
// cheap but has two limits this module removes: a body faster than one block per
// step passes straight through thin geometry, and every voxel is a full cube.
// Here the motion is swept, so the first surface along the path is always found
// no matter how long the step is, and each block contributes its own
// CollisionShape - a slab is half a cell, a fence post is taller than its cell.
//
// The box type is named SweptBox because vf::Aabb already exists in Physics.h and
// means the same thing there; aabbToSweptBox/sweptBoxToAabb bridge the two so a
// Player or Mob body can be handed straight to this resolver.

struct SweptBox {
    Vec3 min;
    Vec3 max;

    Vec3 size() const { return max - min; }
    Vec3 centre() const { return (min + max) * 0.5f; }
    float volume() const {
        Vec3 s = size();
        return s.x * s.y * s.z;
    }
};

SweptBox makeBoxAtFeet(const Vec3& feetCentre, float width, float height);
SweptBox translatedBox(const SweptBox& box, const Vec3& delta);
SweptBox inflatedBox(const SweptBox& box, float amount);
// Union of the box at its start and at its end: the outline the motion sweeps.
SweptBox motionEnvelope(const SweptBox& box, const Vec3& motion);
bool boxesOverlap(const SweptBox& a, const SweptBox& b);
bool boxesTouchOrOverlap(const SweptBox& a, const SweptBox& b, float tolerance);

SweptBox aabbToSweptBox(const Aabb& box);
Aabb sweptBoxToAabb(const SweptBox& box);

enum class ContactAxis : uint8_t { None = 0, X = 1, Y = 2, Z = 3 };

// Which sub-cell solid a block presents. Blocks whose shape is not a full cube
// carry their own entry here; everything else falls back to Full or None
// following Block.h's isSolid.
enum class CollisionShape : uint8_t {
    None = 0,
    Full,
    SlabBottom,
    SlabTop,
    Stairs,
    Fence,
    Carpet,
    Ladder,
    Vine,
};

CollisionShape collisionShapeOf(Block block);
// Ladders and vines are passed through but can be gripped; see Locomotion.
bool isClimbableShape(CollisionShape shape);
bool shapeBlocksMovement(CollisionShape shape);

constexpr int maxShapeBoxes = 2;
constexpr float fencePostHeight = 1.5f;
constexpr float carpetHeight = 0.125f;
// A fence stands taller than its own cell, so the broad phase has to look one
// cell further down than the swept envelope alone would suggest.
constexpr int shapeOverhangCells = 1;

struct ShapeBoxes {
    SweptBox boxes[maxShapeBoxes];
    int count = 0;
};

ShapeBoxes shapeBoxesAt(CollisionShape shape, const Vec3i& cell);

struct SweepHit {
    bool hit = false;
    float time = 1.0f;                        // 0..1 along the requested motion
    ContactAxis axis = ContactAxis::None;
    Vec3 normal;
    Vec3i cell;
    Block block = Block::Air;
};

// Earliest contact of `mover` travelling `motion` against a static box.
SweepHit sweepBoxAgainstBox(const SweptBox& mover, const Vec3& motion, const SweptBox& obstacle);

struct BroadPhaseCell {
    Vec3i cell;
    Block block = Block::Air;
    CollisionShape shape = CollisionShape::None;
    ShapeBoxes solids;
};

// Collects only the blocks the motion outline actually crosses. A 40 block step
// gathers the corridor it sweeps, never a radius around the body.
class BroadPhase {
public:
    void gather(const World& world, const SweptBox& box, const Vec3& motion);
    void gatherStatic(const World& world, const SweptBox& box);
    void clear() { cells_.clear(); }

    const std::vector<BroadPhaseCell>& cells() const { return cells_; }
    std::size_t size() const { return cells_.size(); }
    const SweptBox& envelope() const { return envelope_; }

private:
    std::vector<BroadPhaseCell> cells_;
    SweptBox envelope_;
};

SweepHit sweepBoxAgainstWorld(const World& world, const SweptBox& box, const Vec3& motion,
                              BroadPhase& broadPhase);

struct CollisionResult {
    SweptBox box;
    Vec3 remainingMotion;
    Vec3 appliedMotion;
    SweepHit firstHit;
    bool touchedX = false;
    bool touchedY = false;
    bool touchedZ = false;
    bool onGround = false;
    bool hitCeiling = false;
    int passes = 0;
};

// Contact slack kept between a resolved body and the surface it rests on, and
// the depth of the downward probe that decides onGround. The probe is an order
// of magnitude deeper than the slack, so a body parked at exactly y = 1.0 on top
// of a block whose face is exactly y = 1.0 still reads as grounded after any
// rounding the sweep introduced.
constexpr float contactSkin = 1.0e-4f;
constexpr float groundProbeDepth = 2.0e-3f;
constexpr int maxSlidePasses = 4;

bool boxOverlapsWorld(const World& world, const SweptBox& box);
bool restsOnGround(const World& world, const SweptBox& box, float probeDepth = groundProbeDepth);
// Highest solid surface under the box within `maxDrop`, or box.min.y - maxDrop
// when there is nothing to land on.
float supportHeightBelow(const World& world, const SweptBox& box, float maxDrop);

// Sweeps the box through `motion`, cutting one axis per contact in time order
// and keeping the components along the surface untouched, so a body entering a
// wall keeps sliding and a body entering a corner stops without sticking.
CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion,
                            BroadPhase& broadPhase);
CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion);

// Zeroes the velocity components that ran into something this step.
Vec3 clipVelocity(const Vec3& velocity, const CollisionResult& collision);

} // namespace vf
