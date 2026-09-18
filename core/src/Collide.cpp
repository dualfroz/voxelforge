#include "vf/Collide.h"

#include <cmath>
#include <limits>

namespace vf {

namespace {

constexpr float gatherPadding = 1.0e-3f;
constexpr float parallelMotion = 1.0e-12f;

float component(const Vec3& v, int a) { return (&v.x)[a]; }
void setComponent(Vec3& v, int a, float value) { (&v.x)[a] = value; }

int axisIndex(ContactAxis axis) {
    switch (axis) {
        case ContactAxis::X: return 0;
        case ContactAxis::Y: return 1;
        case ContactAxis::Z: return 2;
        default: return -1;
    }
}

ContactAxis axisFromIndex(int a) {
    switch (a) {
        case 0: return ContactAxis::X;
        case 1: return ContactAxis::Y;
        default: return ContactAxis::Z;
    }
}

int floorToCell(float value) { return static_cast<int>(std::floor(value)); }

bool motionIsIdle(const Vec3& motion) {
    return std::fabs(motion.x) < parallelMotion && std::fabs(motion.y) < parallelMotion &&
           std::fabs(motion.z) < parallelMotion;
}

SweptBox unitCellBox(const Vec3i& cell, float loY, float hiY, float inset) {
    float x = static_cast<float>(cell.x);
    float y = static_cast<float>(cell.y);
    float z = static_cast<float>(cell.z);
    return SweptBox{{x + inset, y + loY, z + inset}, {x + 1.0f - inset, y + hiY, z + 1.0f - inset}};
}

}

SweptBox makeBoxAtFeet(const Vec3& feetCentre, float width, float height) {
    float half = width * 0.5f;
    return SweptBox{{feetCentre.x - half, feetCentre.y, feetCentre.z - half},
                    {feetCentre.x + half, feetCentre.y + height, feetCentre.z + half}};
}

SweptBox translatedBox(const SweptBox& box, const Vec3& delta) {
    return SweptBox{box.min + delta, box.max + delta};
}

SweptBox inflatedBox(const SweptBox& box, float amount) {
    Vec3 pad{amount, amount, amount};
    return SweptBox{box.min - pad, box.max + pad};
}

SweptBox motionEnvelope(const SweptBox& box, const Vec3& motion) {
    SweptBox moved = translatedBox(box, motion);
    SweptBox out;
    for (int a = 0; a < 3; ++a) {
        setComponent(out.min, a, std::fmin(component(box.min, a), component(moved.min, a)));
        setComponent(out.max, a, std::fmax(component(box.max, a), component(moved.max, a)));
    }
    return out;
}

bool boxesOverlap(const SweptBox& a, const SweptBox& b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}

bool boxesTouchOrOverlap(const SweptBox& a, const SweptBox& b, float tolerance) {
    return boxesOverlap(inflatedBox(a, tolerance), b);
}

SweptBox aabbToSweptBox(const Aabb& box) { return SweptBox{box.min, box.max}; }

Aabb sweptBoxToAabb(const SweptBox& box) { return Aabb{box.min, box.max}; }

CollisionShape collisionShapeOf(Block block) {
    switch (block) {
        case Block::Slab:      return CollisionShape::SlabBottom;
        case Block::Stairs:    return CollisionShape::Stairs;
        case Block::Fence:     return CollisionShape::Fence;
        case Block::Ladder:    return CollisionShape::Ladder;
        case Block::Vine:      return CollisionShape::Vine;
        case Block::SnowLayer: return CollisionShape::Carpet;
        default:               return isSolid(block) ? CollisionShape::Full : CollisionShape::None;
    }
}

bool isClimbableShape(CollisionShape shape) {
    return shape == CollisionShape::Ladder || shape == CollisionShape::Vine;
}

bool shapeBlocksMovement(CollisionShape shape) {
    switch (shape) {
        case CollisionShape::None:
        case CollisionShape::Ladder:
        case CollisionShape::Vine:
            return false;
        default:
            return true;
    }
}

ShapeBoxes shapeBoxesAt(CollisionShape shape, const Vec3i& cell) {
    ShapeBoxes out;
    switch (shape) {
        case CollisionShape::Full:
            out.boxes[out.count++] = unitCellBox(cell, 0.0f, 1.0f, 0.0f);
            break;
        case CollisionShape::SlabBottom:
            out.boxes[out.count++] = unitCellBox(cell, 0.0f, 0.5f, 0.0f);
            break;
        case CollisionShape::SlabTop:
            out.boxes[out.count++] = unitCellBox(cell, 0.5f, 1.0f, 0.0f);
            break;
        case CollisionShape::Stairs:
            out.boxes[out.count++] = unitCellBox(cell, 0.0f, 0.5f, 0.0f);
            {
                SweptBox upper = unitCellBox(cell, 0.5f, 1.0f, 0.0f);
                upper.min.x = static_cast<float>(cell.x) + 0.5f;
                out.boxes[out.count++] = upper;
            }
            break;
        case CollisionShape::Fence:
            out.boxes[out.count++] = unitCellBox(cell, 0.0f, fencePostHeight, 0.375f);
            break;
        case CollisionShape::Carpet:
            out.boxes[out.count++] = unitCellBox(cell, 0.0f, carpetHeight, 0.0f);
            break;
        case CollisionShape::None:
        case CollisionShape::Ladder:
        case CollisionShape::Vine:
            break;
    }
    return out;
}

SweepHit sweepBoxAgainstBox(const SweptBox& mover, const Vec3& motion, const SweptBox& obstacle) {
    SweepHit hit;
    Vec3 moverSize = mover.size();
    SweptBox expanded{obstacle.min - moverSize, obstacle.max};
    Vec3 point = mover.min;

    float enter = -std::numeric_limits<float>::infinity();
    float exit = std::numeric_limits<float>::infinity();
    int enterAxis = -1;
    float enterSign = 0.0f;

    for (int a = 0; a < 3; ++a) {
        float origin = component(point, a);
        float travel = component(motion, a);
        float lo = component(expanded.min, a);
        float hi = component(expanded.max, a);
        if (std::fabs(travel) < parallelMotion) {
            if (origin <= lo || origin >= hi) return hit;
            continue;
        }
        float toLow = (lo - origin) / travel;
        float toHigh = (hi - origin) / travel;
        float axisEnter = toLow;
        float axisExit = toHigh;
        float sign = -1.0f;
        if (toLow > toHigh) {
            axisEnter = toHigh;
            axisExit = toLow;
            sign = 1.0f;
        }
        if (axisEnter > enter) {
            enter = axisEnter;
            enterAxis = a;
            enterSign = sign;
        }
        if (axisExit < exit) exit = axisExit;
    }

    if (enterAxis < 0) return hit;
    if (enter > exit || enter >= 1.0f || exit <= 0.0f || enter < 0.0f) return hit;

    hit.hit = true;
    hit.time = enter;
    hit.axis = axisFromIndex(enterAxis);
    setComponent(hit.normal, enterAxis, enterSign);
    return hit;
}

void BroadPhase::gather(const World& world, const SweptBox& box, const Vec3& motion) {
    cells_.clear();
    envelope_ = inflatedBox(motionEnvelope(box, motion), gatherPadding);

    int x0 = floorToCell(envelope_.min.x);
    int x1 = floorToCell(envelope_.max.x);
    int y0 = floorToCell(envelope_.min.y) - shapeOverhangCells;
    int y1 = floorToCell(envelope_.max.y);
    int z0 = floorToCell(envelope_.min.z);
    int z1 = floorToCell(envelope_.max.z);

    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                Vec3i cell{x, y, z};
                Block block = world.blockAt(x, y, z);
                CollisionShape shape = collisionShapeOf(block);
                if (!shapeBlocksMovement(shape)) continue;
                ShapeBoxes solids = shapeBoxesAt(shape, cell);
                bool relevant = false;
                for (int i = 0; i < solids.count; ++i) {
                    if (boxesOverlap(envelope_, solids.boxes[i])) relevant = true;
                }
                if (!relevant) continue;
                cells_.push_back(BroadPhaseCell{cell, block, shape, solids});
            }
        }
    }
}

