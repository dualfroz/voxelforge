#include "vf/DataPack.h"

#include <algorithm>
#include <cctype>

#include "vf/Command.h"

namespace vf {

namespace {

const char* const knownFolders[] = {"recipes", "lootTables", "tags/blocks", "tags/items",
                                    "advancements"};

std::string loweredWithoutSeparators(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '_' || c == '-') continue;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool splitDataPath(const std::string& file, std::string& space, std::string& folder,
                   std::string& name) {
    const std::string root = "data/";
    if (file.compare(0, root.size(), root) != 0) return false;
    std::size_t spaceEnd = file.find('/', root.size());
    if (spaceEnd == std::string::npos) return false;
    space = file.substr(root.size(), spaceEnd - root.size());
    std::string rest = file.substr(spaceEnd + 1u);
    if (!endsWith(rest, ".json")) return false;
    rest = rest.substr(0, rest.size() - 5u);

    std::string bestFolder;
    for (const char* candidate : knownFolders) {
        std::string prefix = std::string(candidate) + "/";
        if (rest.compare(0, prefix.size(), prefix) != 0 || rest.size() <= prefix.size()) continue;
        if (bestFolder.size() < prefix.size() - 1u) bestFolder = candidate;
    }
    if (bestFolder.empty()) return false;
    folder = bestFolder;
    name = rest.substr(bestFolder.size() + 1u);
    return !name.empty();
}

bool readResourceField(const JsonValue& owner, const std::string& key,
                       const std::string& defaultSpace, ResourceId& out, std::string& problem) {
    const JsonValue* field = owner.find(key);
    if (field == nullptr || !field->isString()) {
        problem = "\"" + key + "\" must be a resource id string";
        return false;
    }
    if (!ResourceId::parse(field->stringValue(), defaultSpace, out)) {
        problem = "\"" + key + "\" is not a valid resource id: " + field->stringValue();
        return false;
    }
    return true;
}

bool readItemField(const JsonValue& owner, const std::string& key, const std::string& defaultSpace,
                   ItemId& out, std::string& problem) {
    ResourceId id;
    if (!readResourceField(owner, key, defaultSpace, id, problem)) return false;
    if (!resolveItemId(id, out)) {
        problem = "no such item: " + id.text();
        return false;
    }
    return true;
}

struct NamedFlag {
    const char* name;
    uint32_t bit;
};

const NamedFlag lootConditionFlags[] = {
    {"playerKill", LootEntry::PlayerKill},
    {"fiery", LootEntry::Fiery},
    {"notFiery", LootEntry::NotFiery},
    {"silkTouched", LootEntry::SilkTouched},
    {"notSilkTouched", LootEntry::NotSilkTouched},
    {"needsLooting", LootEntry::NeedsLooting},
    {"needsFortune", LootEntry::NeedsFortune},
    {"explosive", LootEntry::Explosive},
    {"ranged", LootEntry::Ranged},
};

const NamedFlag lootFunctionFlags[] = {
    {"countRange", LootEntry::CountRange},
    {"oreFortune", LootEntry::OreFortune},
    {"lootingBonus", LootEntry::LootingBonus},
    {"chanceLooting", LootEntry::ChanceLooting},
    {"chanceFortune", LootEntry::ChanceFortune},
    {"smeltIfFiery", LootEntry::SmeltIfFiery},
};

bool readFlagList(const JsonValue& owner, const std::string& key, const NamedFlag* table,
                  std::size_t tableSize, uint32_t& out, std::string& problem) {
    const JsonValue* list = owner.find(key);
    if (list == nullptr) return true;
    if (!list->isArray()) {
        problem = "\"" + key + "\" must be an array of names";
        return false;
    }
    for (const JsonValue& element : list->elements()) {
        if (!element.isString()) {
            problem = "\"" + key + "\" holds a non string entry";
            return false;
        }
        bool matched = false;
        for (std::size_t i = 0u; i < tableSize; ++i) {
            if (element.stringValue() == table[i].name) {
                out |= table[i].bit;
                matched = true;
                break;
            }
        }
        if (!matched) {
            problem = "unknown " + key + " name: " + element.stringValue();
            return false;
        }
    }
    return true;
}

bool readCountRange(const JsonValue& owner, const std::string& key, int& minOut, int& maxOut,
                    std::string& problem) {
    const JsonValue* field = owner.find(key);
    if (field == nullptr) return true;
    if (field->isNumber()) {
        minOut = static_cast<int>(field->intValue());
        maxOut = minOut;
    } else if (field->isObject()) {
        minOut = static_cast<int>(field->intOr("min", minOut));
        maxOut = static_cast<int>(field->intOr("max", minOut));
    } else {
        problem = "\"" + key + "\" must be a number or a {min,max} object";
        return false;
    }
    if (minOut > maxOut) {
        problem = "\"" + key + "\" has min above max";
        return false;
    }
    if (minOut < 0) {
        problem = "\"" + key + "\" cannot be negative";
        return false;
    }
    return true;
}

} // namespace

ResourceId::ResourceId(std::string space, std::string path)
    : space(std::move(space)), path(std::move(path)) {}

bool ResourceId::validSpace(const std::string& text) {
    if (text.empty()) return false;
    for (char c : text) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                  c == '.';
        if (!ok) return false;
    }
    return true;
}

