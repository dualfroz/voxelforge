#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "Codec.h"
#include "Item.h"
#include "Math.h"
#include "Nbt.h"
#include "Survival.h"

namespace vf {

// Everything that persists for one player between sessions: transform, the
// survival vitals, experience, the selected hotbar slot and the full item
// inventory (block items, materials, food and tools with durability).
struct PlayerData {
    Vec3 position;
    float yaw = 0.0f;
    float pitch = 0.0f;
    SurvivalState survival;
    int xpLevel = 0;
    float xpProgress = 0.0f;
    int selectedSlot = 0;
    std::vector<ItemSlot> inventory;
};

NbtTag playerToNbt(const PlayerData& player);
bool playerFromNbt(const NbtTag& root, PlayerData& out);

// NBT-backed byte serialization, round-tripping through the existing tag tree.
ByteBuffer savePlayer(const PlayerData& player);
bool loadPlayer(const uint8_t* data, std::size_t size, PlayerData& out);

} // namespace vf
