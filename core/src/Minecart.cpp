#include "vf/Minecart.h"

#include <algorithm>

namespace vf {

namespace {

int slotsFor(CartKind kind) {
    switch (kind) {
        case CartKind::Chest:  return chestCartSlots;
        case CartKind::Hopper: return hopperCartSlots;
        case CartKind::Empty:  return 0;
    }
    return 0;
}

void applyRailForces(World& world, Minecart& cart, const CartConfig& config,
                     const std::function<bool(const Vec3i&)>& poweredRailActive,
                     RailShape shape) {
    Block block = world.blockAt(cart.railPos.x, cart.railPos.y, cart.railPos.z);

    if (block == Block::PoweredRail) {
        if (poweredRailActive(cart.railPos)) {
            cart.speed += config.poweredAccel;
            if (cart.speed < config.poweredLaunch) cart.speed = config.poweredLaunch;
        } else {
            cart.speed *= config.brakeFactor;
        }
    } else {
        cart.speed *= (1.0f - config.friction);
    }

    if (isAscending(shape)) {
        RailDir up = ascendUpDir(shape);
        if (cart.moveDir == up) cart.speed -= config.slopeAccel;
        else if (cart.moveDir == opposite(up)) cart.speed += config.slopeAccel;
    }

    if (cart.speed < 0.0f) {
        cart.moveDir = opposite(cart.moveDir);
        cart.speed = -cart.speed;
    }

    cart.speed = std::min(cart.speed, config.maxSpeed);
    if (cart.speed < config.stopThreshold) cart.speed = 0.0f;
}

void serviceHopperCart(Minecart& cart, const CartConfig& config, ContainerStore& store) {
    if (cart.kind != CartKind::Hopper) return;
    if (cart.hopperCooldown > 0) {
        --cart.hopperCooldown;
        return;
    }
    Container* srcAbove = store.at(cart.railPos + Vec3i{0, 1, 0});
    if (srcAbove && srcAbove->transferTo(cart.inventory, 1) > 0)
        cart.hopperCooldown = config.hopperCooldownTicks;
}

Vec3 velocityVector(const Minecart& cart) {
    Vec3i step = railDelta(cart.moveDir);
    return Vec3{static_cast<float>(step.x) * cart.speed,
                static_cast<float>(step.y) * cart.speed,
                static_cast<float>(step.z) * cart.speed};
}

void assignVelocity(Minecart& cart, const Vec3& vel) {
    float ax = std::fabs(vel.x);
    float az = std::fabs(vel.z);
    if (ax < 1e-6f && az < 1e-6f) {
        cart.speed = 0.0f;
        return;
    }
    if (ax >= az) {
        cart.moveDir = vel.x >= 0.0f ? RailDir::East : RailDir::West;
        cart.speed = ax;
    } else {
        cart.moveDir = vel.z >= 0.0f ? RailDir::South : RailDir::North;
        cart.speed = az;
    }
}

}

Minecart makeMinecart(const Vec3i& railPos, RailDir moveDir, CartKind kind) {
    Minecart cart;
    cart.railPos = railPos;
    cart.moveDir = moveDir;
    cart.kind = kind;
    cart.inventory = Container{slotsFor(kind)};
    return cart;
}

Vec3 cartWorldPos(const Minecart& cart) {
    Vec3i step = railDelta(cart.moveDir);
    return Vec3{
        static_cast<float>(cart.railPos.x) + 0.5f + static_cast<float>(step.x) * cart.offset,
        static_cast<float>(cart.railPos.y) + 0.5f + static_cast<float>(step.y) * cart.offset,
        static_cast<float>(cart.railPos.z) + 0.5f + static_cast<float>(step.z) * cart.offset,
    };
}

void tickMinecart(World& world, Minecart& cart, const CartConfig& config,
                  const std::function<bool(const Vec3i&)>& poweredRailActive,
                  ContainerStore& store) {
    if (cart.derailed) return;
    if (!isRailBlock(world.blockAt(cart.railPos.x, cart.railPos.y, cart.railPos.z))) {
        cart.derailed = true;
        cart.speed = 0.0f;
        return;
    }

    bool switched = poweredRailActive(cart.railPos);
    RailShape shape = inferRailShape(world, cart.railPos, switched);
    applyRailForces(world, cart, config, poweredRailActive, shape);

    cart.offset += cart.speed;
    while (cart.offset >= 1.0f && cart.speed > 0.0f) {
        cart.offset -= 1.0f;
        bool junction = poweredRailActive(cart.railPos);
        RailStep next = advanceOnRail(world, cart.railPos, cart.moveDir, junction);
        if (!next.valid) {
            cart.derailed = true;
            cart.speed = 0.0f;
            cart.offset = 0.0f;
            return;
        }
        cart.railPos = next.nextPos;
        cart.moveDir = next.exitDir;
    }

    serviceHopperCart(cart, config, store);
}

void stepMinecarts(World& world, std::vector<Minecart>& carts, const CartConfig& config,
                   const std::function<bool(const Vec3i&)>& poweredRailActive,
                   ContainerStore& store) {
    std::vector<Vec3i> previous(carts.size());
    for (std::size_t i = 0; i < carts.size(); ++i) previous[i] = carts[i].railPos;

    for (Minecart& cart : carts)
        tickMinecart(world, cart, config, poweredRailActive, store);

    for (std::size_t i = 0; i < carts.size(); ++i) {
        for (std::size_t j = i + 1; j < carts.size(); ++j) {
            Minecart& a = carts[i];
            Minecart& b = carts[j];
            if (a.derailed || b.derailed) continue;
            if (previous[i] == previous[j]) continue;

            bool sameCell = a.railPos == b.railPos;
            bool passedThrough = a.railPos == previous[j] && b.railPos == previous[i];
            if (!sameCell && !passedThrough) continue;

            Vec3 va = velocityVector(a);
            Vec3 vb = velocityVector(b);
            assignVelocity(a, vb);
            assignVelocity(b, va);

            a.railPos = previous[i];
            b.railPos = previous[j];
            a.offset = 0.0f;
            b.offset = 0.0f;
        }
    }
}

}
