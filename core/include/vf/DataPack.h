#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Crafting.h"
#include "Item.h"
#include "Json.h"
#include "Loot.h"

namespace vf {

struct ResourceId {
    std::string space;
    std::string path;

    ResourceId() = default;
    ResourceId(std::string space, std::string path);

    static bool parse(const std::string& text, const std::string& defaultSpace, ResourceId& out);
    static bool validSpace(const std::string& text);
    static bool validPath(const std::string& text);

    bool valid() const { return validSpace(space) && validPath(path); }
    bool empty() const { return space.empty() && path.empty(); }
    std::string text() const { return space + ":" + path; }

    bool operator==(const ResourceId& other) const {
        return space == other.space && path == other.path;
    }
    bool operator!=(const ResourceId& other) const { return !(*this == other); }
    bool operator<(const ResourceId& other) const {
        return space == other.space ? path < other.path : space < other.space;
    }
};

bool resolveItemId(const ResourceId& id, ItemId& out);
bool resolveBlockId(const ResourceId& id, Block& out);

struct DataPackMeta {
    std::string name;
    std::string description;
    int formatVersion = 1;
    int priority = 0;
    std::string defaultSpace = "voxelforge";

    static bool fromJson(const JsonValue& root, DataPackMeta& out, std::string& problem);
};

struct DataPackError {
    std::string pack;
    std::string file;
    std::string reason;
    int line = 0;
    int column = 0;

    std::string text() const;
};

class DataPack {
public:
    DataPack() = default;
    explicit DataPack(DataPackMeta meta) : packMeta(std::move(meta)) {}

    const DataPackMeta& meta() const { return packMeta; }
    DataPackMeta& meta() { return packMeta; }

    void addFile(const std::string& path, std::string contents);
    bool removeFile(const std::string& path);
    bool hasFile(const std::string& path) const;
    const std::string* fileContents(const std::string& path) const;

    std::vector<std::string> filesUnder(const std::string& prefix) const;
    std::vector<std::string> allFiles() const;
    std::size_t fileCount() const { return files.size(); }

private:
    DataPackMeta packMeta;
    std::map<std::string, std::string> files;
};

struct RecipeData {
    enum class Kind : uint8_t { Shaped, Shapeless, Smelting };

    ResourceId id;
    Kind kind = Kind::Shaped;

    int width = 0;
    int height = 0;
    std::array<ItemId, craftGridCells> pattern{};
    bool allowMirror = true;

    std::vector<ItemId> ingredients;

    ItemId smeltInput = ItemId::None;
    float smeltSeconds = 10.0f;
    float experience = 0.0f;

    ItemId resultItem = ItemId::None;
    int resultCount = 1;

    ShapedRecipe toShaped() const;
    ShapelessRecipe toShapeless() const;
    SmeltingRecipe toSmelting() const;
};

struct BlockTag {
    struct Entry {
        ResourceId id;
        bool reference = false;

        bool operator==(const Entry& other) const {
            return id == other.id && reference == other.reference;
        }
    };

    ResourceId id;
    bool replace = false;
    std::vector<Entry> entries;

    bool contains(const ResourceId& member) const;
};

class TagRegistry {
public:
    void define(const BlockTag& tag);
    void clear();

    bool has(const ResourceId& id) const;
    const BlockTag* find(const ResourceId& id) const;
    std::vector<ResourceId> ids() const;
    std::size_t size() const { return tags.size(); }

    bool resolve(const ResourceId& id, std::vector<ResourceId>& out) const;
    bool resolve(const ResourceId& id, std::vector<ResourceId>& out, std::string& problem) const;

    bool contains(const ResourceId& id, const ResourceId& member) const;
    bool hasCycle(const ResourceId& id) const;

private:
    std::map<ResourceId, BlockTag> tags;
};

class DataPackLoader {
public:
    void addPack(DataPack pack);
    void clear();

    bool load();

    const std::vector<DataPackError>& errors() const { return problems; }
    const std::vector<DataPack>& packs() const { return loaded; }
    std::vector<std::string> applyOrder() const;

    const std::map<ResourceId, RecipeData>& recipes() const { return recipeSet; }
    const std::map<ResourceId, LootTable>& lootTables() const { return lootSet; }
    const TagRegistry& blockTags() const { return blockTagSet; }
    const TagRegistry& itemTags() const { return itemTagSet; }

    const RecipeData* recipe(const ResourceId& id) const;
    const LootTable* lootTable(const ResourceId& id) const;

    const std::map<ResourceId, JsonValue>* documentsIn(const std::string& folder) const;

    int applyTo(CraftingRegistry& registry) const;

    LootRoll roll(const ResourceId& table, const LootContext& context) const;

    bool blocksInTag(const ResourceId& tag, std::vector<Block>& out) const;
    bool itemsInTag(const ResourceId& tag, std::vector<ItemId>& out) const;

    void reportError(const std::string& pack, const std::string& file, const std::string& reason);

private:
    void loadPack(DataPack& pack);
    void ingest(DataPack& pack, const std::string& file, const std::string& folder,
                const ResourceId& id, const JsonValue& root);

    std::vector<DataPack> loaded;
    std::vector<DataPackError> problems;
    std::vector<std::string> order;
    std::map<std::string, std::map<ResourceId, JsonValue>> folders;
    std::map<ResourceId, RecipeData> recipeSet;
    std::map<ResourceId, LootTable> lootSet;
    TagRegistry blockTagSet;
    TagRegistry itemTagSet;
};

bool parseRecipeJson(const JsonValue& root, const std::string& defaultSpace, RecipeData& out,
                     std::string& problem);
bool parseLootTableJson(const JsonValue& root, const std::string& defaultSpace, LootTable& out,
                        std::string& problem);
bool parseTagJson(const JsonValue& root, const std::string& defaultSpace, BlockTag& out,
                  std::string& problem);

}
