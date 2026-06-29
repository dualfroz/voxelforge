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

// A namespaced identifier, "voxelforge:iron_ingot". Both halves are lowercase
// by construction: the space accepts [a-z0-9_.-] and the path additionally
// accepts '/', so one namespace can hold folders of resources.
struct ResourceId {
    std::string space;
    std::string path;

    ResourceId() = default;
    ResourceId(std::string space, std::string path);

    // Accepts "space:path" or a bare "path", which takes defaultSpace.
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

// Maps a resource id onto the palettes the engine already has. The path is
// matched case insensitively with '_' and '-' removed, so "iron_ingot" finds
// ItemId::IronIngot through the shared name table in Command.cpp.
bool resolveItemId(const ResourceId& id, ItemId& out);
bool resolveBlockId(const ResourceId& id, Block& out);

struct DataPackMeta {
    std::string name;
    std::string description;
    int formatVersion = 1;
    // Higher priority is applied later, so it overwrites what came before.
    int priority = 0;
    std::string defaultSpace = "voxelforge";

    static bool fromJson(const JsonValue& root, DataPackMeta& out, std::string& problem);
};

// One thing that went wrong while loading, always naming the file it came from.
// A pack keeps loading past its own errors: one broken recipe costs that recipe
// and nothing else.
struct DataPackError {
    std::string pack;
    std::string file;
    std::string reason;
    int line = 0;
    int column = 0;

    std::string text() const;
};

// A pack as a directory tree held in memory: a map from a slash separated path
// to the file's bytes. Nothing here ever touches a real filesystem.
//
// Layout, all below data/<space>/:
//   recipes/<name>.json      shaped, shapeless or smelting
//   lootTables/<name>.json   pools of entries, conditions and functions
//   tags/blocks/<name>.json  block tag, entries may reference other tags
//   tags/items/<name>.json   item tag
//   advancements/<name>.json read by Advancement.h
// plus an optional pack.json at the root carrying the metadata.
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

    // Sorted, so loading order never depends on hashing.
    std::vector<std::string> filesUnder(const std::string& prefix) const;
    std::vector<std::string> allFiles() const;
    std::size_t fileCount() const { return files.size(); }

private:
    DataPackMeta packMeta;
    std::map<std::string, std::string> files;
};

// A recipe read from data, in the shape the existing CraftingRegistry takes.
// The conversions below hand back the very structs Crafting.h declares, so a
// loaded recipe is matched by the same craftMatch as a built in one.
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

// A tag: a named set of resource ids that may itself name other tags. The same
// type carries block tags and item tags; which palette an entry resolves
// against is decided by the registry it was loaded into.
struct BlockTag {
    struct Entry {
        ResourceId id;
        // Written as "#space:path" in data: pull in every member of that tag.
        bool reference = false;

        bool operator==(const Entry& other) const {
            return id == other.id && reference == other.reference;
        }
    };

    ResourceId id;
    // false appends to whatever an earlier pack defined, true wipes it first.
    bool replace = false;
    std::vector<Entry> entries;

    bool contains(const ResourceId& member) const;
};

// Tags by id, with references between them flattened on demand.
class TagRegistry {
public:
    // Applies replace/append semantics against what is already registered.
    void define(const BlockTag& tag);
    void clear();

    bool has(const ResourceId& id) const;
    const BlockTag* find(const ResourceId& id) const;
    std::vector<ResourceId> ids() const;
    std::size_t size() const { return tags.size(); }

    // Flattens nested references into plain members, in declaration order and
    // without duplicates. False when a reference cycles or names an unknown
    // tag; `problem` then says which.
    bool resolve(const ResourceId& id, std::vector<ResourceId>& out) const;
    bool resolve(const ResourceId& id, std::vector<ResourceId>& out, std::string& problem) const;

    // True when the flattened tag holds `member`.
    bool contains(const ResourceId& id, const ResourceId& member) const;
    // True when following references from `id` comes back to a tag already on
    // the path.
    bool hasCycle(const ResourceId& id) const;

private:
    std::map<ResourceId, BlockTag> tags;
};

// Loads any number of packs into one merged view. Packs are applied in order of
// meta().priority, insertion order breaking ties; a later pack overwrites a
// resource an earlier one defined, except for tags, which append unless the
// later tag sets "replace".
class DataPackLoader {
public:
    void addPack(DataPack pack);
    void clear();

    // Parses every pack.json and every data/**/**.json, then interprets the
    // known folders. Returns true when nothing was rejected; either way the
    // resources that did load are available.
    bool load();

    const std::vector<DataPackError>& errors() const { return problems; }
    const std::vector<DataPack>& packs() const { return loaded; }
    // Pack names in the order they were applied.
    std::vector<std::string> applyOrder() const;

    const std::map<ResourceId, RecipeData>& recipes() const { return recipeSet; }
    const std::map<ResourceId, LootTable>& lootTables() const { return lootSet; }
    const TagRegistry& blockTags() const { return blockTagSet; }
    const TagRegistry& itemTags() const { return itemTagSet; }

    const RecipeData* recipe(const ResourceId& id) const;
    const LootTable* lootTable(const ResourceId& id) const;

    // Every parsed document that lived under data/<space>/<folder>/, keyed by
    // resource id. Advancement.h reads its own folder back out of here.
    const std::map<ResourceId, JsonValue>* documentsIn(const std::string& folder) const;

    // Registers every loaded recipe with an existing registry, on top of the
    // built in ones. Returns how many were added.
    int applyTo(CraftingRegistry& registry) const;

    // Rolls a loaded table through the existing loot roller.
    LootRoll roll(const ResourceId& table, const LootContext& context) const;

    // Flattened tag members resolved against the block and item palettes.
    // Members that name nothing in the palette are dropped.
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

// Parsers for the individual document kinds, exposed so a caller can read one
// document without building a pack around it. `problem` is filled on failure.
bool parseRecipeJson(const JsonValue& root, const std::string& defaultSpace, RecipeData& out,
                     std::string& problem);
bool parseLootTableJson(const JsonValue& root, const std::string& defaultSpace, LootTable& out,
                        std::string& problem);
bool parseTagJson(const JsonValue& root, const std::string& defaultSpace, BlockTag& out,
                  std::string& problem);

} // namespace vf
