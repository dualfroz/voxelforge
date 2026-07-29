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

// Blocks the palette has no item for still carry a rule: it names the tool class
// and harvest level, and records that the block yields nothing rather than
// inventing an item that Item.h does not have.
BlockDropRule barrenRule(Block block, ToolType tool) { return plainRule(block, ItemId::None, tool); }

BlockDropRule barrenGatedRule(Block block, ToolType tool, ToolTier tier) {
    return gatedRule(block, ItemId::None, tool, tier);
}

// Wooden crafted blocks salvage back to the planks they were built from.
BlockDropRule plankSalvage(Block block, int count = 1) {
    BlockDropRule rule = plainRule(block, ItemId::Planks, ToolType::Axe);
    rule.minCount = count;
    rule.maxCount = count;
    return rule;
}

// The palette carries exactly one broken stone item, so the overworld stone
// family salvages to cobblestone.
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

    // No sapling in the palette, so a wood cutting stands in as the replantable
    // drop, the way the salvaged leaf table already does.
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

    // Ice melts away unless it is taken whole.
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
    // The head is part of the body; breaking it drops the body, not itself.
    add(barrenGatedRule(Block::PistonHead, ToolType::Pickaxe, ToolTier::Wood));
    add(selfRule(Block::Tnt, ToolType::None));
    add(selfRule(Block::Ladder, ToolType::Axe));

    // Automation and logistics: crafted from iron and planks, neither of which
    // the palette can hand back as a block item.
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

    // Farming: tilled soil goes back to dirt, crops have no seed item to give.
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

    // Brewing, enchanting and combat fixtures.
    add(barrenGatedRule(Block::BrewingStand, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::EnchantingTable, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::Cauldron, ToolType::Pickaxe, ToolTier::Wood));
    add(barrenGatedRule(Block::AnvilBlock, ToolType::Pickaxe, ToolTier::Wood));
    // No book in the palette, so a shelf salvages the planks of its frame.
    add(plankSalvage(Block::Bookshelf, 3));

    BlockDropRule cobweb = barrenRule(Block::Cobweb, shearToolType);
    cobweb.shearHarvest = true;
    add(cobweb);

    // Nether and End.
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

    // Village and construction.
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

    // Geology and decoration.
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

    // No bone or arrow item in the palette, so a skeleton leaves shafts behind,
    // the same substitution the salvaged mob table already makes.
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

    // Blaze rods only ever come off a player kill, and coal stands in for the
    // rod the palette does not carry.
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

