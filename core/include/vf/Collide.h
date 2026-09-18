#pragma once

#include <cstdint>
#include <vector>

#include "Block.h"
#include "Math.h"
#include "Physics.h"
#include "World.h"

namespace vf {

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
SweptBox motionEnvelope(const SweptBox& box, const Vec3& motion);
bool boxesOverlap(const SweptBox& a, const SweptBox& b);
bool boxesTouchOrOverlap(const SweptBox& a, const SweptBox& b, float tolerance);

SweptBox aabbToSweptBox(const Aabb& box);
Aabb sweptBoxToAabb(const SweptBox& box);

enum class ContactAxis : uint8_t { None = 0, X = 1, Y = 2, Z = 3 };

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
bool isClimbableShape(CollisionShape shape);
bool shapeBlocksMovement(CollisionShape shape);

constexpr int maxShapeBoxes = 2;
constexpr float fencePostHeight = 1.5f;
constexpr float carpetHeight = 0.125f;
constexpr int shapeOverhangCells = 1;

struct ShapeBoxes {
    SweptBox boxes[maxShapeBoxes];
    int count = 0;
};

ShapeBoxes shapeBoxesAt(CollisionShape shape, const Vec3i& cell);

struct SweepHit {
    bool hit = false;
    float time = 1.0f;
    ContactAxis axis = ContactAxis::None;
    Vec3 normal;
    Vec3i cell;
    Block block = Block::Air;
};

SweepHit sweepBoxAgainstBox(const SweptBox& mover, const Vec3& motion, const SweptBox& obstacle);

struct BroadPhaseCell {
    Vec3i cell;
    Block block = Block::Air;
    CollisionShape shape = CollisionShape::None;
    ShapeBoxes solids;
};

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

constexpr float contactSkin = 1.0e-4f;
constexpr float groundProbeDepth = 2.0e-3f;
constexpr int maxSlidePasses = 4;

bool boxOverlapsWorld(const World& world, const SweptBox& box);
bool restsOnGround(const World& world, const SweptBox& box, float probeDepth = groundProbeDepth);
float supportHeightBelow(const World& world, const SweptBox& box, float maxDrop);

CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion,
                            BroadPhase& broadPhase);
CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion);

Vec3 clipVelocity(const Vec3& velocity, const CollisionResult& collision);

}
