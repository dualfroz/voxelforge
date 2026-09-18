#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "Rail.h"
#include "Container.h"
#include "World.h"
#include "Math.h"

namespace vf {

enum class CartKind : uint8_t {
    Empty,
    Chest,
    Hopper,
};

constexpr int chestCartSlots = 27;
constexpr int hopperCartSlots = 5;

struct CartConfig {
    float maxSpeed = 0.4f;
    float friction = 0.02f;
    float slopeAccel = 0.05f;
    float poweredAccel = 0.06f;
    float poweredLaunch = 0.06f;
    float brakeFactor = 0.5f;
    float stopThreshold = 0.005f;
    int hopperCooldownTicks = 4;
};

struct Minecart {
    Vec3i railPos;
    RailDir moveDir = RailDir::East;
    float speed = 0.0f;
    float offset = 0.0f;
    CartKind kind = CartKind::Empty;
    bool derailed = false;
    int hopperCooldown = 0;
    Container inventory;
};

Minecart makeMinecart(const Vec3i& railPos, RailDir moveDir, CartKind kind);

Vec3 cartWorldPos(const Minecart& cart);

void tickMinecart(World& world, Minecart& cart, const CartConfig& config,
                  const std::function<bool(const Vec3i&)>& poweredRailActive,
                  ContainerStore& store);

void stepMinecarts(World& world, std::vector<Minecart>& carts, const CartConfig& config,
                   const std::function<bool(const Vec3i&)>& poweredRailActive,
                   ContainerStore& store);

}
