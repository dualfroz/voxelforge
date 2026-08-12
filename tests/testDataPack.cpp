#include "testHarness.h"
#include "vf/DataPack.h"

#include <cstdio>
#include <string>

using namespace vf;

namespace {

ResourceId id(const std::string& text) {
    ResourceId parsed;
    ResourceId::parse(text, "voxelforge", parsed);
    return parsed;
}

const char* const redstoneTorchShaped = R"json({
  "type": "craftingShaped",
  "pattern": ["R", "S"],
  "key": {"R": "voxelforge:redstone_dust", "S": "voxelforge:stick"},
  "mirror": false,
  "result": {"item": "voxelforge:redstone_torch", "count": 4}
})json";

const char* const redstoneTorchRicher = R"json({
  "type": "craftingShaped",
  "pattern": ["R", "S"],
  "key": {"R": "voxelforge:redstone_dust", "S": "voxelforge:stick"},
  "mirror": false,
  "result": {"item": "voxelforge:redstone_torch", "count": 8}
})json";

const char* const appleBread = R"json({
  "type": "craftingShapeless",
  "ingredients": ["voxelforge:apple", "voxelforge:apple"],
  "result": {"item": "voxelforge:bread", "count": 1}
})json";

const char* const sandSmelting = R"json({
  "type": "smelting",
  "ingredient": "voxelforge:sand",
  "seconds": 12.5,
  "experience": 0.7,
  "result": {"item": "voxelforge:sandstone", "count": 1}
})json";

const char* const starterChest = R"json({
  "pools": [
    {
      "rolls": 1,
      "entries": [
        {"item": "voxelforge:bread", "weight": 1, "count": 2}
      ]
    },
    {
      "rolls": 1,
      "entries": [
        {"item": "voxelforge:diamond_gem", "weight": 1, "count": 1,
         "conditions": ["playerKill"], "functions": ["lootingBonus"]}
      ]
    }
  ]
})json";

DataPack basePack() {
    DataPackMeta meta;
    meta.name = "base";
    DataPack pack(meta);
    pack.addFile("pack.json",
                 R"json({"name": "base", "description": "the shipped content",
                          "formatVersion": 1, "defaultSpace": "voxelforge"})json");
    pack.addFile("data/voxelforge/recipes/redstone_torch.json", redstoneTorchShaped);
    pack.addFile("data/voxelforge/recipes/bread.json", appleBread);
    pack.addFile("data/voxelforge/recipes/sandstone.json", sandSmelting);
    pack.addFile("data/voxelforge/lootTables/chests/starter.json", starterChest);
    pack.addFile("data/voxelforge/tags/blocks/logs.json",
                 R"json({"values": ["voxelforge:wood"]})json");
    pack.addFile("data/voxelforge/tags/blocks/mineable.json",
                 R"json({"values": ["#voxelforge:logs", "voxelforge:stone", "voxelforge:sand"]})json");
    pack.addFile("data/voxelforge/tags/items/fuel.json",
                 R"json({"values": ["voxelforge:coal_lump", "voxelforge:planks"]})json");
    pack.addFile("readme.txt", "not a data file");
    return pack;
}

DataPack overlayPack() {
    DataPackMeta meta;
    meta.name = "overlay";
    meta.priority = 10;
    DataPack pack(meta);
    pack.addFile("data/voxelforge/recipes/redstone_torch.json", redstoneTorchRicher);
    pack.addFile("data/voxelforge/tags/blocks/logs.json",
                 R"json({"values": ["voxelforge:planks"]})json");
    return pack;
}

} // namespace

