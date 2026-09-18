#include "vf/LootTables.h"

#include <algorithm>

namespace vf {

namespace {

constexpr uint32_t defaultLootSeed = 0x9e3779b9u;

uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int rollBelow(uint32_t& rng, int bound) {
    if (bound <= 1) return 0;
    return static_cast<int>(xorshift(rng) % static_cast<uint32_t>(bound));
}

int rollBetween(uint32_t& rng, int low, int high) {
    if (high <= low) return low;
    return low + rollBelow(rng, high - low + 1);
}

void appendPool(LootTable& table, const LootEntry& entry) {
    LootPool pool;
    pool.entries.push_back(entry);
    table.pools.push_back(pool);
}

BlockDropRule plainRule(Block block, ItemId drop, ToolType tool) {
    BlockDropRule rule;
    rule.block = block;
    rule.drop = drop;
    rule.harvestTool = tool;
    return rule;
}

BlockDropRule selfRule(Block block, ToolType tool) {
    return plainRule(block, itemForBlock(block), tool);
}

BlockDropRule gatedRule(Block block, ItemId drop, ToolType tool, ToolTier tier) {
    BlockDropRule rule = plainRule(block, drop, tool);
    rule.minimumTier = tier;
    rule.toolRequiredForDrop = true;
    return rule;
}

BlockDropRule selfGatedRule(Block block, ToolType tool, ToolTier tier) {
    return gatedRule(block, itemForBlock(block), tool, tier);
}

BlockDropRule oreRule(Block block, ItemId raw, int minCount, int maxCount, ToolTier tier) {
    BlockDropRule rule = gatedRule(block, raw, ToolType::Pickaxe, tier);
    rule.minCount = minCount;
    rule.maxCount = maxCount;
    rule.silkDrop = itemForBlock(block);
    rule.fortuneMultiplies = true;
    return rule;
}

BlockDropRule barrenRule(Block block, ToolType tool) { return plainRule(block, ItemId::None, tool); }

BlockDropRule barrenGatedRule(Block block, ToolType tool, ToolTier tier) {
    return gatedRule(block, ItemId::None, tool, tier);
}

BlockDropRule plankSalvage(Block block, int count = 1) {
    BlockDropRule rule = plainRule(block, ItemId::Planks, ToolType::Axe);
    rule.minCount = count;
    rule.maxCount = count;
    return rule;
}

BlockDropRule stoneSalvage(Block block) {
    return gatedRule(block, ItemId::Cobblestone, ToolType::Pickaxe, ToolTier::Wood);
}

std::vector<BlockDropRule> buildBlockRules() {
    std::vector<BlockDropRule> rules;
    rules.reserve(static_cast<std::size_t>(Block::Count));
    auto add = [&rules](const BlockDropRule& rule) { rules.push_back(rule); };

    add(barrenRule(Block::Air, ToolType::None));

    BlockDropRule grass = plainRule(Block::Grass, ItemId::Dirt, ToolType::Shovel);
    grass.silkDrop = ItemId::Grass;
    add(grass);

    add(selfRule(Block::Dirt, ToolType::Shovel));

    BlockDropRule stone = gatedRule(Block::Stone, ItemId::Cobblestone, ToolType::Pickaxe,
                                    ToolTier::Wood);
    stone.silkDrop = ItemId::Stone;
    stone.firedDrop = ItemId::Stone;
    add(stone);

    add(selfRule(Block::Sand, ToolType::Shovel));
    add(selfRule(Block::Wood, ToolType::Axe));

    BlockDropRule leaves = plainRule(Block::Leaves, ItemId::Wood, shearToolType);
    leaves.dropChance = 0.05f;
    leaves.chancePerFortune = 0.04f;
    leaves.silkDrop = ItemId::Leaves;
    leaves.shearHarvest = true;
    leaves.bonusDrop = ItemId::Stick;
    leaves.bonusChance = 0.02f;
    leaves.bonusMax = 2;
    add(leaves);

    add(barrenRule(Block::Water, ToolType::None));
    add(selfRule(Block::Snow, ToolType::Shovel));
    add(oreRule(Block::CoalOre, ItemId::CoalLump, 1, 1, ToolTier::Wood));

    BlockDropRule ironOre = oreRule(Block::IronOre, ItemId::IronOre, 1, 1, ToolTier::Stone);
    ironOre.firedDrop = ItemId::IronIngot;
    add(ironOre);

    BlockDropRule goldOre = oreRule(Block::GoldOre, ItemId::GoldOre, 1, 1, ToolTier::Iron);
    goldOre.firedDrop = ItemId::GoldIngot;
    add(goldOre);

    add(selfRule(Block::Gravel, ToolType::Shovel));

    BlockDropRule glowstone = oreRule(Block::Glowstone, ItemId::Glowstone, 2, 4, ToolTier::None);
    glowstone.toolRequiredForDrop = false;
    add(glowstone);

    BlockDropRule cobblestone = selfGatedRule(Block::Cobblestone, ToolType::Pickaxe, ToolTier::Wood);
    cobblestone.firedDrop = ItemId::Stone;
    add(cobblestone);

    add(selfRule(Block::Planks, ToolType::Axe));
    add(selfGatedRule(Block::Sandstone, ToolType::Pickaxe, ToolTier::Wood));

    BlockDropRule ice = barrenRule(Block::Ice, ToolType::Pickaxe);
    ice.silkDrop = ItemId::Ice;
    add(ice);

    add(barrenGatedRule(Block::Bedrock, ToolType::Pickaxe, ToolTier::Diamond));
    add(selfGatedRule(Block::Obsidian, ToolType::Pickaxe, ToolTier::Diamond));
    add(barrenRule(Block::Lava, ToolType::None));
    add(oreRule(Block::DiamondOre, ItemId::DiamondGem, 1, 1, ToolTier::Iron));
    add(oreRule(Block::RedstoneOre, ItemId::RedstoneDust, 4, 5, ToolTier::Stone));
    add(oreRule(Block::EmeraldOre, ItemId::EmeraldGem, 1, 1, ToolTier::Iron));
    add(oreRule(Block::LapisOre, ItemId::LapisLazuli, 4, 9, ToolTier::Stone));
    add(selfRule(Block::CraftingTable, ToolType::Axe));
    add(selfGatedRule(Block::Furnace, ToolType::Pickaxe, ToolTier::Wood));
    add(gatedRule(Block::FurnaceLit, ItemId::Furnace, ToolType::Pickaxe, ToolTier::Wood));
    add(selfRule(Block::Chest, ToolType::Axe));
    add(selfRule(Block::Torch, ToolType::None));
    add(plainRule(Block::RedstoneWire, ItemId::RedstoneDust, ToolType::None));
    add(selfRule(Block::RedstoneTorch, ToolType::None));
    add(selfRule(Block::RedstoneLamp, ToolType::None));
    add(plainRule(Block::RedstoneLampLit, ItemId::RedstoneLamp, ToolType::None));
    add(selfRule(Block::Lever, ToolType::None));
    add(selfRule(Block::Button, ToolType::None));
    add(selfGatedRule(Block::RedstoneBlock, ToolType::Pickaxe, ToolTier::Wood));
    add(selfRule(Block::Repeater, ToolType::None));
    add(selfRule(Block::Comparator, ToolType::None));
    add(selfGatedRule(Block::Piston, ToolType::Pickaxe, ToolTier::Wood));
    add(selfGatedRule(Block::PistonSticky, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::PistonHead, ToolType::Pickaxe, ToolTier::Wood));
    add(selfRule(Block::Tnt, ToolType::None));
    add(selfRule(Block::Ladder, ToolType::Axe));

    add(barrenRule(Block::Rail, ToolType::Pickaxe));
    add(barrenRule(Block::PoweredRail, ToolType::Pickaxe));
    add(barrenRule(Block::DetectorRail, ToolType::Pickaxe));
    add(barrenRule(Block::ActivatorRail, ToolType::Pickaxe));
    add(barrenGatedRule(Block::Hopper, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Dropper, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Dispenser, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Observer, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::TargetBlock, ToolType::Hoe, ToolTier::None));
    add(barrenRule(Block::SlimeBlock, ToolType::None));
    add(plankSalvage(Block::NoteBlock));

    add(plainRule(Block::Farmland, ItemId::Dirt, ToolType::Shovel));
    add(barrenRule(Block::Wheat, ToolType::None));
    add(barrenRule(Block::Carrot, ToolType::None));
    add(barrenRule(Block::Potato, ToolType::None));
    add(barrenRule(Block::Pumpkin, ToolType::Axe));
    add(barrenRule(Block::Melon, ToolType::Axe));
    add(barrenRule(Block::PumpkinStem, ToolType::None));
    add(barrenRule(Block::MelonStem, ToolType::None));
    add(barrenRule(Block::SugarCane, ToolType::None));
    add(barrenRule(Block::Cactus, ToolType::None));
    add(barrenRule(Block::Sapling, ToolType::None));

    BlockDropRule vine = barrenRule(Block::Vine, shearToolType);
    vine.shearHarvest = true;
    add(vine);

    add(plainRule(Block::SnowLayer, ItemId::Snow, ToolType::Shovel));
    add(barrenGatedRule(Block::PackedIce, ToolType::Pickaxe, ToolTier::Wood));

    add(barrenGatedRule(Block::BrewingStand, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::EnchantingTable, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Cauldron, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::AnvilBlock, ToolType::Pickaxe, ToolTier::Wood));
    add(plankSalvage(Block::Bookshelf, 3));

    BlockDropRule cobweb = barrenRule(Block::Cobweb, shearToolType);
    cobweb.shearHarvest = true;
    add(cobweb);

    add(barrenGatedRule(Block::Netherrack, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::SoulSand, ToolType::Shovel));
    add(barrenRule(Block::SoulSoil, ToolType::Shovel));
    add(barrenGatedRule(Block::NetherQuartzOre, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::NetherBrick, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::NetherPortal, ToolType::None));
    add(barrenGatedRule(Block::Magma, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Basalt, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Blackstone, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::EndStone, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::EndPortal, ToolType::None));
    add(barrenGatedRule(Block::EndPortalFrame, ToolType::Pickaxe, ToolTier::Diamond));
    add(barrenGatedRule(Block::Purpur, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::ChorusPlant, ToolType::Axe));
    add(barrenRule(Block::ChorusFlower, ToolType::Axe));

    BlockDropRule glass = barrenRule(Block::Glass, ToolType::Pickaxe);
    glass.shearHarvest = false;
    add(glass);

    add(barrenGatedRule(Block::Bricks, ToolType::Pickaxe, ToolTier::Wood));
    add(stoneSalvage(Block::StoneBricks));
    add(stoneSalvage(Block::MossyCobblestone));
    add(plankSalvage(Block::Door));
    add(plankSalvage(Block::Slab));
    add(plankSalvage(Block::Stairs));
    add(plankSalvage(Block::Fence));
    add(plankSalvage(Block::Bed));
    add(barrenGatedRule(Block::Bell, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Lantern, ToolType::Pickaxe, ToolTier::Wood));
    add(plankSalvage(Block::Composter));
    add(plankSalvage(Block::Barrel));
    add(plankSalvage(Block::SmithingTable));
    add(plankSalvage(Block::Loom));
    add(plankSalvage(Block::CartographyTable));
    add(plankSalvage(Block::Grindstone));
    add(barrenGatedRule(Block::Stonecutter, ToolType::Pickaxe, ToolTier::Wood));
    add(plankSalvage(Block::Lectern));
    add(barrenRule(Block::HayBale, ToolType::Hoe));

    add(barrenRule(Block::Clay, ToolType::Shovel));
    add(barrenGatedRule(Block::Terracotta, ToolType::Pickaxe, ToolTier::Wood));

    BlockDropRule wool = barrenRule(Block::Wool, shearToolType);
    wool.shearHarvest = true;
    add(wool);

    add(plainRule(Block::Mycelium, ItemId::Dirt, ToolType::Shovel));
    add(stoneSalvage(Block::Andesite));
    add(stoneSalvage(Block::Diorite));
    add(stoneSalvage(Block::Granite));
    add(stoneSalvage(Block::Deepslate));
    add(barrenGatedRule(Block::CopperOre, ToolType::Pickaxe, ToolTier::Stone));
    add(stoneSalvage(Block::Tuff));
    add(stoneSalvage(Block::Dripstone));
    add(barrenGatedRule(Block::AmethystBlock, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::Sculk, ToolType::Hoe));
    add(barrenGatedRule(Block::CryingObsidian, ToolType::Pickaxe, ToolTier::Diamond));
    add(barrenGatedRule(Block::Prismarine, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenRule(Block::SeaLantern, ToolType::Pickaxe));

    BlockDropRule kelp = barrenRule(Block::Kelp, shearToolType);
    kelp.shearHarvest = true;
    add(kelp);

    add(barrenRule(Block::Coral, ToolType::Pickaxe));
    add(barrenRule(Block::Mud, ToolType::Shovel));

    BlockDropRule moss = barrenRule(Block::Moss, shearToolType);
    moss.shearHarvest = true;
    add(moss);

    return rules;
}

MobDropRule mobRule(MobDropRule::Species species, const char* name, ItemId drop, int minCount,
                    int maxCount, int experience) {
    MobDropRule rule;
    rule.species = species;
    rule.name = name;
    rule.drop = drop;
    rule.minCount = minCount;
    rule.maxCount = maxCount;
    rule.experience = experience;
    return rule;
}

MobDropRule withRare(MobDropRule rule, ItemId rare, float chance, float perLooting) {
    rule.rareDrop = rare;
    rule.rareChance = chance;
    rule.rareChancePerLooting = perLooting;
    return rule;
}

MobDropRule livestock(MobDropRule::Species species, const char* name, ItemId raw, ItemId cooked,
                      int minCount, int maxCount) {
    MobDropRule rule = mobRule(species, name, raw, minCount, maxCount, 2);
    rule.firedDrop = cooked;
    rule.hostile = false;
    return rule;
}

MobDropRule undeadRule(MobDropRule rule) {
    rule.undead = true;
    return rule;
}

std::vector<MobDropRule> buildMobRules() {
    using Species = MobDropRule::Species;
    std::vector<MobDropRule> rules;
    auto add = [&rules](const MobDropRule& rule) { rules.push_back(rule); };

    add(undeadRule(withRare(mobRule(Species::Zombie, "zombie", ItemId::RottenFlesh, 0, 2, 5),
                            ItemId::IronIngot, 0.025f, 0.01f)));
    add(undeadRule(withRare(mobRule(Species::Husk, "husk", ItemId::RottenFlesh, 0, 2, 5),
                            ItemId::IronIngot, 0.025f, 0.01f)));
    add(undeadRule(withRare(mobRule(Species::Drowned, "drowned", ItemId::RottenFlesh, 0, 2, 5),
                            ItemId::GoldIngot, 0.05f, 0.01f)));
    add(undeadRule(withRare(mobRule(Species::ZombiePigman, "zombiePigman", ItemId::RottenFlesh, 0, 1,
                                    5),
                            ItemId::GoldIngot, 0.025f, 0.01f)));

    add(undeadRule(withRare(mobRule(Species::Skeleton, "skeleton", ItemId::Stick, 0, 2, 5),
                            ItemId::CoalLump, 0.05f, 0.01f)));
    add(undeadRule(withRare(mobRule(Species::Stray, "stray", ItemId::Stick, 0, 2, 5),
                            ItemId::CoalLump, 0.05f, 0.01f)));

    add(mobRule(Species::Creeper, "creeper", ItemId::Gunpowder, 0, 2, 5));

    MobDropRule witch = withRare(mobRule(Species::Witch, "witch", ItemId::RedstoneDust, 0, 2, 5),
                                 ItemId::Glowstone, 0.25f, 0.05f);
    witch.rareNeedsPlayerKill = false;
    add(witch);

    MobDropRule spider = mobRule(Species::Spider, "spider", ItemId::Stick, 0, 2, 5);
    spider.arthropod = true;
    add(spider);

    MobDropRule caveSpider = mobRule(Species::CaveSpider, "caveSpider", ItemId::Stick, 0, 2, 5);
    caveSpider.arthropod = true;
    add(caveSpider);

    MobDropRule blaze = mobRule(Species::Blaze, "blaze", ItemId::CoalLump, 0, 1, 10);
    blaze.dropNeedsPlayerKill = true;
    add(blaze);

    add(mobRule(Species::Ghast, "ghast", ItemId::Gunpowder, 0, 2, 5));

    MobDropRule golem = mobRule(Species::IronGolem, "ironGolem", ItemId::IronIngot, 3, 5, 0);
    golem.lootingRaisesCount = false;
    golem.hostile = false;
    add(golem);

    MobDropRule villager = mobRule(Species::Villager, "villager", ItemId::None, 0, 0, 0);
    villager.hostile = false;
    add(villager);

    add(livestock(Species::Sheep, "sheep", ItemId::RawMutton, ItemId::CookedMutton, 1, 2));
    add(livestock(Species::Cow, "cow", ItemId::RawBeef, ItemId::CookedBeef, 1, 3));
    add(livestock(Species::Pig, "pig", ItemId::RawPorkchop, ItemId::CookedPorkchop, 1, 3));
    return rules;
}

LootEntry chestEntry(ItemId item, int weight, int minCount, int maxCount) {
    LootEntry entry;
    entry.item = item;
    entry.weight = weight;
    entry.minCount = minCount;
    entry.maxCount = maxCount;
    if (minCount != maxCount) entry.functions = LootEntry::CountRange;
    return entry;
}

ChestFill chestFillOf(ChestFill::Structure structure, const char* name, int minRolls, int maxRolls,
                      int chestsMin, int chestsMax, const std::vector<LootEntry>& entries) {
    ChestFill fill;
    fill.structure = structure;
    fill.name = name;
    fill.pool.minRolls = minRolls;
    fill.pool.maxRolls = maxRolls;
    fill.pool.entries = entries;
    fill.chestsMin = chestsMin;
    fill.chestsMax = chestsMax;
    return fill;
}

std::vector<ChestFill> buildChestFills() {
    using Structure = ChestFill::Structure;
    std::vector<ChestFill> fills;

    fills.push_back(chestFillOf(Structure::Village, "village", 3, 5, 2, 4,
                                {chestEntry(ItemId::Bread, 10, 1, 3),
                                 chestEntry(ItemId::Apple, 8, 1, 3),
                                 chestEntry(ItemId::Wood, 6, 1, 3),
                                 chestEntry(ItemId::Stick, 6, 1, 4),
                                 chestEntry(ItemId::IronIngot, 5, 1, 2),
                                 chestEntry(ItemId::WoodHoe, 3, 1, 1),
                                 chestEntry(ItemId::EmeraldGem, 2, 1, 2)}));

    fills.push_back(chestFillOf(Structure::Dungeon, "dungeon", 2, 4, 1, 2,
                                {chestEntry(ItemId::IronIngot, 10, 1, 4),
                                 chestEntry(ItemId::Bread, 10, 1, 1),
                                 chestEntry(ItemId::CoalLump, 10, 1, 4),
                                 chestEntry(ItemId::Apple, 8, 1, 3),
                                 chestEntry(ItemId::RedstoneDust, 5, 1, 4),
                                 chestEntry(ItemId::GoldIngot, 5, 1, 4),
                                 chestEntry(ItemId::LapisLazuli, 5, 1, 3),
                                 chestEntry(ItemId::DiamondGem, 1, 1, 2)}));

    fills.push_back(chestFillOf(Structure::Mineshaft, "mineshaft", 3, 5, 1, 3,
                                {chestEntry(ItemId::CoalLump, 10, 1, 8),
                                 chestEntry(ItemId::IronIngot, 10, 1, 5),
                                 chestEntry(ItemId::Bread, 8, 1, 3),
                                 chestEntry(ItemId::Gravel, 8, 1, 6),
                                 chestEntry(ItemId::Stick, 6, 1, 8),
                                 chestEntry(ItemId::GoldIngot, 4, 1, 3),
                                 chestEntry(ItemId::RedstoneDust, 4, 1, 4),
                                 chestEntry(ItemId::LapisLazuli, 3, 1, 4),
                                 chestEntry(ItemId::IronPickaxe, 2, 1, 1),
                                 chestEntry(ItemId::DiamondGem, 1, 1, 2)}));

    fills.push_back(chestFillOf(Structure::Temple, "temple", 2, 4, 2, 4,
                                {chestEntry(ItemId::Gunpowder, 5, 1, 8),
                                 chestEntry(ItemId::RottenFlesh, 5, 3, 7),
                                 chestEntry(ItemId::GoldIngot, 5, 1, 3),
                                 chestEntry(ItemId::Bread, 5, 1, 1),
                                 chestEntry(ItemId::LapisLazuli, 2, 1, 3),
                                 chestEntry(ItemId::DiamondGem, 1, 1, 3),
                                 chestEntry(ItemId::EmeraldGem, 1, 1, 3)}));

    fills.push_back(chestFillOf(Structure::NetherFortress, "netherFortress", 2, 4, 1, 4,
                                {chestEntry(ItemId::GoldIngot, 10, 1, 3),
                                 chestEntry(ItemId::Obsidian, 5, 2, 4),
                                 chestEntry(ItemId::RedstoneDust, 5, 1, 4),
                                 chestEntry(ItemId::Gunpowder, 5, 1, 4),
                                 chestEntry(ItemId::IronIngot, 5, 1, 5),
                                 chestEntry(ItemId::Bread, 5, 1, 1),
                                 chestEntry(ItemId::GoldSword, 5, 1, 1),
                                 chestEntry(ItemId::DiamondGem, 2, 1, 3)}));

    fills.push_back(chestFillOf(Structure::EndShip, "endShip", 2, 3, 1, 1,
                                {chestEntry(ItemId::DiamondGem, 5, 1, 3),
                                 chestEntry(ItemId::EmeraldGem, 5, 1, 3),
                                 chestEntry(ItemId::GoldIngot, 5, 1, 6),
                                 chestEntry(ItemId::IronIngot, 5, 1, 6),
                                 chestEntry(ItemId::DiamondSword, 2, 1, 1),
                                 chestEntry(ItemId::DiamondPickaxe, 2, 1, 1)}));
    return fills;
}

std::vector<LootTable> buildFishingTables() {
    std::vector<LootTable> tables;

    LootPool fish;
    fish.entries = {chestEntry(ItemId::RawPorkchop, 34, 1, 1), chestEntry(ItemId::RawBeef, 33, 1, 1),
                    chestEntry(ItemId::RawMutton, 33, 1, 1)};

    LootPool treasure;
    treasure.entries = {chestEntry(ItemId::GoldIngot, 25, 1, 1),
                        chestEntry(ItemId::LapisLazuli, 25, 1, 3),
                        chestEntry(ItemId::EmeraldGem, 20, 1, 1),
                        chestEntry(ItemId::IronIngot, 20, 1, 2),
                        chestEntry(ItemId::DiamondGem, 10, 1, 1)};

    LootPool junk;
    junk.entries = {chestEntry(ItemId::Stick, 30, 1, 2), chestEntry(ItemId::RottenFlesh, 25, 1, 1),
                    chestEntry(ItemId::Gravel, 20, 1, 1), chestEntry(ItemId::Leaves, 15, 1, 1),
                    chestEntry(ItemId::WoodHoe, 10, 1, 1)};

    for (const LootPool& pool : {fish, treasure, junk}) {
        LootTable table;
        table.pools.push_back(pool);
        tables.push_back(table);
    }
    return tables;
}

const LootTable& emptyLootTable() {
    static const LootTable table;
    return table;
}

}

int toolHarvestLevel(ToolTier tier) {
    switch (tier) {
        case ToolTier::None:    return 0;
        case ToolTier::Wood:    return 1;
        case ToolTier::Gold:    return 1;
        case ToolTier::Stone:   return 2;
        case ToolTier::Iron:    return 3;
        case ToolTier::Diamond: return 4;
    }
    return 0;
}

bool toolAllows(const BlockDropRule& rule, ItemId tool) {
    if (!rule.toolRequiredForDrop) return true;
    const ItemInfo& info = itemInfo(tool);
    if (info.toolType != rule.harvestTool) return false;
    return toolHarvestLevel(info.toolTier) >= toolHarvestLevel(rule.minimumTier);
}

LootTable blockRuleTable(const BlockDropRule& rule) {
    LootTable table;

    ItemId harvested = rule.silkTouchDrop();
    if (harvested != ItemId::None) {
        LootEntry silk;
        silk.item = harvested;
        silk.conditions = LootEntry::SilkTouched;
        appendPool(table, silk);
    }

    if (rule.drop != ItemId::None) {
        LootEntry broken;
        broken.item = rule.drop;
        broken.minCount = rule.minCount;
        broken.maxCount = rule.maxCount;
        broken.chance = rule.dropChance;
        broken.conditions = LootEntry::NotSilkTouched;
        if (rule.firedDrop != ItemId::None) broken.conditions |= LootEntry::NotFiery;
        if (rule.minCount != rule.maxCount) broken.functions |= LootEntry::CountRange;
        if (rule.fortuneMultiplies) broken.functions |= LootEntry::OreFortune;
        if (rule.chancePerFortune > 0.0f) {
            broken.functions |= LootEntry::ChanceFortune;
            broken.chancePerLevel = rule.chancePerFortune;
        }
        appendPool(table, broken);
    }

    if (rule.firedDrop != ItemId::None) {
        LootEntry burned;
        burned.item = rule.firedDrop;
        burned.minCount = rule.minCount;
        burned.maxCount = rule.maxCount;
        burned.conditions = LootEntry::NotSilkTouched | LootEntry::Fiery;
        if (rule.minCount != rule.maxCount) burned.functions |= LootEntry::CountRange;
        appendPool(table, burned);
    }

    if (rule.bonusDrop != ItemId::None) {
        LootEntry bonus;
        bonus.item = rule.bonusDrop;
        bonus.minCount = rule.bonusMin;
        bonus.maxCount = rule.bonusMax;
        bonus.chance = rule.bonusChance;
        bonus.conditions = LootEntry::NotSilkTouched;
        if (rule.bonusMin != rule.bonusMax) bonus.functions |= LootEntry::CountRange;
        appendPool(table, bonus);
    }
    return table;
}

LootTable mobRuleTable(const MobDropRule& rule) {
    LootTable table;
    if (rule.drop != ItemId::None) {
        LootEntry common;
        common.item = rule.drop;
        common.minCount = rule.minCount;
        common.maxCount = rule.maxCount;
        if (rule.minCount != rule.maxCount) common.functions |= LootEntry::CountRange;
        if (rule.lootingRaisesCount) common.functions |= LootEntry::LootingBonus;
        if (rule.dropNeedsPlayerKill) common.conditions |= LootEntry::PlayerKill;
        if (rule.firedDrop != ItemId::None) common.conditions |= LootEntry::NotFiery;
        appendPool(table, common);

        if (rule.firedDrop != ItemId::None) {
            LootEntry cooked = common;
            cooked.item = rule.firedDrop;
            cooked.conditions = (common.conditions & ~LootEntry::NotFiery) | LootEntry::Fiery;
            appendPool(table, cooked);
        }
    }

    if (rule.rareDrop != ItemId::None) {
        LootEntry rare;
        rare.item = rule.rareDrop;
        rare.chance = rule.rareChance;
        rare.chancePerLevel = rule.rareChancePerLooting;
        if (rule.rareChancePerLooting > 0.0f) rare.functions |= LootEntry::ChanceLooting;
        if (rule.rareNeedsPlayerKill) rare.conditions |= LootEntry::PlayerKill;
        appendPool(table, rare);
    }
    return table;
}

LootTableId LootTableId::ofBlock(Block block) {
    return LootTableId{Domain::Block, static_cast<uint16_t>(block)};
}

LootTableId LootTableId::ofMob(MobDropRule::Species species) {
    return LootTableId{Domain::Mob, static_cast<uint16_t>(species)};
}

LootTableId LootTableId::ofChest(ChestFill::Structure structure) {
    return LootTableId{Domain::Chest, static_cast<uint16_t>(structure)};
}

LootTableId LootTableId::ofFishing(FishingLoot::Pool pool) {
    return LootTableId{Domain::Fishing, static_cast<uint16_t>(pool)};
}

bool LootTableId::valid() const {
    switch (domain) {
        case Domain::Block:   return index < static_cast<uint16_t>(Block::Count);
        case Domain::Mob:     return index < static_cast<uint16_t>(MobDropRule::Species::Count);
        case Domain::Chest:   return index < static_cast<uint16_t>(ChestFill::Structure::Count);
        case Domain::Fishing: return index < static_cast<uint16_t>(FishingLoot::Pool::Count);
        case Domain::Count:   return false;
    }
    return false;
}

LootRegistry::LootRegistry()
    : blocks(buildBlockRules()),
      mobs(buildMobRules()),
      chests(buildChestFills()),
      fishingTables(buildFishingTables()) {
    for (const BlockDropRule& rule : blocks) blockTables.push_back(blockRuleTable(rule));
    for (const MobDropRule& rule : mobs) mobTables.push_back(mobRuleTable(rule));
    for (const ChestFill& fill : chests) {
        LootTable table;
        table.pools.push_back(fill.pool);
        chestTables.push_back(table);
    }
}

const BlockDropRule& LootRegistry::blockRule(Block block) const {
    return blocks[static_cast<std::size_t>(block)];
}

const MobDropRule& LootRegistry::mobRule(MobDropRule::Species species) const {
    return mobs[static_cast<std::size_t>(species)];
}

const ChestFill& LootRegistry::chestFill(ChestFill::Structure structure) const {
    return chests[static_cast<std::size_t>(structure)];
}

bool LootRegistry::coversEveryBlock() const {
    if (blocks.size() != static_cast<std::size_t>(Block::Count)) return false;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].block != static_cast<Block>(i)) return false;
    }
    return true;
}

