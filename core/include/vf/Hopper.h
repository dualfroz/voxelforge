#pragma once

#include "Container.h"
#include "World.h"
#include "Block.h"
#include "Math.h"

namespace vf {

constexpr int hopperSlots = 5;
constexpr int hopperCooldownTicks = 8;

struct HopperState {
    Face facing = Face::NegY;
    int cooldown = 0;
};

bool tickHopper(const World& world, const Vec3i& pos, HopperState& state, ContainerStore& store);

bool hopperCanTransfer(const Vec3i& pos, const HopperState& state, ContainerStore& store);

}