VF_TEST(resourceIdsAcceptNamespacedLowercasePathsAndRefuseTheRest) {
    ResourceId parsed;
    CHECK(ResourceId::parse("voxelforge:iron_ingot", "fallback", parsed));
    CHECK_EQ(parsed.space, std::string("voxelforge"));
    CHECK_EQ(parsed.path, std::string("iron_ingot"));
    CHECK_EQ(parsed.text(), std::string("voxelforge:iron_ingot"));

    CHECK(ResourceId::parse("chests/starter", "voxelforge", parsed));
    CHECK_EQ(parsed.space, std::string("voxelforge"));
    CHECK_EQ(parsed.path, std::string("chests/starter"));

    CHECK(!ResourceId::parse("Voxelforge:stone", "voxelforge", parsed));
    CHECK(!ResourceId::parse("voxelforge:Stone", "voxelforge", parsed));
    CHECK(!ResourceId::parse("a:b:c", "voxelforge", parsed));
    CHECK(!ResourceId::parse("", "voxelforge", parsed));
    CHECK(!ResourceId::parse("voxelforge:", "voxelforge", parsed));
    CHECK(!ResourceId::parse(":stone", "voxelforge", parsed));
    CHECK(!ResourceId::parse("voxelforge:with space", "voxelforge", parsed));
    CHECK(!ResourceId::parse("voxelforge://double", "voxelforge", parsed));
    CHECK(!ResourceId::parse("voxelforge:/leading", "voxelforge", parsed));

    CHECK(id("voxelforge:stone") == id("voxelforge:stone"));
    CHECK(id("voxelforge:stone") != id("other:stone"));
    CHECK(id("voxelforge:a") < id("voxelforge:b"));
}

VF_TEST(resourceIdsResolveOntoTheExistingBlockAndItemPalettes) {
    ItemId item = ItemId::None;
    CHECK(resolveItemId(id("voxelforge:iron_ingot"), item));
    CHECK(item == ItemId::IronIngot);
    CHECK(resolveItemId(id("voxelforge:crafting_table"), item));
    CHECK(item == ItemId::CraftingTable);
    CHECK(resolveItemId(id("voxelforge:stone"), item));
    CHECK(item == ItemId::Stone);
    CHECK(!resolveItemId(id("voxelforge:unobtainium"), item));

    Block block = Block::Air;
    CHECK(resolveBlockId(id("voxelforge:redstone_lamp"), block));
    CHECK(block == Block::RedstoneLamp);
    CHECK(resolveBlockId(id("voxelforge:packed_ice"), block));
    CHECK(block == Block::PackedIce);
    CHECK(!resolveBlockId(id("voxelforge:cheese"), block));
}

VF_TEST(packMetadataComesOutOfTheDataItself) {
    DataPackLoader loader;
    loader.addPack(basePack());
    CHECK(loader.load());
    CHECK_EQ(loader.errors().size(), static_cast<std::size_t>(0));
    CHECK_EQ(loader.packs().size(), static_cast<std::size_t>(1));
    const DataPackMeta& meta = loader.packs()[0].meta();
    CHECK_EQ(meta.name, std::string("base"));
    CHECK_EQ(meta.description, std::string("the shipped content"));
    CHECK_EQ(meta.formatVersion, 1);
    CHECK_EQ(meta.defaultSpace, std::string("voxelforge"));

    DataPackMeta broken;
    std::string problem;
    JsonParseResult noName = parseJson(R"json({"description": "nameless"})json");
    CHECK(!DataPackMeta::fromJson(noName.root, broken, problem));
    CHECK(problem.find("name") != std::string::npos);

    JsonParseResult zeroVersion = parseJson(R"json({"name": "x", "formatVersion": 0})json");
    CHECK(!DataPackMeta::fromJson(zeroVersion.root, broken, problem));
    CHECK(problem.find("formatVersion") != std::string::npos);

    JsonParseResult badSpace = parseJson(R"json({"name": "x", "defaultSpace": "Bad Space"})json");
    CHECK(!DataPackMeta::fromJson(badSpace.root, broken, problem));
    CHECK(problem.find("defaultSpace") != std::string::npos);
}

