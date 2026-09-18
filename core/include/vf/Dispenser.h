#pragma once

#include <cstdint>
#include "Container.h"
#include "World.h"
#include "Circuit.h"
#include "Block.h"
#include "Math.h"

namespace vf {

constexpr int dispenserSlots = 9;

enum class DispenseAction : uint8_t {
    None,
    PlacedBlock,
    InsertedToContainer,
    EjectedToWorld,
};

struct DispenseResult {
    DispenseAction action = DispenseAction::None;
    ItemSlot item;
    Vec3i target;
};

struct DispenserState {
    Face facing = Face::PosX;
    bool lastPowered = false;
    std::uint32_t rng = 0x9e3779b9u;
};

DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, int powerLevel);
DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, int powerLevel);

DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, const RedstoneCircuit& circuit);
DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, const RedstoneCircuit& circuit);

int chooseNonEmptySlot(const Container& inventory, std::uint32_t& rng);

}
