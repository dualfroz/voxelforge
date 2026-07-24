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