VF_TEST(loadedRecipesAreMatchedByTheExistingCraftingRegistry) {
    DataPackLoader loader;
    loader.addPack(basePack());
    CHECK(loader.load());
    CHECK_EQ(loader.recipes().size(), static_cast<std::size_t>(3));

    const RecipeData* torch = loader.recipe(id("voxelforge:redstone_torch"));
    CHECK(torch != nullptr);
    CHECK(torch->kind == RecipeData::Kind::Shaped);
    CHECK_EQ(torch->width, 1);
    CHECK_EQ(torch->height, 2);
    // Packed by the recipe width, which is how Crafting.cpp reads a pattern.
    CHECK(torch->pattern[0] == ItemId::RedstoneDust);
    CHECK(torch->pattern[1] == ItemId::Stick);
    CHECK(!torch->allowMirror);
    CHECK(torch->resultItem == ItemId::RedstoneTorch);

    CraftingRegistry registry;
    std::size_t shapedBefore = registry.shapedRecipes().size();
    std::size_t shapelessBefore = registry.shapelessRecipes().size();
    std::size_t smeltingBefore = registry.smeltingRecipes().size();
    CHECK_EQ(loader.applyTo(registry), 3);
    CHECK_EQ(registry.shapedRecipes().size(), shapedBefore + 1u);
    CHECK_EQ(registry.shapelessRecipes().size(), shapelessBefore + 1u);
    CHECK_EQ(registry.smeltingRecipes().size(), smeltingBefore + 1u);

    CraftGrid grid;
    grid.fill(ItemId::None);
    grid[static_cast<std::size_t>(1 * craftGridWidth + 2)] = ItemId::RedstoneDust;
    grid[static_cast<std::size_t>(2 * craftGridWidth + 2)] = ItemId::Stick;
    CraftResult crafted = registry.craftMatch(grid);
    CHECK(crafted.valid());
    CHECK(crafted.item == ItemId::RedstoneTorch);
    CHECK_EQ(crafted.count, 4);

    // The built in coal torch is untouched: loading data adds, it does not
    // shadow what the registry already knew.
    CraftGrid coalTorch;
    coalTorch.fill(ItemId::None);
    coalTorch[static_cast<std::size_t>(1 * craftGridWidth + 2)] = ItemId::CoalLump;
    coalTorch[static_cast<std::size_t>(2 * craftGridWidth + 2)] = ItemId::Stick;
    CHECK(registry.craftMatch(coalTorch).item == ItemId::Torch);

    CraftGrid shapeless;
    shapeless.fill(ItemId::None);
    shapeless[0] = ItemId::Apple;
    shapeless[4] = ItemId::Apple;
    CraftResult baked = registry.craftMatch(shapeless);
    CHECK(baked.valid());
    CHECK(baked.item == ItemId::Bread);

    const SmeltingRecipe* smelt = registry.smeltingFor(ItemId::Sand);
    CHECK(smelt != nullptr);
    CHECK(smelt->result.item == ItemId::Sandstone);
    CHECK_NEAR(smelt->smeltSeconds, 12.5f, 0.001f);
    CHECK_NEAR(loader.recipe(id("voxelforge:sandstone"))->experience, 0.7f, 0.001f);
}

VF_TEST(loadedLootTablesRollThroughTheExistingLootRoller) {
    DataPackLoader loader;
    loader.addPack(basePack());
    CHECK(loader.load());

    const LootTable* table = loader.lootTable(id("voxelforge:chests/starter"));
    CHECK(table != nullptr);
    CHECK_EQ(table->pools.size(), static_cast<std::size_t>(2));
    CHECK_EQ(table->pools[0].entries.size(), static_cast<std::size_t>(1));
    CHECK(table->pools[0].entries[0].item == ItemId::Bread);
    CHECK_EQ(table->pools[0].entries[0].minCount, 2);
    CHECK_EQ(table->pools[1].entries[0].conditions, LootEntry::PlayerKill);
    CHECK_EQ(table->pools[1].entries[0].functions, LootEntry::LootingBonus);

    LootContext context;
    context.seed = 1234u;
    LootRoll plain = loader.roll(id("voxelforge:chests/starter"), context);
    CHECK_EQ(plain.countOf(ItemId::Bread), 2);
    // The second pool is gated behind a player kill, so it stays shut here.
    CHECK_EQ(plain.countOf(ItemId::DiamondGem), 0);

    context.playerKill = true;
    LootRoll earned = loader.roll(id("voxelforge:chests/starter"), context);
    CHECK_EQ(earned.countOf(ItemId::Bread), 2);
    CHECK_EQ(earned.countOf(ItemId::DiamondGem), 1);

    // Same seed, same roll: nothing here reaches for a clock.
    LootRoll again = loader.roll(id("voxelforge:chests/starter"), context);
    CHECK_EQ(again.totalCount(), earned.totalCount());
    CHECK(loader.roll(id("voxelforge:chests/nothing"), context).empty());
}