bool LootRegistry::coversEveryMob() const {
    if (mobs.size() != static_cast<std::size_t>(MobDropRule::Species::Count)) return false;
    for (std::size_t i = 0; i < mobs.size(); ++i) {
        if (mobs[i].species != static_cast<MobDropRule::Species>(i)) return false;
    }
    return true;
}

bool LootRegistry::coversEveryStructure() const {
    if (chests.size() != static_cast<std::size_t>(ChestFill::Structure::Count)) return false;
    for (std::size_t i = 0; i < chests.size(); ++i) {
        if (chests[i].structure != static_cast<ChestFill::Structure>(i)) return false;
    }
    return true;
}

const LootTable& LootRegistry::tableFor(LootTableId id) const {
    if (!id.valid()) return emptyLootTable();
    std::size_t index = static_cast<std::size_t>(id.index);
    switch (id.domain) {
        case LootTableId::Domain::Block:   return blockTables[index];
        case LootTableId::Domain::Mob:     return mobTables[index];
        case LootTableId::Domain::Chest:   return chestTables[index];
        case LootTableId::Domain::Fishing: return fishingTables[index];
        case LootTableId::Domain::Count:   break;
    }
    return emptyLootTable();
}

LootRoll LootRegistry::rollBlock(Block block, const LootContext& context) const {
    if (block >= Block::Count) return {};
    const BlockDropRule& rule = blockRule(block);
    if (!toolAllows(rule, context.tool)) return {};

    if (rule.shearHarvest && itemInfo(context.tool).toolType == shearToolType) {
        LootRoll sheared;
        ItemId harvested = rule.silkTouchDrop();
        if (harvested != ItemId::None) sheared.items.push_back({harvested, 1, 0});
        return sheared;
    }
    return rollLootTable(blockTables[static_cast<std::size_t>(block)], context);
}

