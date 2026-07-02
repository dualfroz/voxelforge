#pragma once

#include <cstdint>
#include <vector>

#include "Loot.h"

namespace vf {

// The item palette carries no shears, and correctToolFor already treats leaves
// as a hoe block, so the hoe is the shear class tool throughout the registry.
constexpr ToolType shearToolType = ToolType::Hoe;

constexpr int fishingBaseWaitTicks = 600;
constexpr int fishingMinWaitTicks = 100;
constexpr int fishingLureTicksPerLevel = 100;
constexpr int fishingFishWeight = 85;
constexpr int fishingTreasureWeight = 5;
constexpr int fishingJunkWeight = 10;
constexpr int fishingLuckWeightShift = 2;

// Harvest level a tier reaches. Gold sits at the wooden level, which is why a
// golden pickaxe cannot take a diamond.
int toolHarvestLevel(ToolTier tier);

// One authored row per block in the palette. `harvestTool` always names the tool
// class the block is broken with, whether or not a drop depends on it;
// `toolRequiredForDrop` is what actually gates the drop.
struct BlockDropRule {
    Block block = Block::Air;
    // ItemId::None means the block yields nothing. For the blocks added after
    // the item palette froze at ItemId::Ladder that is a decision, not a gap:
    // there is no item to hand out and none is invented.
    ItemId drop = ItemId::None;
    int minCount = 1;
    int maxCount = 1;
    ToolType harvestTool = ToolType::None;
    ToolTier minimumTier = ToolTier::None;
    bool toolRequiredForDrop = false;
    // What Silk Touch takes instead; ItemId::None falls back to `drop`.
    ItemId silkDrop = ItemId::None;
    // Shear class tools take the silk drop too, without the enchantment.
    bool shearHarvest = false;
    // What comes out when the block breaks inside fire; ItemId::None means fire
    // changes nothing.
    ItemId firedDrop = ItemId::None;
    // Ore style: Fortune multiplies the whole count.
    bool fortuneMultiplies = false;
    // Leaf style: Fortune improves the odds instead of the count.
    float dropChance = 1.0f;
    float chancePerFortune = 0.0f;
    ItemId bonusDrop = ItemId::None;
    float bonusChance = 0.0f;
    int bonusMin = 1;
    int bonusMax = 1;

    ItemId silkTouchDrop() const { return silkDrop != ItemId::None ? silkDrop : drop; }
    // Ice yields nothing when it is smashed but something when it is taken
    // whole, so the silk branch has to count here too.
    bool yieldsNothing() const {
        return silkTouchDrop() == ItemId::None && bonusDrop == ItemId::None;
    }
};

struct MobDropRule {
    enum class Species : uint8_t {
        Zombie, Husk, Drowned, ZombiePigman, Skeleton, Stray, Creeper, Witch, Spider, CaveSpider,
        Blaze, Ghast, IronGolem, Villager, Sheep, Cow, Pig,
        Count
    };

    Species species = Species::Zombie;
    const char* name = "";
    ItemId drop = ItemId::None;
    int minCount = 0;
    int maxCount = 0;
    bool lootingRaisesCount = true;
    bool dropNeedsPlayerKill = false;
    // Cooked counterpart when the kill was fiery; ItemId::None means fire
    // changes nothing.
    ItemId firedDrop = ItemId::None;
    ItemId rareDrop = ItemId::None;
    float rareChance = 0.0f;
    float rareChancePerLooting = 0.0f;
    bool rareNeedsPlayerKill = true;
    int experience = 0;
    bool undead = false;
    bool arthropod = false;
    bool hostile = true;
};

struct ChestFill {
    enum class Structure : uint8_t {
        Village, Dungeon, Mineshaft, Temple, NetherFortress, EndShip,
        Count
    };

    Structure structure = Structure::Village;
    const char* name = "";
    LootPool pool;
    // How many chests one generated structure of this kind holds.
    int chestsMin = 1;
    int chestsMax = 1;
};

struct FishingLoot {
    // EnchantKind has no Luck of the Sea and no Lure, so Fortune on the rod
    // shifts the pool weights and Efficiency shortens the wait. The palette has
    // no fish either: the catch pool hands back the raw meats it does have.
    enum class Pool : uint8_t { Fish, Treasure, Junk, Count };

    Pool pool = Pool::Fish;
    LootRoll caught;
    int waitTicks = 0;
    // The weights the roll actually used, after the luck shift.
    int fishWeight = 0;
    int treasureWeight = 0;
    int junkWeight = 0;
};

// A handle to any table in the registry, so a caller can hold on to one without
// caring which family it came from.
struct LootTableId {
    enum class Domain : uint8_t { Block, Mob, Chest, Fishing, Count };

    Domain domain = Domain::Block;
    uint16_t index = 0;

    static LootTableId ofBlock(Block block);
    static LootTableId ofMob(MobDropRule::Species species);
    static LootTableId ofChest(ChestFill::Structure structure);
    static LootTableId ofFishing(FishingLoot::Pool pool);

    bool valid() const;
    bool operator==(const LootTableId& other) const {
        return domain == other.domain && index == other.index;
    }
    bool operator!=(const LootTableId& other) const { return !(*this == other); }
};

// Whether the tool in hand clears the rule's tool class and harvest level.
bool toolAllows(const BlockDropRule& rule, ItemId tool);

// Rules expressed as the pools the shared roller in Loot.cpp understands.
LootTable blockRuleTable(const BlockDropRule& rule);
LootTable mobRuleTable(const MobDropRule& rule);

// Every block, mob, structure chest and fishing pool as data, authored one row
// per enum value and in enum order.
class LootRegistry {
public:
    LootRegistry();

    const BlockDropRule& blockRule(Block block) const;
    const MobDropRule& mobRule(MobDropRule::Species species) const;
    const ChestFill& chestFill(ChestFill::Structure structure) const;

    const std::vector<BlockDropRule>& blockRules() const { return blocks; }
    const std::vector<MobDropRule>& mobRules() const { return mobs; }
    const std::vector<ChestFill>& chestFills() const { return chests; }

    // Each of these checks the authored table is complete and in enum order.
    bool coversEveryBlock() const;
    bool coversEveryMob() const;
    bool coversEveryStructure() const;

    const LootTable& tableFor(LootTableId id) const;

    LootRoll rollBlock(Block block, const LootContext& context) const;
    LootRoll rollMob(MobDropRule::Species species, const LootContext& context) const;
    LootRoll rollChest(ChestFill::Structure structure, const LootContext& context) const;
    int chestCountFor(ChestFill::Structure structure, uint32_t seed) const;
    FishingLoot rollFishing(const LootContext& context) const;

private:
    std::vector<BlockDropRule> blocks;
    std::vector<MobDropRule> mobs;
    std::vector<ChestFill> chests;
    std::vector<LootTable> blockTables;
    std::vector<LootTable> mobTables;
    std::vector<LootTable> chestTables;
    std::vector<LootTable> fishingTables;
};

const LootRegistry& lootRegistry();

} // namespace vf
