#pragma once

#include "World.h"
#include "Math.h"

namespace vf {

struct Aabb {
    Vec3 min;
    Vec3 max;
};

struct Player {
    Vec3 position;
    Vec3 velocity;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float width = 0.6f;
    float height = 1.8f;
    float eyeHeight = 1.62f;
    bool onGround = false;
    bool flying = false;
};

struct MoveInput {
    float forward = 0.0f;
    float strafe = 0.0f;
    bool jump = false;
    bool up = false;
    bool down = false;
};

struct PhysicsConfig {
    float walkSpeed = 4.6f;
    float flySpeed = 9.0f;
    float gravity = 26.0f;
    float jumpSpeed = 8.4f;
    float maxFallSpeed = 55.0f;
};

Aabb playerBox(const Player& player);
Vec3 eyePosition(const Player& player);

bool boxOverlapsSolid(const World& world, const Aabb& box);

void stepPlayer(const World& world, Player& player, const MoveInput& input,
                const PhysicsConfig& config, float dt);

}
