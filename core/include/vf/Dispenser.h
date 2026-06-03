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

// Fires on a rising redstone edge. A dispenser places block items into the air
// cell it faces, or ejects non-block items (into a container in front when one
// exists, otherwise into the world). A dropper never places: it always ejects.
// The slot to draw from is chosen with the deterministic per-dispenser rng.
DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, int powerLevel);
DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, int powerLevel);

DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, const RedstoneCircuit& circuit);
DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, const RedstoneCircuit& circuit);

// Deterministic non-empty slot pick used by dispensers/droppers. Returns -1 when
// the inventory holds nothing. Exposed for testing the selection stream.
int chooseNonEmptySlot(const Container& inventory, std::uint32_t& rng);

} // namespace vf
