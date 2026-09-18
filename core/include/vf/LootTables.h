#pragma once

#include <cstdint>
#include <vector>

#include "Loot.h"

namespace vf {

constexpr ToolType shearToolType = ToolType::Hoe;

constexpr int fishingBaseWaitTicks = 600;
constexpr int fishingMinWaitTicks = 100;
constexpr int fishingLureTicksPerLevel = 100;
constexpr int fishingFishWeight = 85;
constexpr int fishingTreasureWeight = 5;
constexpr int fishingJunkWeight = 10;
constexpr int fishingLuckWeightShift = 2;

int toolHarvestLevel(ToolTier tier);

struct BlockDropRule {
    Block block = Block::Air;
    ItemId drop = ItemId::None;
    int minCount = 1;
    int maxCount = 1;
    ToolType harvestTool = ToolType::None;
    ToolTier minimumTier = ToolTier::None;
    bool toolRequiredForDrop = false;
    ItemId silkDrop = ItemId::None;
    bool shearHarvest = false;
    ItemId firedDrop = ItemId::None;
    bool fortuneMultiplies = false;
    float dropChance = 1.0f;
    float chancePerFortune = 0.0f;
    ItemId bonusDrop = ItemId::None;
    float bonusChance = 0.0f;
    int bonusMin = 1;
    int bonusMax = 1;

    ItemId silkTouchDrop() const { return silkDrop != ItemId::None ? silkDrop : drop; }
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
    int chestsMin = 1;
    int chestsMax = 1;
};

struct FishingLoot {
    enum class Pool : uint8_t { Fish, Treasure, Junk, Count };

    Pool pool = Pool::Fish;
    LootRoll caught;
    int waitTicks = 0;
    int fishWeight = 0;
    int treasureWeight = 0;
    int junkWeight = 0;
};

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

bool toolAllows(const BlockDropRule& rule, ItemId tool);

LootTable blockRuleTable(const BlockDropRule& rule);
LootTable mobRuleTable(const MobDropRule& rule);

class LootRegistry {
public:
    LootRegistry();

    const BlockDropRule& blockRule(Block block) const;
    const MobDropRule& mobRule(MobDropRule::Species species) const;
    const ChestFill& chestFill(ChestFill::Structure structure) const;

    const std::vector<BlockDropRule>& blockRules() const { return blocks; }
    const std::vector<MobDropRule>& mobRules() const { return mobs; }
    const std::vector<ChestFill>& chestFills() const { return chests; }

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

}
