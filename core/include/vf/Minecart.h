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

// World-space centre of the cart: the rail cell centre pushed along the heading
// by the sub-cell offset.
Vec3 cartWorldPos(const Minecart& cart);

// Advances the cart one simulation tick: applies powered-rail boost or braking,
// slope gravity and rolling friction, then walks the track by the resulting
// speed, switching heading through curves and derailing at track ends. Hopper
// carts also pull one item per cooldown from a container above their cell.
void tickMinecart(World& world, Minecart& cart, const CartConfig& config,
                  const std::function<bool(const Vec3i&)>& poweredRailActive,
                  ContainerStore& store);

// Ticks a batch of carts, then resolves collisions deterministically in index
// order: two carts that land on the same rail cell exchange momentum (equal
// mass, elastic 1D) and separate. A head-on pair bounces apart; a moving cart
// hitting a stalled one hands over its speed and stops.
void stepMinecarts(World& world, std::vector<Minecart>& carts, const CartConfig& config,
                   const std::function<bool(const Vec3i&)>& poweredRailActive,
                   ContainerStore& store);

} // namespace vf
