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

// One deterministic hopper step at `pos`. Pulls a single item from the
// container directly above and pushes a single item into the container the
// hopper faces, then arms the cooldown when anything moved. The hopper owns a
// five slot inventory registered in `store` at its own cell. No-op while the
// cooldown is running or when the world block is no longer a hopper.
bool tickHopper(const World& world, const Vec3i& pos, HopperState& state, ContainerStore& store);

// True when the hopper has an item ready to push and a target that can take it,
// or a source above with an item it can pull. Used to gate cooldown re-arming.
bool hopperCanTransfer(const Vec3i& pos, const HopperState& state, ContainerStore& store);

} // namespace vf
