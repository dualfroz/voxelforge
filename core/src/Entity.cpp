#include "vf/Entity.h"

#include <cmath>

namespace vf {

namespace {

constexpr float epsilon = 1e-4f;

float& axis(Vec3& v, int i) { return (&v.x)[i]; }

Aabb boxAt(const Mob& mob, const Vec3& position) {
    float half = mob.width * 0.5f;
    return Aabb{
        {position.x - half, position.y, position.z - half},
        {position.x + half, position.y + mob.height, position.z + half},
    };
}

bool moveAxis(const World& world, Mob& mob, int a, float disp) {
    axis(mob.position, a) += disp;
    Aabb box = boxAt(mob, mob.position);
    if (!boxOverlapsSolid(world, box)) return false;

    if (disp > 0.0f) {
        float snapped = std::floor(axis(box.max, a));
        axis(mob.position, a) -= (axis(box.max, a) - snapped) + epsilon;
    } else if (disp < 0.0f) {
        float snapped = std::ceil(axis(box.min, a));
        axis(mob.position, a) += (snapped - axis(box.min, a)) + epsilon;
    }
    return true;
}

float horizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Vec3 pickWander(uint32_t& rng) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    float angle = (static_cast<float>(rng) / 4294967295.0f) * 6.2831853f;
    return {std::cos(angle), 0.0f, std::sin(angle)};
}

Vec3 wanderDirection(Mob& mob) {
    if (length(mob.wanderDir) < 0.1f) {
        mob.wanderDir = pickWander(mob.rng);
    } else {
        uint32_t r = mob.rng;
        r ^= r << 13;
        r ^= r >> 17;
        r ^= r << 5;
        mob.rng = r;
        if ((r & 31u) == 0) mob.wanderDir = pickWander(mob.rng);
    }
    return mob.wanderDir;
}

Vec3i feetCellOf(const Vec3& position) {
    return {static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)),
            static_cast<int>(std::floor(position.z))};
}

}

Aabb mobBox(const Mob& mob) { return boxAt(mob, mob.position); }

Vec3i mobFeetCell(const Mob& mob) { return feetCellOf(mob.position); }

void stepMob(const World& world, Mob& mob, const Vec3& wishDir, const MobConfig& config, float dt) {
    mob.velocity.x = wishDir.x * config.moveSpeed;
    mob.velocity.z = wishDir.z * config.moveSpeed;
    mob.velocity.y -= config.gravity * dt;
    if (mob.velocity.y < -config.maxFallSpeed) mob.velocity.y = -config.maxFallSpeed;

    mob.onGround = false;
    moveAxis(world, mob, 0, mob.velocity.x * dt);
    moveAxis(world, mob, 2, mob.velocity.z * dt);
    bool collidedY = moveAxis(world, mob, 1, mob.velocity.y * dt);
    if (collidedY) {
        if (mob.velocity.y < 0.0f) mob.onGround = true;
        mob.velocity.y = 0.0f;
    }
}

Vec3 updateMobBrain(const World& world, Mob& mob, const Vec3& targetPos, const MobConfig& config) {
    float dist = horizontalDistance(mob.position, targetPos);
    bool wantChase = mob.state == MobState::Chase ? dist <= config.loseRadius
                                                  : dist <= config.chaseRadius;

    if (wantChase) {
        Vec3i start = mobFeetCell(mob);
        Vec3i goal = feetCellOf(targetPos);
        std::vector<Vec3i> path = findPath(world, start, goal, config.path);
        if (path.size() >= 2) {
            mob.state = MobState::Chase;
            Vec3 center{static_cast<float>(path[1].x) + 0.5f, mob.position.y,
                        static_cast<float>(path[1].z) + 0.5f};
            Vec3 toNext{center.x - mob.position.x, 0.0f, center.z - mob.position.z};
            return normalize(toNext);
        }
        if (path.size() == 1) {
            mob.state = MobState::Chase;
            return {0.0f, 0.0f, 0.0f};
        }
    }

    mob.state = MobState::Wander;
    return wanderDirection(mob);
}

}