LootRoll LootRegistry::rollMob(MobDropRule::Species species, const LootContext& context) const {
    if (species >= MobDropRule::Species::Count) return {};
    return rollLootTable(mobTables[static_cast<std::size_t>(species)], context);
}

LootRoll LootRegistry::rollChest(ChestFill::Structure structure, const LootContext& context) const {
    if (structure >= ChestFill::Structure::Count) return {};
    return rollLootTable(chestTables[static_cast<std::size_t>(structure)], context);
}

int LootRegistry::chestCountFor(ChestFill::Structure structure, uint32_t seed) const {
    if (structure >= ChestFill::Structure::Count) return 0;
    const ChestFill& fill = chestFill(structure);
    uint32_t rng = seed != 0 ? seed : defaultLootSeed;
    return rollBetween(rng, fill.chestsMin, fill.chestsMax);
}

FishingLoot LootRegistry::rollFishing(const LootContext& context) const {
    uint32_t rng = context.seed != 0 ? context.seed : defaultLootSeed;
    int luck = context.fortune();
    int lure = context.toolEnchants.levelOf(EnchantKind::Efficiency);

    FishingLoot outcome;
    outcome.fishWeight = fishingFishWeight;
    outcome.treasureWeight = fishingTreasureWeight + fishingLuckWeightShift * luck;
    outcome.junkWeight = std::max(0, fishingJunkWeight - fishingLuckWeightShift * luck);

    int pick = rollBelow(rng, outcome.fishWeight + outcome.treasureWeight + outcome.junkWeight);
    if (pick < outcome.junkWeight) {
        outcome.pool = FishingLoot::Pool::Junk;
    } else if (pick < outcome.junkWeight + outcome.treasureWeight) {
        outcome.pool = FishingLoot::Pool::Treasure;
    } else {
        outcome.pool = FishingLoot::Pool::Fish;
    }

    int waited = rollBetween(rng, fishingMinWaitTicks, fishingBaseWaitTicks);
    outcome.waitTicks = std::max(fishingMinWaitTicks / 2,
                                 waited - lure * fishingLureTicksPerLevel);

    LootContext poolContext = context;
    poolContext.seed = rng;
    outcome.caught = rollLootTable(fishingTables[static_cast<std::size_t>(outcome.pool)],
                                   poolContext);
    return outcome;
}

const LootRegistry& lootRegistry() {
    static const LootRegistry registry;
    return registry;
}

}
