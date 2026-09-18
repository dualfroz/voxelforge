#include "Explorer.h"

#include <algorithm>
#include <cmath>

namespace vf {

namespace {

constexpr int physicsSubsteps = 4;

Block blockAtPoint(const World& world, const Vec3& point) {
    return world.blockAt(static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y)),
                         static_cast<int>(std::floor(point.z)));
}

bool ledgeAhead(const World& world, const Player& player) {
    Vec3 forward{std::sin(player.yaw), 0.0f, -std::cos(player.yaw)};
    Vec3 probe = player.position + forward * (player.width * 0.5f + 0.4f);
    int gx = static_cast<int>(std::floor(probe.x));
    int gz = static_cast<int>(std::floor(probe.z));
    int gy = static_cast<int>(std::floor(player.position.y + 0.5f));
    return world.solidAt(gx, gy, gz) && !world.solidAt(gx, gy + 1, gz) &&
           !world.solidAt(gx, gy + 2, gz);
}

}

Explorer::Explorer() : survival(config) {
    waterPhysics.walkSpeed = 2.4f;
    waterPhysics.gravity = 7.0f;
    waterPhysics.jumpSpeed = 3.2f;
    waterPhysics.maxFallSpeed = 2.6f;
}

void Explorer::sampleWater(const World& world) {
    Block feet = blockAtPoint(world, player.position + Vec3{0.0f, 0.2f, 0.0f});
    Block eyes = blockAtPoint(world, eyePosition(player));
    underwater = eyes == Block::Water;
    inWater = underwater || feet == Block::Water;
}

void Explorer::move(const World& world, const ExplorerControls& controls, float dt) {
    MoveInput input;
    input.forward = controls.forward;
    input.strafe = controls.strafe;
    float step = dt / physicsSubsteps;
    for (int i = 0; i < physicsSubsteps; ++i) {
        sampleWater(world);
        if (player.flying) {
            input.up = controls.jump;
            input.down = controls.sink;
            stepPlayer(world, player, input, landPhysics, step);
            continue;
        }
        input.jump = controls.jump;
        if (!inWater) {
            stepPlayer(world, player, input, landPhysics, step);
            continue;
        }
        if (controls.jump && !underwater && ledgeAhead(world, player)) {
            player.onGround = true;
            stepPlayer(world, player, input, landPhysics, step);
            continue;
        }
        if (controls.jump) player.onGround = true;
        if (controls.sink) player.velocity.y = -waterPhysics.maxFallSpeed;
        stepPlayer(world, player, input, waterPhysics, step);
    }
    sampleWater(world);
    trackFall();
}

void Explorer::trackFall() {
    float y = player.position.y;
    if (player.flying || inWater) {
        fallPeak = y;
        wasGrounded = player.onGround;
        return;
    }
    if (player.onGround) {
        if (!wasGrounded) applyFallDamage(survival, config, fallPeak - y);
        fallPeak = y;
    } else {
        fallPeak = std::max(fallPeak, y);
    }
    wasGrounded = player.onGround;
}

void Explorer::tick(float dt) {
    tickHungerAndRegen(survival, config, dt);
    tickBreath(survival, config, dt, underwater && !player.flying);
    float dx = player.position.x - lastTickPosition.x;
    float dz = player.position.z - lastTickPosition.z;
    float moved = std::sqrt(dx * dx + dz * dz);
    if (!player.flying && moved < 2.0f)
        applyExertion(survival, config, inWater ? ExertionAction::Swim : ExertionAction::Walk, moved);
    lastTickPosition = player.position;
}

void Explorer::hurt(DamageSource source, float amount) {
    if (player.flying || amount <= 0.0f) return;
    applyDamage(survival, source, amount);
}

void Explorer::respawn(const Vec3& position) {
    survival = SurvivalState(config);
    player.position = position;
    player.velocity = {};
    fallPeak = position.y;
    lastTickPosition = position;
}

ItemId Explorer::eatBest() {
    ItemId best = ItemId::None;
    for (const auto& entry : pantry) {
        if (entry.second <= 0 || !isFood(entry.first)) continue;
        if (best == ItemId::None || foodValue(entry.first).hunger > foodValue(best).hunger)
            best = entry.first;
    }
    if (best == ItemId::None) return best;
    eat(survival, config, best);
    --pantry[best];
    return best;
}

int Explorer::foodCount() const {
    int count = 0;
    for (const auto& entry : pantry)
        if (isFood(entry.first)) count += entry.second;
    return count;
}

}