VF_TEST(aLaterPackOverwritesAResourceAndAppendsToATag) {
    DataPackLoader loader;
    loader.addPack(basePack());
    loader.addPack(overlayPack());
    CHECK(loader.load());
    CHECK_EQ(loader.errors().size(), static_cast<std::size_t>(0));
    CHECK_EQ(loader.applyOrder().size(), static_cast<std::size_t>(2));
    CHECK_EQ(loader.applyOrder()[0], std::string("base"));
    CHECK_EQ(loader.applyOrder()[1], std::string("overlay"));

    // One recipe id, the higher pack's version of it.
    CHECK_EQ(loader.recipes().size(), static_cast<std::size_t>(3));
    CHECK_EQ(loader.recipe(id("voxelforge:redstone_torch"))->resultCount, 8);

    std::vector<Block> logs;
    CHECK(loader.blocksInTag(id("voxelforge:logs"), logs));
    CHECK_EQ(logs.size(), static_cast<std::size_t>(2));
    if (logs.size() == 2u) {
        CHECK(logs[0] == Block::Wood);
        CHECK(logs[1] == Block::Planks);
    }

    CraftingRegistry registry;
    loader.applyTo(registry);
    CraftGrid grid;
    grid.fill(ItemId::None);
    grid[0] = ItemId::RedstoneDust;
    grid[static_cast<std::size_t>(craftGridWidth)] = ItemId::Stick;
    CHECK(registry.craftMatch(grid).item == ItemId::RedstoneTorch);
    CHECK_EQ(registry.craftMatch(grid).count, 8);
}

VF_TEST(packPriorityDecidesWhoWinsRegardlessOfInsertionOrder) {
    DataPackLoader loader;
    loader.addPack(overlayPack());
    loader.addPack(basePack());
    CHECK(loader.load());
    CHECK_EQ(loader.applyOrder()[0], std::string("base"));
    CHECK_EQ(loader.applyOrder()[1], std::string("overlay"));
    CHECK_EQ(loader.recipe(id("voxelforge:redstone_torch"))->resultCount, 8);
}

VF_TEST(aTagMarkedReplaceWipesWhatCameBeforeIt) {
    DataPackMeta meta;
    meta.name = "replacement";
    meta.priority = 20;
    DataPack replacement(meta);
    replacement.addFile("data/voxelforge/tags/blocks/logs.json",
                        R"json({"replace": true, "values": ["voxelforge:cactus"]})json");

    DataPackLoader loader;
    loader.addPack(basePack());
    loader.addPack(overlayPack());
    loader.addPack(replacement);
    CHECK(loader.load());

    std::vector<Block> logs;
    CHECK(loader.blocksInTag(id("voxelforge:logs"), logs));
    CHECK_EQ(logs.size(), static_cast<std::size_t>(1));
    if (!logs.empty()) CHECK(logs[0] == Block::Cactus);

    const BlockTag* tag = loader.blockTags().find(id("voxelforge:logs"));
    CHECK(tag != nullptr);
    CHECK(tag->replace);
    CHECK(tag->contains(id("voxelforge:cactus")));
    CHECK(!tag->contains(id("voxelforge:wood")));
}

