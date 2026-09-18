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
    BrewingStand = 69,
    EnchantingTable = 70,
    Cauldron = 71,
    AnvilBlock = 72,
    Bookshelf = 73,
    Cobweb = 74,
    Netherrack = 75,
    SoulSand = 76,
    SoulSoil = 77,
    NetherQuartzOre = 78,
    NetherBrick = 79,
    NetherPortal = 80,
    Magma = 81,
    Basalt = 82,
    Blackstone = 83,
    EndStone = 84,
    EndPortal = 85,
    EndPortalFrame = 86,
    Purpur = 87,
    ChorusPlant = 88,
    ChorusFlower = 89,
    Glass = 90,
    Bricks = 91,
    StoneBricks = 92,
    MossyCobblestone = 93,
    Door = 94,
    Slab = 95,
    Stairs = 96,
    Fence = 97,
    Bed = 98,
    Bell = 99,
    Lantern = 100,
    Composter = 101,
    Barrel = 102,
    SmithingTable = 103,
    Loom = 104,
    CartographyTable = 105,
    Grindstone = 106,
    Stonecutter = 107,
    Lectern = 108,
    HayBale = 109,
    Clay = 110,
    Terracotta = 111,
    Wool = 112,
    Mycelium = 113,
    Andesite = 114,
    Diorite = 115,
    Granite = 116,
    Deepslate = 117,
    CopperOre = 118,
    Tuff = 119,
    Dripstone = 120,
    AmethystBlock = 121,
    Sculk = 122,
    CryingObsidian = 123,
    Prismarine = 124,
    SeaLantern = 125,
    Kelp = 126,
    Coral = 127,
    Mud = 128,
    Moss = 129,
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
        case Block::BrewingStand:
        case Block::Cobweb:
        case Block::NetherPortal:
        case Block::EndPortal:
        case Block::Door:
        case Block::Lantern:
        case Block::Bell:
        case Block::Kelp:
        case Block::ChorusFlower:
            return false;
        default:
            return true;
    }
}

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
        case Block::Glass:
        case Block::NetherPortal:
        case Block::EndPortal:
        case Block::Cobweb:
        case Block::BrewingStand:
        case Block::EnchantingTable:
        case Block::Cauldron:
        case Block::AnvilBlock:
        case Block::Door:
        case Block::Slab:
        case Block::Stairs:
        case Block::Fence:
        case Block::Bed:
        case Block::Bell:
        case Block::Lantern:
        case Block::Kelp:
        case Block::ChorusPlant:
        case Block::ChorusFlower:
        case Block::Composter:
        case Block::Grindstone:
        case Block::Lectern:
        case Block::Stonecutter:
            return false;
        default:
            return true;
    }
}

inline bool faceVisible(Block self, Block other) {
    if (isAir(self)) return false;
    if (isOpaque(other)) return false;
    if (self == other) return false;
    return true;
}