bool ResourceId::validPath(const std::string& text) {
    if (text.empty()) return false;
    if (text.front() == '/' || text.back() == '/') return false;
    for (char c : text) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                  c == '.' || c == '/';
        if (!ok) return false;
    }
    return text.find("//") == std::string::npos;
}

bool ResourceId::parse(const std::string& text, const std::string& defaultSpace, ResourceId& out) {
    std::size_t colon = text.find(':');
    if (colon == std::string::npos) {
        out = ResourceId(defaultSpace, text);
    } else {
        if (text.find(':', colon + 1u) != std::string::npos) return false;
        out = ResourceId(text.substr(0, colon), text.substr(colon + 1u));
    }
    return out.valid();
}

bool resolveItemId(const ResourceId& id, ItemId& out) {
    if (!id.valid()) return false;
    return parseItemName(loweredWithoutSeparators(id.path), out);
}

bool resolveBlockId(const ResourceId& id, Block& out) {
    if (!id.valid()) return false;
    return parseBlockName(loweredWithoutSeparators(id.path), out);
}

bool DataPackMeta::fromJson(const JsonValue& root, DataPackMeta& out, std::string& problem) {
    if (!root.isObject()) {
        problem = "pack metadata must be an object";
        return false;
    }
    const JsonValue* name = root.find("name");
    if (name == nullptr || !name->isString() || name->stringValue().empty()) {
        problem = "\"name\" must be a non empty string";
        return false;
    }
    DataPackMeta parsed;
    parsed.name = name->stringValue();
    parsed.description = root.stringOr("description", "");
    parsed.formatVersion = static_cast<int>(root.intOr("formatVersion", 1));
    parsed.priority = static_cast<int>(root.intOr("priority", 0));
    parsed.defaultSpace = root.stringOr("defaultSpace", "voxelforge");
    if (parsed.formatVersion < 1) {
        problem = "\"formatVersion\" must be at least 1";
        return false;
    }
    if (!ResourceId::validSpace(parsed.defaultSpace)) {
        problem = "\"defaultSpace\" is not a valid namespace: " + parsed.defaultSpace;
        return false;
    }
    out = parsed;
    return true;
}

std::string DataPackError::text() const {
    std::string out = pack.empty() ? file : pack + "/" + file;
    if (line > 0) out += ":" + std::to_string(line) + ":" + std::to_string(column);
    return out + ": " + reason;
}

void DataPack::addFile(const std::string& path, std::string contents) {
    files[path] = std::move(contents);
}

bool DataPack::removeFile(const std::string& path) { return files.erase(path) > 0u; }

bool DataPack::hasFile(const std::string& path) const { return files.count(path) > 0u; }

