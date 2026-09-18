#include "Hands.h"

#include <cmath>
#include <cstdio>

#include "Hud.h"
#include "vf/Command.h"

namespace vf {

namespace {

constexpr float reach = 6.0f;
constexpr float punchDamage = 3.0f;
constexpr float punchCooldown = 0.35f;

}

Hands::Hands(World& world, TerrainView& terrain, WaterFlow& water, Wildlife& wildlife,
             Village& village, Explorer& explorer, Hotbar& hotbar)
    : world(world), terrain(terrain), water(water), wildlife(wildlife), village(village),
      explorer(explorer), hotbar(hotbar) {}

void Hands::aim() {
    const Player& body = explorer.body();
    Vec3 eye = eyePosition(body);
    Vec3 dir = directionFromYawPitch(body.yaw, body.pitch);
    block = raycastVoxels(world, eye, dir, reach);
    float limit = block.hit ? block.distance : reach;
    float creatureDistance = 0.0f, villagerDistance = 0.0f;
    creature = wildlife.pick(eye, dir, limit, creatureDistance);
    villager = village.pick(eye, dir, limit, villagerDistance);
    if (creature >= 0 && villager >= 0) {
        if (creatureDistance <= villagerDistance) villager = -1;
        else creature = -1;
    }
}

void Hands::press() {
    held = true;
    punchTimer = 0.0f;
}

void Hands::release() {
    held = false;
    miningProgress = 0.0f;
}

std::string Hands::describeTarget() const {
    char text[128];
    if (creature >= 0) {
        const Creature& mob = wildlife.creatures()[static_cast<std::size_t>(creature)].creature;
        std::snprintf(text, sizeof(text), "%s  %.0f / %.0f hp", speciesDisplayName(mob.species),
                      mob.health, speciesStats(mob.species).maxHealth);
        return text;
    }
    if (villager >= 0) {
        const Villager& resident = village.villagers()[static_cast<std::size_t>(villager)];
        std::snprintf(text, sizeof(text), "villager #%d  %s lv%d  %s", resident.id,
                      villagerProfessionName(resident.profession), resident.level,
                      villagerActivityName(resident.activity));
        return text;
    }
    if (!block.hit) return "";
    Block aimed = world.blockAt(block.block.x, block.block.y, block.block.z);
    return isAir(aimed) ? "" : blockDisplayName(aimed);
}

float Hands::miningFraction() const {
    if (!held || !block.hit || creature >= 0) return 0.0f;
    float seconds = miningSeconds(world.blockAt(block.block.x, block.block.y, block.block.z), ItemId::None);
    if (!std::isfinite(seconds) || seconds <= 0.0f) return 0.0f;
    return miningProgress / seconds;
}

std::string Hands::punch() {
    Species species = wildlife.creatures()[static_cast<std::size_t>(creature)].creature.species;
    std::string report;
    for (const ItemSlot& drop : wildlife.strike(creature, explorer.body().position, punchDamage)) {
        if (drop.empty()) continue;
        explorer.stash(drop.item, drop.count);
        report = std::string(speciesDisplayName(species)) + " dropped " + std::to_string(drop.count) +
                 " " + itemDisplayName(drop.item);
    }
    return report;
}

std::string Hands::work(float dt) {
    if (!held) return "";
    punchTimer -= dt;
    if (creature >= 0) {
        miningProgress = 0.0f;
        if (punchTimer > 0.0f) return "";
        punchTimer = punchCooldown;
        return punch();
    }
    if (!block.hit) {
        miningProgress = 0.0f;
        return "";
    }
    const Vec3i& cell = block.block;
    if (cell != miningCell) {
        miningCell = cell;
        miningProgress = 0.0f;
    }
    Block mined = world.blockAt(cell.x, cell.y, cell.z);
    float seconds = miningSeconds(mined, ItemId::None);
    if (!std::isfinite(seconds)) return "";
    if (explorer.body().flying) seconds = 0.0f;
    miningProgress += dt;
    if (miningProgress < seconds) return "";
    terrain.editBlock(cell.x, cell.y, cell.z, Block::Air);
    hotbar.add(mined, 1);
    pushHotbar(hotbar);
    miningProgress = 0.0f;
    return "";
}

void Hands::place() {
    if (!block.hit || creature >= 0 || villager >= 0) return;
    if (hotbar.slot(hotbar.selectedIndex()).empty()) return;
    Vec3i cell = block.block + block.normal;
    Aabb box = playerBox(explorer.body());
    bool overlaps = box.min.x < cell.x + 1.0f && box.max.x > cell.x && box.min.y < cell.y + 1.0f &&
                    box.max.y > cell.y && box.min.z < cell.z + 1.0f && box.max.z > cell.z;
    Block chosen = hotbar.selectedBlock();
    if (overlaps && isSolid(chosen)) return;
    if (chosen == Block::Water) water.pour(cell);
    else terrain.editBlock(cell.x, cell.y, cell.z, chosen);
    hotbar.consumeSelected();
    pushHotbar(hotbar);
}

}
