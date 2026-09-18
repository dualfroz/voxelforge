#include "vf/Physics.h"

#include <cmath>

namespace vf {

namespace {

constexpr float epsilon = 1e-4f;

float& axis(Vec3& v, int i) { return (&v.x)[i]; }

Aabb boxAt(const Player& player, const Vec3& position) {
    float half = player.width * 0.5f;
    return Aabb{
        {position.x - half, position.y, position.z - half},
        {position.x + half, position.y + player.height, position.z + half},
    };
}

bool moveAxis(const World& world, Player& player, int a, float disp) {
    axis(player.position, a) += disp;
    Aabb box = boxAt(player, player.position);
    if (!boxOverlapsSolid(world, box)) return false;

    if (disp > 0.0f) {
        float snapped = std::floor(axis(box.max, a));
        axis(player.position, a) -= (axis(box.max, a) - snapped) + epsilon;
    } else if (disp < 0.0f) {
        float snapped = std::ceil(axis(box.min, a));
        axis(player.position, a) += (snapped - axis(box.min, a)) + epsilon;
    }
    return true;
}

}

Aabb playerBox(const Player& player) { return boxAt(player, player.position); }

Vec3 eyePosition(const Player& player) {
    return {player.position.x, player.position.y + player.eyeHeight, player.position.z};
}

bool boxOverlapsSolid(const World& world, const Aabb& box) {
    int x0 = static_cast<int>(std::floor(box.min.x));
    int y0 = static_cast<int>(std::floor(box.min.y));
    int z0 = static_cast<int>(std::floor(box.min.z));
    int x1 = static_cast<int>(std::ceil(box.max.x)) - 1;
    int y1 = static_cast<int>(std::ceil(box.max.y)) - 1;
    int z1 = static_cast<int>(std::ceil(box.max.z)) - 1;

    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (world.solidAt(x, y, z)) return true;
    return false;
}

void stepPlayer(const World& world, Player& player, const MoveInput& input,
                const PhysicsConfig& config, float dt) {
    Vec3 forward{std::sin(player.yaw), 0.0f, -std::cos(player.yaw)};
    Vec3 right{std::cos(player.yaw), 0.0f, std::sin(player.yaw)};

    Vec3 wish = forward * input.forward + right * input.strafe;
    float wishLen = length(wish);
    if (wishLen > 1.0f) wish = wish * (1.0f / wishLen);

    if (player.flying) {
        player.velocity.x = wish.x * config.flySpeed;
        player.velocity.z = wish.z * config.flySpeed;
        float vertical = (input.up ? 1.0f : 0.0f) - (input.down ? 1.0f : 0.0f);
        player.velocity.y = vertical * config.flySpeed;
    } else {
        player.velocity.x = wish.x * config.walkSpeed;
        player.velocity.z = wish.z * config.walkSpeed;
        player.velocity.y -= config.gravity * dt;
        if (player.velocity.y < -config.maxFallSpeed) player.velocity.y = -config.maxFallSpeed;
        if (input.jump && player.onGround) {
            player.velocity.y = config.jumpSpeed;
            player.onGround = false;
        }
    }

    player.onGround = false;
    moveAxis(world, player, 0, player.velocity.x * dt);
    moveAxis(world, player, 2, player.velocity.z * dt);
    bool collidedY = moveAxis(world, player, 1, player.velocity.y * dt);
    if (collidedY) {
        if (player.velocity.y < 0.0f) player.onGround = true;
        player.velocity.y = 0.0f;
    }
}

}