const std::string* DataPack::fileContents(const std::string& path) const {
    auto found = files.find(path);
    return found == files.end() ? nullptr : &found->second;
}

std::vector<std::string> DataPack::filesUnder(const std::string& prefix) const {
    std::vector<std::string> out;
    for (const auto& entry : files)
        if (entry.first.compare(0, prefix.size(), prefix) == 0) out.push_back(entry.first);
    return out;
}

std::vector<std::string> DataPack::allFiles() const {
    std::vector<std::string> out;
    out.reserve(files.size());
    for (const auto& entry : files) out.push_back(entry.first);
    return out;
}

ShapedRecipe RecipeData::toShaped() const {
    ShapedRecipe out;
    out.width = width;
    out.height = height;
    out.pattern = pattern;
    out.allowMirror = allowMirror;
    out.result.item = resultItem;
    out.result.count = resultCount;
    return out;
}

ShapelessRecipe RecipeData::toShapeless() const {
    ShapelessRecipe out;
    out.ingredients = ingredients;
    out.result.item = resultItem;
    out.result.count = resultCount;
    return out;
}

SmeltingRecipe RecipeData::toSmelting() const {
    SmeltingRecipe out;
    out.input = smeltInput;
    out.smeltSeconds = smeltSeconds;
    out.result.item = resultItem;
    out.result.count = resultCount;
    return out;
}

bool BlockTag::contains(const ResourceId& member) const {
    for (const Entry& entry : entries)
        if (!entry.reference && entry.id == member) return true;
    return false;
}

void TagRegistry::define(const BlockTag& tag) {
    auto found = tags.find(tag.id);
    if (found == tags.end() || tag.replace) {
        tags[tag.id] = tag;
        return;
    }
    BlockTag& existing = found->second;
    for (const BlockTag::Entry& entry : tag.entries) {
        bool already = false;
        for (const BlockTag::Entry& present : existing.entries)
            if (present == entry) already = true;
        if (!already) existing.entries.push_back(entry);
    }
}

void TagRegistry::clear() { tags.clear(); }

bool TagRegistry::has(const ResourceId& id) const { return tags.count(id) > 0u; }

const BlockTag* TagRegistry::find(const ResourceId& id) const {
    auto found = tags.find(id);
    return found == tags.end() ? nullptr : &found->second;
}

std::vector<ResourceId> TagRegistry::ids() const {
    std::vector<ResourceId> out;
    out.reserve(tags.size());
    for (const auto& entry : tags) out.push_back(entry.first);
    return out;
}

namespace {

bool expandTag(const TagRegistry& registry, const ResourceId& id, std::vector<ResourceId>& out,
               std::vector<ResourceId>& path, std::vector<ResourceId>& expanded,
               std::string& problem) {
    for (const ResourceId& onPath : path) {
        if (onPath == id) {
            problem = "tag cycle through " + id.text();
            return false;
        }
    }
    for (const ResourceId& done : expanded)
        if (done == id) return true;

    const BlockTag* tag = registry.find(id);
    if (tag == nullptr) {
        problem = "unknown tag " + id.text();
        return false;
    }
    path.push_back(id);
    for (const BlockTag::Entry& entry : tag->entries) {
        if (entry.reference) {
            if (!expandTag(registry, entry.id, out, path, expanded, problem)) return false;
            continue;
        }
        bool already = false;
        for (const ResourceId& present : out)
            if (present == entry.id) already = true;
        if (!already) out.push_back(entry.id);
    }
    path.pop_back();
    expanded.push_back(id);
    return true;
}

} // namespace

bool TagRegistry::resolve(const ResourceId& id, std::vector<ResourceId>& out,
                          std::string& problem) const {
    out.clear();
    std::vector<ResourceId> path;
    std::vector<ResourceId> expanded;
    if (!expandTag(*this, id, out, path, expanded, problem)) {
        out.clear();
        return false;
    }
    return true;
}

