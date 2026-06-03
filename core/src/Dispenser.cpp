#include "vf/Dispenser.h"

namespace vf {

namespace {

std::uint32_t nextRandom(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

DispenseResult ejectInto(ContainerStore& store, const Vec3i& front, const ItemSlot& one) {
    Container* dest = store.at(front);
    if (dest) {
        ItemSlot copy = one;
        if (dest->insert(copy) > 0) return {DispenseAction::InsertedToContainer, one, front};
    }
    return {DispenseAction::EjectedToWorld, one, front};
}

bool risingEdge(DispenserState& state, int powerLevel) {
    bool powered = powerLevel > 0;
    bool rising = powered && !state.lastPowered;
    state.lastPowered = powered;
    return rising;
}

DispenseResult fire(World& world, const Vec3i& pos, DispenserState& state,
                    ContainerStore& store, bool placeBlocks) {
    Container& inventory = store.ensure(pos, dispenserSlots);
    int index = chooseNonEmptySlot(inventory, state.rng);
    if (index < 0) return {};

    ItemSlot one = inventory.extractSlot(index, 1);
    if (one.empty()) return {};

    Vec3i front = pos + faceDelta(state.facing);
    if (placeBlocks && isBlockItem(one.item)) {
        Block placed = blockForItem(one.item);
        if (placed != Block::Air && world.blockAt(front.x, front.y, front.z) == Block::Air) {
            world.setBlock(front.x, front.y, front.z, placed);
            return {DispenseAction::PlacedBlock, one, front};
        }
    }
    return ejectInto(store, front, one);
}

} // namespace

int chooseNonEmptySlot(const Container& inventory, std::uint32_t& rng) {
    int candidates = 0;
    for (int i = 0; i < inventory.size(); ++i)
        if (!inventory.slot(i).empty()) ++candidates;
    if (candidates == 0) return -1;

    int pick = static_cast<int>((nextRandom(rng) >> 16) % static_cast<std::uint32_t>(candidates));
    for (int i = 0; i < inventory.size(); ++i) {
        if (inventory.slot(i).empty()) continue;
        if (pick == 0) return i;
        --pick;
    }
    return -1;
}

DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, int powerLevel) {
    if (!risingEdge(state, powerLevel)) return {};
    return fire(world, pos, state, store, true);
}

DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, int powerLevel) {
    if (!risingEdge(state, powerLevel)) return {};
    return fire(world, pos, state, store, false);
}

DispenseResult tickDispenser(World& world, const Vec3i& pos, DispenserState& state,
                             ContainerStore& store, const RedstoneCircuit& circuit) {
    return tickDispenser(world, pos, state, store, circuit.powerAt(pos.x, pos.y, pos.z));
}

DispenseResult tickDropper(World& world, const Vec3i& pos, DispenserState& state,
                           ContainerStore& store, const RedstoneCircuit& circuit) {
    return tickDropper(world, pos, state, store, circuit.powerAt(pos.x, pos.y, pos.z));
}

} // namespace vf
