#pragma once

#include <cstdint>

namespace vf {

enum class Block : uint8_t {
    Air = 0,
    Grass = 1,
    Dirt = 2,
    Stone = 3,
    Sand = 4,
    Wood = 5,
    Leaves = 6,
    Water = 7,
    Snow = 8,
    CoalOre = 9,
    IronOre = 10,
    GoldOre = 11,
    Gravel = 12,
    Glowstone = 13,
    // --- extended palette (wave 1, additive; values above are frozen) ---
    Cobblestone = 14,
    Planks = 15,
    Sandstone = 16,
    Ice = 17,
    Bedrock = 18,
    Obsidian = 19,
    Lava = 20,
    DiamondOre = 21,
    RedstoneOre = 22,
    EmeraldOre = 23,
    LapisOre = 24,
    CraftingTable = 25,
    Furnace = 26,
    FurnaceLit = 27,
    Chest = 28,
    Torch = 29,
    RedstoneWire = 30,
    RedstoneTorch = 31,
    RedstoneLamp = 32,
    RedstoneLampLit = 33,
    Lever = 34,
    Button = 35,
    RedstoneBlock = 36,
    Repeater = 37,
    Comparator = 38,
    Piston = 39,
    PistonSticky = 40,
    PistonHead = 41,
    Tnt = 42,
    Ladder = 43,
    // --- extended palette (wave 2, additive; values above are frozen) ---
    // Automation & logistics (agent A).
    Rail = 44,
    PoweredRail = 45,
    DetectorRail = 46,
    ActivatorRail = 47,
    Hopper = 48,
    Dropper = 49,
    Dispenser = 50,
    Observer = 51,
    TargetBlock = 52,
    SlimeBlock = 53,
    NoteBlock = 54,
    // Environment, farming & climate (agent B).
    Farmland = 55,
    Wheat = 56,
    Carrot = 57,
    Potato = 58,
    Pumpkin = 59,
    Melon = 60,
    PumpkinStem = 61,
    MelonStem = 62,
    SugarCane = 63,
    Cactus = 64,
    Sapling = 65,
    Vine = 66,
    SnowLayer = 67,
    PackedIce = 68,
    Count
};

enum class Face : uint8_t {
    PosX = 0,
    NegX = 1,
    PosY = 2,
    NegY = 3,
    PosZ = 4,
    NegZ = 5,
};

inline bool isAir(Block b) { return b == Block::Air; }

// Non-solid blocks are walked through: fluids and thin redstone/utility parts.
inline bool isSolid(Block b) {
    switch (b) {
        case Block::Air:
        case Block::Water:
        case Block::Lava:
        case Block::Torch:
        case Block::RedstoneTorch:
        case Block::RedstoneWire:
        case Block::Lever:
        case Block::Button:
        case Block::Repeater:
        case Block::Comparator:
        case Block::Ladder:
        case Block::Rail:
        case Block::PoweredRail:
        case Block::DetectorRail:
        case Block::ActivatorRail:
        case Block::Wheat:
        case Block::Carrot:
        case Block::Potato:
        case Block::PumpkinStem:
        case Block::MelonStem:
        case Block::SugarCane:
        case Block::Sapling:
        case Block::Vine:
        case Block::SnowLayer:
            return false;
        default:
            return true;
    }
}

// Opaque blocks fully hide the neighbouring face. Leaves, fluids, ice and the
// thin redstone/utility blocks do not.
inline bool isOpaque(Block b) {
    switch (b) {
        case Block::Air:
        case Block::Leaves:
        case Block::Water:
        case Block::Ice:
        case Block::Torch:
        case Block::RedstoneWire:
        case Block::RedstoneTorch:
        case Block::Lever:
        case Block::Button:
        case Block::Repeater:
        case Block::Comparator:
        case Block::Ladder:
        case Block::Rail:
        case Block::PoweredRail:
        case Block::DetectorRail:
        case Block::ActivatorRail:
        case Block::Hopper:
        case Block::Cactus:
        case Block::Wheat:
        case Block::Carrot:
        case Block::Potato:
        case Block::PumpkinStem:
        case Block::MelonStem:
        case Block::SugarCane:
        case Block::Sapling:
        case Block::Vine:
        case Block::SnowLayer:
            return false;
        default:
            return true;
    }
}

// Whether a face of `self` should be emitted given the neighbour `other`.
// A face is visible when the neighbour does not opaquely cover it, and the
// two blocks are not the same translucent type (so water/leaves/ice do not
// draw internal seams against their own kind).
inline bool faceVisible(Block self, Block other) {
    if (isAir(self)) return false;
    if (isOpaque(other)) return false;
    if (self == other) return false;
    return true;
}

// Texture-array layer for a given block face. Sided blocks vary by face.
inline int faceTile(Block block, Face face) {
    switch (block) {
        case Block::Grass:
            if (face == Face::PosY) return 0;  // grass top
            if (face == Face::NegY) return 2;  // dirt bottom
            return 1;                          // grass side
        case Block::Dirt: return 2;
        case Block::Stone: return 3;
        case Block::Sand: return 4;
        case Block::Wood:
            if (face == Face::PosY || face == Face::NegY) return 6;  // rings
            return 5;                                                // bark
        case Block::Leaves: return 7;
        case Block::Water: return 8;
        case Block::Snow: return 9;
        case Block::CoalOre: return 10;
        case Block::IronOre: return 11;
        case Block::GoldOre: return 12;
        case Block::Gravel: return 13;
        case Block::Glowstone: return 14;
        case Block::Cobblestone: return 15;
        case Block::Planks: return 16;
        case Block::Sandstone:
            if (face == Face::PosY || face == Face::NegY) return 17;  // top/bottom
            return 18;                                                // side
        case Block::Ice: return 19;
        case Block::Bedrock: return 20;
        case Block::Obsidian: return 21;
        case Block::Lava: return 22;
        case Block::DiamondOre: return 23;
        case Block::RedstoneOre: return 24;
        case Block::EmeraldOre: return 25;
        case Block::LapisOre: return 26;
        case Block::CraftingTable:
            if (face == Face::PosY) return 27;  // grid top
            if (face == Face::NegY) return 16;  // planks bottom
            return 28;                          // tools side
        case Block::Furnace:
            if (face == Face::PosZ) return 29;  // cold front
            if (face == Face::PosY || face == Face::NegY) return 30;
            return 30;                          // side
        case Block::FurnaceLit:
            if (face == Face::PosZ) return 31;  // burning front
            return 30;
        case Block::Chest: return 32;
        case Block::Torch: return 33;
        case Block::RedstoneWire: return 34;
        case Block::RedstoneTorch: return 35;
        case Block::RedstoneLamp: return 36;
        case Block::RedstoneLampLit: return 37;
        case Block::Lever: return 38;
        case Block::Button: return 39;
        case Block::RedstoneBlock: return 40;
        case Block::Repeater: return 41;
        case Block::Comparator: return 42;
        case Block::Piston:
            if (face == Face::PosZ) return 43;  // face plate
            if (face == Face::NegZ) return 45;  // back
            return 44;                          // side
        case Block::PistonSticky:
            if (face == Face::PosZ) return 46;  // sticky face
            if (face == Face::NegZ) return 45;
            return 44;
        case Block::PistonHead: return 47;
        case Block::Tnt:
            if (face == Face::PosY || face == Face::NegY) return 48;  // caps
            return 49;                                                // fuse band
        case Block::Ladder: return 50;
        case Block::Rail: return 51;
        case Block::PoweredRail: return 52;
        case Block::DetectorRail: return 53;
        case Block::ActivatorRail: return 54;
        case Block::Hopper: return 55;
        case Block::Dropper: return 56;
        case Block::Dispenser: return 57;
        case Block::Observer: return 58;
        case Block::TargetBlock: return 59;
        case Block::SlimeBlock: return 60;
        case Block::NoteBlock: return 61;
        case Block::Farmland:
            if (face == Face::PosY) return 62;  // tilled top
            return 2;                           // dirt sides/bottom
        case Block::Wheat: return 63;
        case Block::Carrot: return 64;
        case Block::Potato: return 65;
        case Block::Pumpkin:
            if (face == Face::PosY || face == Face::NegY) return 66;
            return 67;
        case Block::Melon:
            if (face == Face::PosY || face == Face::NegY) return 68;
            return 69;
        case Block::PumpkinStem: return 70;
        case Block::MelonStem: return 71;
        case Block::SugarCane: return 72;
        case Block::Cactus:
            if (face == Face::PosY) return 73;
            if (face == Face::NegY) return 74;
            return 75;
        case Block::Sapling: return 76;
        case Block::Vine: return 77;
        case Block::SnowLayer: return 78;
        case Block::PackedIce: return 79;
        default: return 3;
    }
}

constexpr int tileCount = 80;

// Block light emitted by a block itself, 0..15.
inline int blockLightEmission(Block b) {
    switch (b) {
        case Block::Glowstone:       return 15;
        case Block::Lava:            return 15;
        case Block::RedstoneLampLit: return 15;
        case Block::Torch:           return 14;
        case Block::FurnaceLit:      return 13;
        case Block::RedstoneTorch:   return 7;
        case Block::RedstoneOre:     return 0;  // lit only on touch, handled by gameplay
        default:                     return 0;
    }
}

// A block that ambient-occludes its neighbours and blocks light propagation.
inline bool occludesLight(Block b) { return isOpaque(b); }

// Meshed in the transparent pass (depth write off, back-to-front): fluids + ice.
inline bool isTransparent(Block b) {
    return b == Block::Water || b == Block::Ice;
}

} // namespace vf