void BroadPhase::gatherStatic(const World& world, const SweptBox& box) {
    gather(world, box, Vec3{0.0f, 0.0f, 0.0f});
}

SweepHit sweepBoxAgainstWorld(const World& world, const SweptBox& box, const Vec3& motion,
                              BroadPhase& broadPhase) {
    broadPhase.gather(world, box, motion);
    SweepHit best;
    if (motionIsIdle(motion)) return best;

    for (const BroadPhaseCell& entry : broadPhase.cells()) {
        for (int i = 0; i < entry.solids.count; ++i) {
            SweepHit candidate = sweepBoxAgainstBox(box, motion, entry.solids.boxes[i]);
            if (!candidate.hit) continue;
            if (best.hit && candidate.time >= best.time) continue;
            candidate.cell = entry.cell;
            candidate.block = entry.block;
            best = candidate;
        }
    }
    return best;
}

bool boxOverlapsWorld(const World& world, const SweptBox& box) {
    BroadPhase broadPhase;
    broadPhase.gatherStatic(world, box);
    for (const BroadPhaseCell& entry : broadPhase.cells()) {
        for (int i = 0; i < entry.solids.count; ++i) {
            if (boxesOverlap(box, entry.solids.boxes[i])) return true;
        }
    }
    return false;
}