VF_TEST(tagReferencesFlattenIntoTheirMembersWithoutDuplicates) {
    DataPackLoader loader;
    loader.addPack(basePack());
    CHECK(loader.load());

    std::vector<ResourceId> members;
    CHECK(loader.blockTags().resolve(id("voxelforge:mineable"), members));
    CHECK_EQ(members.size(), static_cast<std::size_t>(3));
    if (members.size() == 3u) {
        CHECK(members[0] == id("voxelforge:wood"));
        CHECK(members[1] == id("voxelforge:stone"));
        CHECK(members[2] == id("voxelforge:sand"));
    }
    CHECK(loader.blockTags().contains(id("voxelforge:mineable"), id("voxelforge:wood")));
    CHECK(!loader.blockTags().contains(id("voxelforge:mineable"), id("voxelforge:cactus")));

    std::vector<Block> blocks;
    CHECK(loader.blocksInTag(id("voxelforge:mineable"), blocks));
    CHECK_EQ(blocks.size(), static_cast<std::size_t>(3));
    if (!blocks.empty()) CHECK(blocks[0] == Block::Wood);

    std::vector<ItemId> fuel;
    CHECK(loader.itemsInTag(id("voxelforge:fuel"), fuel));
    CHECK_EQ(fuel.size(), static_cast<std::size_t>(2));
    if (fuel.size() == 2u) {
        CHECK(fuel[0] == ItemId::CoalLump);
        CHECK(fuel[1] == ItemId::Planks);
    }

    // A tag that names a tag nobody defined resolves to nothing, loudly.
    std::vector<ResourceId> unresolved;
    std::string problem;
    TagRegistry registry;
    BlockTag dangling;
    dangling.id = id("voxelforge:dangling");
    dangling.entries.push_back({id("voxelforge:absent"), true});
    registry.define(dangling);
    CHECK(!registry.resolve(dangling.id, unresolved, problem));
    CHECK(problem.find("unknown tag") != std::string::npos);
    CHECK(!registry.hasCycle(dangling.id));
}