bool TagRegistry::resolve(const ResourceId& id, std::vector<ResourceId>& out) const {
    std::string ignored;
    return resolve(id, out, ignored);
}

bool TagRegistry::contains(const ResourceId& id, const ResourceId& member) const {
    std::vector<ResourceId> members;
    if (!resolve(id, members)) return false;
    for (const ResourceId& present : members)
        if (present == member) return true;
    return false;
}

bool TagRegistry::hasCycle(const ResourceId& id) const {
    std::vector<ResourceId> members;
    std::string problem;
    if (resolve(id, members, problem)) return false;
    return problem.compare(0, 10, "tag cycle ") == 0;
}

bool parseRecipeJson(const JsonValue& root, const std::string& defaultSpace, RecipeData& out,
                     std::string& problem) {
    if (!root.isObject()) {
        problem = "a recipe must be an object";
        return false;
    }
    const JsonValue* type = root.find("type");
    if (type == nullptr || !type->isString()) {
        problem = "\"type\" must be one of craftingShaped, craftingShapeless, smelting";
        return false;
    }
    const JsonValue* result = root.find("result");
    if (result == nullptr || !result->isObject()) {
        problem = "\"result\" must be an object";
        return false;
    }
    RecipeData parsed;
    if (!readItemField(*result, "item", defaultSpace, parsed.resultItem, problem)) return false;
    parsed.resultCount = static_cast<int>(result->intOr("count", 1));
    if (parsed.resultCount < 1 || parsed.resultCount > maxStackSize(parsed.resultItem)) {
        problem = "result count out of range for " + result->stringOr("item", "");
        return false;
    }

    const std::string& kindName = type->stringValue();
    if (kindName == "craftingShaped") {
        parsed.kind = RecipeData::Kind::Shaped;
        const JsonValue* pattern = root.find("pattern");
        const JsonValue* key = root.find("key");
        if (pattern == nullptr || !pattern->isArray() || pattern->elements().empty()) {
            problem = "\"pattern\" must be a non empty array of rows";
            return false;
        }
        if (key == nullptr || !key->isObject()) {
            problem = "\"key\" must be an object mapping pattern characters to items";
            return false;
        }
        if (pattern->elements().size() > static_cast<std::size_t>(craftGridHeight)) {
            problem = "\"pattern\" is taller than the crafting grid";
            return false;
        }
        parsed.height = static_cast<int>(pattern->elements().size());
        parsed.width = -1;
        parsed.pattern.fill(ItemId::None);
        for (int row = 0; row < parsed.height; ++row) {
            const JsonValue& line = pattern->elements()[static_cast<std::size_t>(row)];
            if (!line.isString()) {
                problem = "\"pattern\" rows must be strings";
                return false;
            }
            const std::string& cells = line.stringValue();
            if (cells.empty() || cells.size() > static_cast<std::size_t>(craftGridWidth)) {
                problem = "\"pattern\" row width must be 1 to 3 characters";
                return false;
            }
            if (parsed.width < 0) parsed.width = static_cast<int>(cells.size());
            if (parsed.width != static_cast<int>(cells.size())) {
                problem = "\"pattern\" rows are not all the same width";
                return false;
            }
            for (int column = 0; column < parsed.width; ++column) {
                char symbol = cells[static_cast<std::size_t>(column)];
                if (symbol == ' ') continue;
                ItemId ingredient = ItemId::None;
                if (!readItemField(*key, std::string(1, symbol), defaultSpace, ingredient,
                                   problem)) {
                    problem = "pattern character '" + std::string(1, symbol) + "': " + problem;
                    return false;
                }
                // Packed by the recipe's own width, which is how matchesShaped
                // in Crafting.cpp indexes a pattern.
                parsed.pattern[static_cast<std::size_t>(row * parsed.width + column)] = ingredient;
            }
        }
        parsed.allowMirror = root.boolOr("mirror", true);
    } else if (kindName == "craftingShapeless") {
        parsed.kind = RecipeData::Kind::Shapeless;
        const JsonValue* ingredients = root.find("ingredients");
        if (ingredients == nullptr || !ingredients->isArray() || ingredients->elements().empty()) {
            problem = "\"ingredients\" must be a non empty array";
            return false;
        }
        if (ingredients->elements().size() > static_cast<std::size_t>(craftGridCells)) {
            problem = "\"ingredients\" holds more than the grid can take";
            return false;
        }
        for (const JsonValue& element : ingredients->elements()) {
            if (!element.isString()) {
                problem = "\"ingredients\" holds a non string entry";
                return false;
            }
            ResourceId id;
            ItemId ingredient = ItemId::None;
            if (!ResourceId::parse(element.stringValue(), defaultSpace, id) ||
                !resolveItemId(id, ingredient)) {
                problem = "no such item: " + element.stringValue();
                return false;
            }
            parsed.ingredients.push_back(ingredient);
        }
    } else if (kindName == "smelting") {
        parsed.kind = RecipeData::Kind::Smelting;
        if (!readItemField(root, "ingredient", defaultSpace, parsed.smeltInput, problem))
            return false;
        parsed.smeltSeconds = static_cast<float>(root.numberOr("seconds", 10.0));
        parsed.experience = static_cast<float>(root.numberOr("experience", 0.0));
        if (parsed.smeltSeconds <= 0.0f) {
            problem = "\"seconds\" must be above zero";
            return false;
        }
        if (parsed.experience < 0.0f) {
            problem = "\"experience\" cannot be negative";
            return false;
        }
    } else {
        problem = "unknown recipe type: " + kindName;
        return false;
    }
    out = parsed;
    return true;
}

