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

