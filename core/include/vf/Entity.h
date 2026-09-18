#pragma once

#include <cstdint>
#include "World.h"
#include "Math.h"
#include "Physics.h"
#include "Pathfind.h"

namespace vf {

enum class MobState : uint8_t {
    Wander = 0,
    Chase = 1,
};

struct Mob {
    Vec3 position;
    Vec3 velocity;
    float width = 0.6f;
    float height = 1.8f;
    bool onGround = false;
    MobState state = MobState::Wander;
    uint32_t rng = 0x2545f491u;
    Vec3 wanderDir;
};

struct MobConfig {
    float moveSpeed = 3.2f;
    float gravity = 26.0f;
    float maxFallSpeed = 55.0f;
    float chaseRadius = 12.0f;
    float loseRadius = 16.0f;
    PathConfig path;
};

Aabb mobBox(const Mob& mob);
Vec3i mobFeetCell(const Mob& mob);

void stepMob(const World& world, Mob& mob, const Vec3& wishDir, const MobConfig& config, float dt);

Vec3 updateMobBrain(const World& world, Mob& mob, const Vec3& targetPos, const MobConfig& config);

}