bool parseLootTableJson(const JsonValue& root, const std::string& defaultSpace, LootTable& out,
                        std::string& problem) {
    if (!root.isObject()) {
        problem = "a loot table must be an object";
        return false;
    }
    const JsonValue* pools = root.find("pools");
    if (pools == nullptr || !pools->isArray() || pools->elements().empty()) {
        problem = "\"pools\" must be a non empty array";
        return false;
    }
    LootTable parsed;
    for (const JsonValue& poolJson : pools->elements()) {
        if (!poolJson.isObject()) {
            problem = "every pool must be an object";
            return false;
        }
        LootPool pool;
        if (!readCountRange(poolJson, "rolls", pool.minRolls, pool.maxRolls, problem)) return false;
        pool.bonusRollsPerLooting = static_cast<int>(poolJson.intOr("bonusRollsPerLooting", 0));
        if (pool.bonusRollsPerLooting < 0) {
            problem = "\"bonusRollsPerLooting\" cannot be negative";
            return false;
        }
        const JsonValue* entries = poolJson.find("entries");
        if (entries == nullptr || !entries->isArray() || entries->elements().empty()) {
            problem = "every pool needs a non empty \"entries\" array";
            return false;
        }
        for (const JsonValue& entryJson : entries->elements()) {
            if (!entryJson.isObject()) {
                problem = "every loot entry must be an object";
                return false;
            }
            LootEntry entry;
            if (!readItemField(entryJson, "item", defaultSpace, entry.item, problem)) return false;
            entry.weight = static_cast<int>(entryJson.intOr("weight", 1));
            if (entry.weight < 1) {
                problem = "\"weight\" must be at least 1";
                return false;
            }
            if (!readCountRange(entryJson, "count", entry.minCount, entry.maxCount, problem))
                return false;
            // An authored range is meant to be rolled; the roller only does that
            // when the CountRange function is on the entry.
            if (entry.maxCount > entry.minCount) entry.functions |= LootEntry::CountRange;
            entry.chance = static_cast<float>(entryJson.numberOr("chance", 1.0));
            entry.chancePerLevel = static_cast<float>(entryJson.numberOr("chancePerLevel", 0.0));
            if (entry.chance < 0.0f || entry.chance > 1.0f) {
                problem = "\"chance\" must sit between 0 and 1";
                return false;
            }
            if (!readFlagList(entryJson, "conditions", lootConditionFlags,
                              sizeof(lootConditionFlags) / sizeof(lootConditionFlags[0]),
                              entry.conditions, problem))
                return false;
            if (!readFlagList(entryJson, "functions", lootFunctionFlags,
                              sizeof(lootFunctionFlags) / sizeof(lootFunctionFlags[0]),
                              entry.functions, problem))
                return false;
            pool.entries.push_back(entry);
        }
        parsed.pools.push_back(pool);
    }
    out = parsed;
    return true;
}