bool restsOnGround(const World& world, const SweptBox& box, float probeDepth) {
    SweptBox probe = box;
    probe.max.y = box.min.y;
    probe.min.y = box.min.y - probeDepth;
    probe.min.x += contactSkin;
    probe.min.z += contactSkin;
    probe.max.x -= contactSkin;
    probe.max.z -= contactSkin;
    return boxOverlapsWorld(world, probe);
}

float supportHeightBelow(const World& world, const SweptBox& box, float maxDrop) {
    BroadPhase broadPhase;
    Vec3 drop{0.0f, -maxDrop, 0.0f};
    SweepHit hit = sweepBoxAgainstWorld(world, box, drop, broadPhase);
    if (!hit.hit) return box.min.y - maxDrop;
    return box.min.y - maxDrop * hit.time;
}

CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion,
                            BroadPhase& broadPhase) {
    CollisionResult out;
    out.box = box;
    Vec3 remaining = motion;

    for (int pass = 0; pass < maxSlidePasses; ++pass) {
        if (motionIsIdle(remaining)) break;
        out.passes = pass + 1;

        SweepHit hit = sweepBoxAgainstWorld(world, out.box, remaining, broadPhase);
        if (!hit.hit) {
            out.box = translatedBox(out.box, remaining);
            out.appliedMotion += remaining;
            remaining = Vec3{0.0f, 0.0f, 0.0f};
            break;
        }
        if (!out.firstHit.hit) out.firstHit = hit;

        Vec3 step = remaining * hit.time;
        Vec3 leftover = remaining - step;

        int a = axisIndex(hit.axis);
        float backedOff = component(step, a) + component(hit.normal, a) * contactSkin;
        if (backedOff * component(remaining, a) < 0.0f) backedOff = 0.0f;
        setComponent(step, a, backedOff);

        out.box = translatedBox(out.box, step);
        out.appliedMotion += step;

        switch (hit.axis) {
            case ContactAxis::X: out.touchedX = true; break;
            case ContactAxis::Y:
                out.touchedY = true;
                if (hit.normal.y > 0.0f) out.onGround = true;
                if (hit.normal.y < 0.0f) out.hitCeiling = true;
                break;
            case ContactAxis::Z: out.touchedZ = true; break;
            case ContactAxis::None: break;
        }

        setComponent(leftover, a, 0.0f);
        remaining = leftover;
    }

    out.remainingMotion = remaining;
    if (!out.onGround && restsOnGround(world, out.box)) out.onGround = true;
    return out;
}

CollisionResult resolveMove(const World& world, const SweptBox& box, const Vec3& motion) {
    BroadPhase broadPhase;
    return resolveMove(world, box, motion, broadPhase);
}

Vec3 clipVelocity(const Vec3& velocity, const CollisionResult& collision) {
    Vec3 out = velocity;
    if (collision.touchedX) out.x = 0.0f;
    if (collision.touchedY) out.y = 0.0f;
    if (collision.touchedZ) out.z = 0.0f;
    return out;
}

}
