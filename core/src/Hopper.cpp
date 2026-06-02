#include "vf/Hopper.h"

namespace vf {

namespace {

Vec3i above(const Vec3i& pos) { return pos + Vec3i{0, 1, 0}; }

bool pushOne(Container& source, ContainerStore& store, const Vec3i& target) {
    Container* dest = store.at(target);
    if (!dest) return false;
    return source.transferTo(*dest, 1) > 0;
}

bool pullOne(ContainerStore& store, const Vec3i& source, Container& sink) {
    Container* src = store.at(source);
    if (!src) return false;
    return src->transferTo(sink, 1) > 0;
}

} // namespace

bool hopperCanTransfer(const Vec3i& pos, const HopperState& state, ContainerStore& store) {
    Container* self = store.at(pos);
    if (!self) return false;

    Vec3i target = pos + faceDelta(state.facing);
    if (!self->isEmpty()) {
        Container* dest = store.at(target);
        if (dest && !dest->isFull()) {
            Container probe = *self;
            Container destCopy = *dest;
            if (probe.transferTo(destCopy, 1) > 0) return true;
        }
    }

    Container* src = store.at(above(pos));
    if (src && !src->isEmpty() && !self->isFull()) {
        Container srcCopy = *src;
        Container selfCopy = *self;
        if (srcCopy.transferTo(selfCopy, 1) > 0) return true;
    }
    return false;
}

bool tickHopper(const World& world, const Vec3i& pos, HopperState& state, ContainerStore& store) {
    if (world.blockAt(pos.x, pos.y, pos.z) != Block::Hopper) return false;

    Container& self = store.ensure(pos, hopperSlots);
    if (state.cooldown > 0) {
        --state.cooldown;
        return false;
    }

    bool moved = false;
    Vec3i target = pos + faceDelta(state.facing);
    moved |= pushOne(self, store, target);
    moved |= pullOne(store, above(pos), self);

    if (moved) state.cooldown = hopperCooldownTicks;
    return moved;
}

} // namespace vf