bool parseTagJson(const JsonValue& root, const std::string& defaultSpace, BlockTag& out,
                  std::string& problem) {
    if (!root.isObject()) {
        problem = "a tag must be an object";
        return false;
    }
    const JsonValue* values = root.find("values");
    if (values == nullptr || !values->isArray()) {
        problem = "\"values\" must be an array";
        return false;
    }
    BlockTag parsed;
    parsed.replace = root.boolOr("replace", false);
    for (const JsonValue& element : values->elements()) {
        if (!element.isString()) {
            problem = "\"values\" holds a non string entry";
            return false;
        }
        BlockTag::Entry entry;
        std::string text = element.stringValue();
        if (!text.empty() && text[0] == '#') {
            entry.reference = true;
            text = text.substr(1);
        }
        if (!ResourceId::parse(text, defaultSpace, entry.id)) {
            problem = "not a valid resource id: " + element.stringValue();
            return false;
        }
        parsed.entries.push_back(entry);
    }
    out = parsed;
    return true;
}

void DataPackLoader::addPack(DataPack pack) { loaded.push_back(std::move(pack)); }

void DataPackLoader::clear() {
    loaded.clear();
    problems.clear();
    order.clear();
    folders.clear();
    recipeSet.clear();
    lootSet.clear();
    blockTagSet.clear();
    itemTagSet.clear();
}

void DataPackLoader::reportError(const std::string& pack, const std::string& file,
                                 const std::string& reason) {
    DataPackError failure;
    failure.pack = pack;
    failure.file = file;
    failure.reason = reason;
    problems.push_back(failure);
}

bool DataPackLoader::load() {
    problems.clear();
    order.clear();
    folders.clear();
    recipeSet.clear();
    lootSet.clear();
    blockTagSet.clear();
    itemTagSet.clear();

    std::vector<std::size_t> sequence(loaded.size());
    for (std::size_t i = 0u; i < loaded.size(); ++i) sequence[i] = i;
    std::stable_sort(sequence.begin(), sequence.end(), [this](std::size_t a, std::size_t b) {
        return loaded[a].meta().priority < loaded[b].meta().priority;
    });

    for (std::size_t index : sequence) {
        loadPack(loaded[index]);
        order.push_back(loaded[index].meta().name);
    }
    return problems.empty();
}

void DataPackLoader::loadPack(DataPack& pack) {
    if (const std::string* metaText = pack.fileContents("pack.json")) {
        JsonParseResult parsed = parseJson(*metaText);
        if (!parsed.ok) {
            DataPackError failure;
            failure.pack = pack.meta().name;
            failure.file = "pack.json";
            failure.reason = parsed.error.message;
            failure.line = parsed.error.line;
            failure.column = parsed.error.column;
            problems.push_back(failure);
        } else {
            DataPackMeta meta;
            std::string problem;
            if (DataPackMeta::fromJson(parsed.root, meta, problem)) {
                int keptPriority = pack.meta().priority;
                pack.meta() = meta;
                if (parsed.root.find("priority") == nullptr) pack.meta().priority = keptPriority;
            } else {
                reportError(pack.meta().name, "pack.json", problem);
            }
        }
    }

    for (const std::string& file : pack.filesUnder("data/")) {
        if (!endsWith(file, ".json")) continue;
        std::string space;
        std::string folder;
        std::string name;
        if (!splitDataPath(file, space, folder, name)) {
            reportError(pack.meta().name, file,
                        "path does not match data/<space>/<folder>/<name>.json with a known folder");
            continue;
        }
        ResourceId id(space, name);
        if (!id.valid()) {
            reportError(pack.meta().name, file, "not a valid resource id: " + id.text());
            continue;
        }
        JsonParseResult parsed = parseJson(*pack.fileContents(file));
        if (!parsed.ok) {
            DataPackError failure;
            failure.pack = pack.meta().name;
            failure.file = file;
            failure.reason = parsed.error.message;
            failure.line = parsed.error.line;
            failure.column = parsed.error.column;
            problems.push_back(failure);
            continue;
        }
        ingest(pack, file, folder, id, parsed.root);
    }
}