VF_TEST(aTagThatReferencesItselfIsReportedAsACycleInsteadOfLoopingForever) {
    DataPackMeta meta;
    meta.name = "looping";
    DataPack pack(meta);
    pack.addFile("data/voxelforge/tags/blocks/first.json",
                 R"json({"values": ["voxelforge:stone", "#voxelforge:second"]})json");
    pack.addFile("data/voxelforge/tags/blocks/second.json",
                 R"json({"values": ["voxelforge:sand", "#voxelforge:third"]})json");
    pack.addFile("data/voxelforge/tags/blocks/third.json",
                 R"json({"values": ["#voxelforge:first"]})json");
    pack.addFile("data/voxelforge/tags/blocks/selfish.json",
                 R"json({"values": ["#voxelforge:selfish"]})json");

    DataPackLoader loader;
    loader.addPack(pack);
    CHECK(loader.load());

    const TagRegistry& tags = loader.blockTags();
    std::vector<ResourceId> members;
    std::string problem;
    CHECK(!tags.resolve(id("voxelforge:first"), members, problem));
    CHECK(problem.find("tag cycle") != std::string::npos);
    CHECK(members.empty());
    CHECK(tags.hasCycle(id("voxelforge:first")));
    CHECK(tags.hasCycle(id("voxelforge:second")));
    CHECK(tags.hasCycle(id("voxelforge:selfish")));

    std::vector<Block> blocks;
    CHECK(!loader.blocksInTag(id("voxelforge:first"), blocks));
    CHECK(blocks.empty());

    // A diamond is not a cycle: two paths onto the same tag still resolve.
    DataPackMeta diamondMeta;
    diamondMeta.name = "diamond";
    DataPack diamond(diamondMeta);
    diamond.addFile("data/voxelforge/tags/blocks/leaf.json",
                    R"json({"values": ["voxelforge:stone"]})json");
    diamond.addFile("data/voxelforge/tags/blocks/left.json",
                    R"json({"values": ["#voxelforge:leaf"]})json");
    diamond.addFile("data/voxelforge/tags/blocks/right.json",
                    R"json({"values": ["#voxelforge:leaf"]})json");
    diamond.addFile("data/voxelforge/tags/blocks/top.json",
                    R"json({"values": ["#voxelforge:left", "#voxelforge:right"]})json");
    DataPackLoader diamondLoader;
    diamondLoader.addPack(diamond);
    CHECK(diamondLoader.load());
    CHECK(diamondLoader.blockTags().resolve(id("voxelforge:top"), members));
    CHECK_EQ(members.size(), static_cast<std::size_t>(1));
    CHECK(!diamondLoader.blockTags().hasCycle(id("voxelforge:top")));
}

VF_TEST(oneRottenFileCostsItselfAndNothingElseInThePack) {
    DataPackMeta meta;
    meta.name = "mixed";
    DataPack pack(meta);
    pack.addFile("data/voxelforge/recipes/good.json", appleBread);
    pack.addFile("data/voxelforge/recipes/torn.json", "{\"type\": \"craftingShapeless\",");
    pack.addFile("data/voxelforge/recipes/ghost_item.json",
                 R"json({"type": "craftingShapeless", "ingredients": ["voxelforge:cheese"],
                          "result": {"item": "voxelforge:bread"}})json");
    pack.addFile("data/voxelforge/recipes/nonsense.json",
                 R"json({"type": "alchemy", "result": {"item": "voxelforge:bread"}})json");
    pack.addFile("data/voxelforge/nowhere/lost.json", R"json({"values": []})json");
    pack.addFile("data/voxelforge/lootTables/empty.json", R"json({"pools": []})json");
    pack.addFile("data/voxelforge/tags/blocks/bad_entry.json",
                 R"json({"values": ["Not An Id"]})json");
    pack.addFile("data/voxelforge/tags/blocks/fine.json",
                 R"json({"values": ["voxelforge:stone"]})json");
    pack.addFile("data/voxelforge/recipes/ShoutyName.json", appleBread);

    DataPackLoader loader;
    loader.addPack(pack);
    CHECK(!loader.load());
    CHECK_EQ(loader.errors().size(), static_cast<std::size_t>(7));

    for (const DataPackError& failure : loader.errors()) {
        CHECK(!failure.file.empty());
        CHECK(!failure.reason.empty());
        CHECK_EQ(failure.pack, std::string("mixed"));
        CHECK(failure.text().find("mixed/data/") != std::string::npos);
    }

    // The healthy files in the very same pack still landed.
    CHECK_EQ(loader.recipes().size(), static_cast<std::size_t>(1));
    CHECK(loader.recipe(id("voxelforge:good")) != nullptr);
    CHECK_EQ(loader.blockTags().size(), static_cast<std::size_t>(1));
    CHECK(loader.blockTags().has(id("voxelforge:fine")));

    bool sawParseError = false;
    bool sawUnknownItem = false;
    bool sawUnknownFolder = false;
    bool sawUppercaseId = false;
    for (const DataPackError& failure : loader.errors()) {
        if (failure.file == "data/voxelforge/recipes/torn.json") {
            sawParseError = failure.line > 0;
        }
        if (failure.file == "data/voxelforge/recipes/ghost_item.json")
            sawUnknownItem = failure.reason.find("no such item") != std::string::npos;
        if (failure.file == "data/voxelforge/nowhere/lost.json")
            sawUnknownFolder = failure.reason.find("known folder") != std::string::npos;
        if (failure.file == "data/voxelforge/recipes/ShoutyName.json")
            sawUppercaseId = failure.reason.find("valid resource id") != std::string::npos;
    }
    CHECK(sawParseError);
    CHECK(sawUnknownItem);
    CHECK(sawUnknownFolder);
    CHECK(sawUppercaseId);
}