inline int faceTile(Block block, Face face) {
    switch (block) {
        case Block::Grass:
            if (face == Face::PosY) return 0;
            if (face == Face::NegY) return 2;
            return 1;
        case Block::Dirt: return 2;
        case Block::Stone: return 3;
        case Block::Sand: return 4;
        case Block::Wood:
            if (face == Face::PosY || face == Face::NegY) return 6;
            return 5;
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
            if (face == Face::PosY || face == Face::NegY) return 17;
            return 18;
        case Block::Ice: return 19;
        case Block::Bedrock: return 20;
        case Block::Obsidian: return 21;
        case Block::Lava: return 22;
        case Block::DiamondOre: return 23;
        case Block::RedstoneOre: return 24;
        case Block::EmeraldOre: return 25;
        case Block::LapisOre: return 26;
        case Block::CraftingTable:
            if (face == Face::PosY) return 27;
            if (face == Face::NegY) return 16;
            return 28;
        case Block::Furnace:
            if (face == Face::PosZ) return 29;
            if (face == Face::PosY || face == Face::NegY) return 30;
            return 30;
        case Block::FurnaceLit:
            if (face == Face::PosZ) return 31;
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
            if (face == Face::PosZ) return 43;
            if (face == Face::NegZ) return 45;
            return 44;
        case Block::PistonSticky:
            if (face == Face::PosZ) return 46;
            if (face == Face::NegZ) return 45;
            return 44;
        case Block::PistonHead: return 47;
        case Block::Tnt:
            if (face == Face::PosY || face == Face::NegY) return 48;
            return 49;
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
            if (face == Face::PosY) return 62;
            return 2;
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
        case Block::BrewingStand: return 80;
        case Block::EnchantingTable:
            if (face == Face::PosY) return 81;
            if (face == Face::NegY) return 21;
            return 82;
        case Block::Cauldron:
            if (face == Face::PosY) return 83;
            return 84;
        case Block::AnvilBlock:
            if (face == Face::PosY) return 85;
            return 86;
        case Block::Bookshelf:
            if (face == Face::PosY || face == Face::NegY) return 16;
            return 87;
        case Block::Cobweb: return 88;
        case Block::Netherrack: return 89;
        case Block::SoulSand: return 90;
        case Block::SoulSoil: return 91;
        case Block::NetherQuartzOre: return 92;
        case Block::NetherBrick: return 93;
        case Block::NetherPortal: return 94;
        case Block::Magma: return 95;
        case Block::Basalt:
            if (face == Face::PosY || face == Face::NegY) return 96;
            return 97;
        case Block::Blackstone: return 98;
        case Block::EndStone: return 99;
        case Block::EndPortal: return 100;
        case Block::EndPortalFrame:
            if (face == Face::PosY) return 101;
            return 102;
        case Block::Purpur: return 103;
        case Block::ChorusPlant: return 104;
        case Block::ChorusFlower: return 105;
        case Block::Glass: return 106;
        case Block::Bricks: return 107;
        case Block::StoneBricks: return 108;
        case Block::MossyCobblestone: return 109;
        case Block::Door: return 110;
        case Block::Slab:
            if (face == Face::PosY || face == Face::NegY) return 111;
            return 112;
        case Block::Stairs: return 112;
        case Block::Fence: return 113;
        case Block::Bed:
            if (face == Face::PosY) return 114;
            return 115;
        case Block::Bell: return 116;
        case Block::Lantern: return 117;
        case Block::Composter:
            if (face == Face::PosY) return 118;
            return 119;
        case Block::Barrel:
            if (face == Face::PosY || face == Face::NegY) return 120;
            return 121;
        case Block::SmithingTable:
            if (face == Face::PosY) return 122;
            return 123;
        case Block::Loom:
            if (face == Face::PosY) return 124;
            return 125;
        case Block::CartographyTable:
            if (face == Face::PosY) return 126;
            return 127;
        case Block::Grindstone: return 128;
        case Block::Stonecutter:
            if (face == Face::PosY) return 129;
            return 130;
        case Block::Lectern:
            if (face == Face::PosY) return 131;
            return 132;
        case Block::HayBale:
            if (face == Face::PosY || face == Face::NegY) return 133;
            return 134;
        case Block::Clay: return 135;
        case Block::Terracotta: return 136;
        case Block::Wool: return 137;
        case Block::Mycelium:
            if (face == Face::PosY) return 138;
            if (face == Face::NegY) return 2;
            return 139;
        case Block::Andesite: return 140;
        case Block::Diorite: return 141;
        case Block::Granite: return 142;
        case Block::Deepslate:
            if (face == Face::PosY || face == Face::NegY) return 143;
            return 144;
        case Block::CopperOre: return 145;
        case Block::Tuff: return 146;
        case Block::Dripstone: return 147;
        case Block::AmethystBlock: return 148;
        case Block::Sculk: return 149;
        case Block::CryingObsidian: return 150;
        case Block::Prismarine: return 151;
        case Block::SeaLantern: return 152;
        case Block::Kelp: return 153;
        case Block::Coral: return 154;
        case Block::Mud: return 155;
        case Block::Moss: return 156;
        default: return 3;
    }
}

constexpr int tileCount = 157;

inline int blockLightEmission(Block b) {
    switch (b) {
        case Block::Glowstone:       return 15;
        case Block::Lava:            return 15;
        case Block::RedstoneLampLit: return 15;
        case Block::Torch:           return 14;
        case Block::FurnaceLit:      return 13;
        case Block::RedstoneTorch:   return 7;
        case Block::RedstoneOre:     return 0;
        case Block::EndPortal:       return 15;
        case Block::SeaLantern:      return 15;
        case Block::Lantern:         return 15;
        case Block::NetherPortal:    return 11;
        case Block::CryingObsidian:  return 10;
        case Block::EnchantingTable: return 7;
        case Block::Magma:           return 3;
        default:                     return 0;
    }
}

inline bool occludesLight(Block b) { return isOpaque(b); }

inline bool isTransparent(Block b) {
    return b == Block::Water || b == Block::Ice || b == Block::Glass ||
           b == Block::NetherPortal || b == Block::EndPortal;
}

}