void DataPackLoader::ingest(DataPack& pack, const std::string& file, const std::string& folder,
                            const ResourceId& id, const JsonValue& root) {
    folders[folder][id] = root;
    const std::string& defaultSpace = pack.meta().defaultSpace;
    std::string problem;
    if (folder == "recipes") {
        RecipeData recipe;
        if (!parseRecipeJson(root, defaultSpace, recipe, problem)) {
            reportError(pack.meta().name, file, problem);
            return;
        }
        recipe.id = id;
        recipeSet[id] = recipe;
        return;
    }
    if (folder == "lootTables") {
        LootTable table;
        if (!parseLootTableJson(root, defaultSpace, table, problem)) {
            reportError(pack.meta().name, file, problem);
            return;
        }
        lootSet[id] = table;
        return;
    }
    if (folder == "tags/blocks" || folder == "tags/items") {
        BlockTag tag;
        if (!parseTagJson(root, defaultSpace, tag, problem)) {
            reportError(pack.meta().name, file, problem);
            return;
        }
        tag.id = id;
        if (folder == "tags/blocks") {
            blockTagSet.define(tag);
        } else {
            itemTagSet.define(tag);
        }
    }
}

std::vector<std::string> DataPackLoader::applyOrder() const { return order; }

const RecipeData* DataPackLoader::recipe(const ResourceId& id) const {
    auto found = recipeSet.find(id);
    return found == recipeSet.end() ? nullptr : &found->second;
}

const LootTable* DataPackLoader::lootTable(const ResourceId& id) const {
    auto found = lootSet.find(id);
    return found == lootSet.end() ? nullptr : &found->second;
}

const std::map<ResourceId, JsonValue>* DataPackLoader::documentsIn(const std::string& folder) const {
    auto found = folders.find(folder);
    return found == folders.end() ? nullptr : &found->second;
}

int DataPackLoader::applyTo(CraftingRegistry& registry) const {
    int added = 0;
    for (const auto& entry : recipeSet) {
        const RecipeData& recipe = entry.second;
        switch (recipe.kind) {
            case RecipeData::Kind::Shaped: registry.addShaped(recipe.toShaped()); break;
            case RecipeData::Kind::Shapeless: registry.addShapeless(recipe.toShapeless()); break;
            case RecipeData::Kind::Smelting: registry.addSmelting(recipe.toSmelting()); break;
        }
        ++added;
    }
    return added;
}

LootRoll DataPackLoader::roll(const ResourceId& table, const LootContext& context) const {
    const LootTable* found = lootTable(table);
    if (found == nullptr) return LootRoll();
    return rollLootTable(*found, context);
}

bool DataPackLoader::blocksInTag(const ResourceId& tag, std::vector<Block>& out) const {
    out.clear();
    std::vector<ResourceId> members;
    if (!blockTagSet.resolve(tag, members)) return false;
    for (const ResourceId& member : members) {
        Block block = Block::Air;
        if (resolveBlockId(member, block)) out.push_back(block);
    }
    return true;
}

bool DataPackLoader::itemsInTag(const ResourceId& tag, std::vector<ItemId>& out) const {
    out.clear();
    std::vector<ResourceId> members;
    if (!itemTagSet.resolve(tag, members)) return false;
    for (const ResourceId& member : members) {
        ItemId item = ItemId::None;
        if (resolveItemId(member, item)) out.push_back(item);
    }
    return true;
}

} // namespace vf
