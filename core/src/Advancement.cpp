#include "vf/Advancement.h"

#include <algorithm>

#include "vf/Command.h"
#include "vf/PotionCatalog.h"

namespace vf {

namespace {

struct NamedDimension {
    const char* name;
    DimensionId id;
};

// Kept in step with DimensionConfig::nameOf, which Dimension.cpp cannot hand
// back a parser for without dragging every world generator into the link.
const NamedDimension dimensionNames[] = {
    {"overworld", DimensionId::Overworld},
    {"nether", DimensionId::Nether},
    {"end", DimensionId::End},
};

bool parseDimensionName(const std::string& text, DimensionId& out) {
    for (const NamedDimension& entry : dimensionNames) {
        if (text == entry.name) {
            out = entry.id;
            return true;
        }
    }
    return false;
}

bool parsePotionName(const std::string& text, PotionKind& out) {
    const PotionCatalog& catalog = potionCatalog();
    for (const PotionEntry& entry : catalog.entries()) {
        if (text == entry.name) {
            out = entry.kind;
            return true;
        }
    }
    return false;
}

bool parseProfessionName(const std::string& text, VillagerProfession& out) {
    for (int i = 0; i < static_cast<int>(VillagerProfession::Count); ++i) {
        VillagerProfession profession = static_cast<VillagerProfession>(i);
        if (text == villagerProfessionName(profession)) {
            out = profession;
            return true;
        }
    }
    return false;
}

bool readOptionalResource(const JsonValue& owner, const std::string& key,
                          const std::string& defaultSpace, ResourceId& out, bool& present,
                          std::string& problem) {
    const JsonValue* field = owner.find(key);
    present = false;
    if (field == nullptr) return true;
    if (!field->isString()) {
        problem = "\"" + key + "\" must be a string";
        return false;
    }
    if (!ResourceId::parse(field->stringValue(), defaultSpace, out)) {
        problem = "\"" + key + "\" is not a valid resource id: " + field->stringValue();
        return false;
    }
    present = true;
    return true;
}

bool readResourceList(const JsonValue& owner, const std::string& key,
                      const std::string& defaultSpace, std::vector<ResourceId>& out,
                      std::string& problem) {
    const JsonValue* list = owner.find(key);
    if (list == nullptr) return true;
    if (!list->isArray()) {
        problem = "\"" + key + "\" must be an array of resource ids";
        return false;
    }
    for (const JsonValue& element : list->elements()) {
        ResourceId id;
        if (!element.isString() || !ResourceId::parse(element.stringValue(), defaultSpace, id)) {
            problem = "\"" + key + "\" holds something that is not a resource id";
            return false;
        }
        out.push_back(id);
    }
    return true;
}

bool parseCriterionJson(const std::string& criterionName, const JsonValue& root,
                        const std::string& defaultSpace, AdvancementCriterion& out,
                        std::string& problem) {
    if (!root.isObject()) {
        problem = "criterion \"" + criterionName + "\" must be an object";
        return false;
    }
    const JsonValue* trigger = root.find("trigger");
    if (trigger == nullptr || !trigger->isString() ||
        !parseAdvancementTriggerName(trigger->stringValue(), out.trigger)) {
        problem = "criterion \"" + criterionName + "\" needs a known \"trigger\"";
        return false;
    }
    out.name = criterionName;
    out.requiredCount = static_cast<int>(root.intOr("count", 1));
    if (out.requiredCount < 1) {
        problem = "criterion \"" + criterionName + "\" needs a count of at least 1";
        return false;
    }

    ResourceId id;
    bool present = false;
    if (!readOptionalResource(root, "item", defaultSpace, id, present, problem)) return false;
    if (present) {
        if (!resolveItemId(id, out.item)) {
            problem = "criterion \"" + criterionName + "\": no such item " + id.text();
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchItem;
    }
    if (!readOptionalResource(root, "block", defaultSpace, id, present, problem)) return false;
    if (present) {
        if (!resolveBlockId(id, out.block)) {
            problem = "criterion \"" + criterionName + "\": no such block " + id.text();
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchBlock;
    }
    if (!readOptionalResource(root, "tool", defaultSpace, id, present, problem)) return false;
    if (present) {
        if (!resolveItemId(id, out.tool)) {
            problem = "criterion \"" + criterionName + "\": no such tool " + id.text();
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchTool;
    }

    if (const JsonValue* species = root.find("species")) {
        if (!species->isString() || !parseSpeciesName(species->stringValue(), out.species)) {
            problem = "criterion \"" + criterionName + "\": unknown species";
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchSpecies;
    }
    if (const JsonValue* dimension = root.find("dimension")) {
        if (!dimension->isString() ||
            !parseDimensionName(dimension->stringValue(), out.dimension)) {
            problem = "criterion \"" + criterionName + "\": unknown dimension";
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchDimension;
    }
    if (const JsonValue* potion = root.find("potion")) {
        if (!potion->isString() || !parsePotionName(potion->stringValue(), out.potion)) {
            problem = "criterion \"" + criterionName + "\": unknown potion";
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchPotion;
    }
    if (const JsonValue* profession = root.find("profession")) {
        if (!profession->isString() ||
            !parseProfessionName(profession->stringValue(), out.profession)) {
            problem = "criterion \"" + criterionName + "\": unknown villager profession";
            return false;
        }
        out.matchMask |= AdvancementCriterion::MatchProfession;
    }
    return true;
}

} // namespace

AdvancementCriterion::Event AdvancementCriterion::Event::itemObtained(ItemId item, int amount) {
    Event event;
    event.trigger = Trigger::ItemObtained;
    event.item = item;
    event.amount = amount;
    return event;
}

AdvancementCriterion::Event AdvancementCriterion::Event::mobKilled(Species species) {
    Event event;
    event.trigger = Trigger::MobKilled;
    event.species = species;
    return event;
}

AdvancementCriterion::Event AdvancementCriterion::Event::dimensionEntered(DimensionId dimension) {
    Event event;
    event.trigger = Trigger::DimensionEntered;
    event.dimension = dimension;
    return event;
}

AdvancementCriterion::Event AdvancementCriterion::Event::potionBrewed(PotionKind potion) {
    Event event;
    event.trigger = Trigger::PotionBrewed;
    event.potion = potion;
    return event;
}

AdvancementCriterion::Event AdvancementCriterion::Event::villagerTraded(
    VillagerProfession profession, ItemId purchased) {
    Event event;
    event.trigger = Trigger::VillagerTraded;
    event.profession = profession;
    event.item = purchased;
    return event;
}

AdvancementCriterion::Event AdvancementCriterion::Event::blockBroken(Block block, ItemId tool) {
    Event event;
    event.trigger = Trigger::BlockBroken;
    event.block = block;
    event.tool = tool;
    return event;
}

bool AdvancementCriterion::accepts(const Event& event) const {
    if (event.trigger != trigger) return false;
    if ((matchMask & MatchItem) != 0u && event.item != item) return false;
    if ((matchMask & MatchBlock) != 0u && event.block != block) return false;
    if ((matchMask & MatchSpecies) != 0u && event.species != species) return false;
    if ((matchMask & MatchDimension) != 0u && event.dimension != dimension) return false;
    if ((matchMask & MatchPotion) != 0u && event.potion != potion) return false;
    if ((matchMask & MatchProfession) != 0u && event.profession != profession) return false;
    if ((matchMask & MatchTool) != 0u && event.tool != tool) return false;
    return true;
}

const AdvancementCriterion* Advancement::findCriterion(const std::string& criterionName) const {
    for (const AdvancementCriterion& criterion : criteria)
        if (criterion.name == criterionName) return &criterion;
    return nullptr;
}

bool Advancement::satisfiedBy(const std::vector<std::string>& completed) const {
    if (criteria.empty()) return false;
    auto isComplete = [&completed](const std::string& name) {
        return std::find(completed.begin(), completed.end(), name) != completed.end();
    };
    if (requirements.empty()) {
        for (const AdvancementCriterion& criterion : criteria)
            if (!isComplete(criterion.name)) return false;
        return true;
    }
    for (const std::vector<std::string>& group : requirements) {
        bool any = false;
        for (const std::string& name : group)
            if (isComplete(name)) any = true;
        if (!any) return false;
    }
    return true;
}

std::vector<std::string> Advancement::danglingRequirements() const {
    std::vector<std::string> out;
    for (const std::vector<std::string>& group : requirements)
        for (const std::string& name : group)
            if (findCriterion(name) == nullptr) out.push_back(name);
    return out;
}

bool AdvancementTree::add(const Advancement& advancement) {
    if (!advancement.id.valid()) return false;
    if (nodes.count(advancement.id) > 0u) return false;
    nodes[advancement.id] = advancement;
    return true;
}

void AdvancementTree::clear() { nodes.clear(); }

const Advancement* AdvancementTree::find(const AdvancementId& id) const {
    auto found = nodes.find(id);
    return found == nodes.end() ? nullptr : &found->second;
}

std::vector<AdvancementId> AdvancementTree::ids() const {
    std::vector<AdvancementId> out;
    out.reserve(nodes.size());
    for (const auto& node : nodes) out.push_back(node.first);
    return out;
}

std::vector<AdvancementId> AdvancementTree::roots() const {
    std::vector<AdvancementId> out;
    for (const auto& node : nodes)
        if (node.second.isRoot()) out.push_back(node.first);
    return out;
}

std::vector<AdvancementId> AdvancementTree::childrenOf(const AdvancementId& id) const {
    std::vector<AdvancementId> out;
    for (const auto& node : nodes)
        if (!node.second.isRoot() && node.second.parent == id) out.push_back(node.first);
    return out;
}

int AdvancementTree::depthOf(const AdvancementId& id) const {
    const Advancement* node = find(id);
    if (node == nullptr) return -1;
    int depth = 0;
    AdvancementId walk = id;
    for (std::size_t step = 0u; step <= nodes.size(); ++step) {
        const Advancement* here = find(walk);
        if (here == nullptr) return -1;
        if (here->isRoot()) return depth;
        walk = here->parent;
        ++depth;
    }
    return -1;
}

bool AdvancementTree::cycleFrom(const AdvancementId& id) const {
    std::vector<AdvancementId> seen;
    AdvancementId walk = id;
    for (;;) {
        for (const AdvancementId& visited : seen)
            if (visited == walk) return true;
        seen.push_back(walk);
        const Advancement* here = find(walk);
        if (here == nullptr || here->isRoot()) return false;
        walk = here->parent;
    }
}

bool AdvancementTree::hasCycle() const {
    for (const auto& node : nodes)
        if (cycleFrom(node.first)) return true;
    return false;
}

std::vector<AdvancementId> AdvancementTree::missingParents() const {
    std::vector<AdvancementId> out;
    for (const auto& node : nodes) {
        if (node.second.isRoot()) continue;
        if (find(node.second.parent) == nullptr) out.push_back(node.first);
    }
    return out;
}

std::vector<AdvancementId> AdvancementProgress::notify(const AdvancementCriterion::Event& event,
                                                       long long tick) {
    std::vector<AdvancementId> justGranted;
    for (const AdvancementId& id : source->ids()) {
        const Advancement* node = source->find(id);
        bool touched = false;
        for (const AdvancementCriterion& criterion : node->criteria) {
            if (!criterion.accepts(event)) continue;
            CriterionState& state = states[id][criterion.name];
            if (state.done) continue;
            state.progress += event.amount;
            if (state.progress >= criterion.requiredCount) {
                state.progress = criterion.requiredCount;
                state.done = true;
                state.tick = tick;
            }
            touched = true;
        }
        if (!touched || granted(id)) continue;
        if (!node->satisfiedBy(completedCriteria(id))) continue;

        grants[id] = tick;
        experience += node->reward.experience;
        for (const ResourceId& recipe : node->reward.recipes) {
            if (std::find(recipes.begin(), recipes.end(), recipe) == recipes.end())
                recipes.push_back(recipe);
        }
        for (const ResourceId& table : node->reward.lootTables) {
            if (std::find(lootTables.begin(), lootTables.end(), table) == lootTables.end())
                lootTables.push_back(table);
        }
        justGranted.push_back(id);
    }
    return justGranted;
}

bool AdvancementProgress::criterionDone(const AdvancementId& id,
                                        const std::string& criterion) const {
    auto node = states.find(id);
    if (node == states.end()) return false;
    auto state = node->second.find(criterion);
    return state != node->second.end() && state->second.done;
}

int AdvancementProgress::criterionProgress(const AdvancementId& id,
                                           const std::string& criterion) const {
    auto node = states.find(id);
    if (node == states.end()) return 0;
    auto state = node->second.find(criterion);
    return state == node->second.end() ? 0 : state->second.progress;
}

long long AdvancementProgress::criterionTick(const AdvancementId& id,
                                             const std::string& criterion) const {
    auto node = states.find(id);
    if (node == states.end()) return -1;
    auto state = node->second.find(criterion);
    return state == node->second.end() ? -1 : state->second.tick;
}

std::vector<std::string> AdvancementProgress::completedCriteria(const AdvancementId& id) const {
    std::vector<std::string> out;
    auto node = states.find(id);
    if (node == states.end()) return out;
    for (const auto& state : node->second)
        if (state.second.done) out.push_back(state.first);
    return out;
}

bool AdvancementProgress::granted(const AdvancementId& id) const { return grants.count(id) > 0u; }

long long AdvancementProgress::grantedTick(const AdvancementId& id) const {
    auto found = grants.find(id);
    return found == grants.end() ? -1 : found->second;
}

bool AdvancementProgress::visible(const AdvancementId& id) const {
    const Advancement* node = source->find(id);
    if (node == nullptr) return false;
    if (!node->hidden) return true;
    if (granted(id)) return true;
    return !node->isRoot() && granted(node->parent);
}

std::vector<AdvancementId> AdvancementProgress::grantedIds() const {
    std::vector<AdvancementId> out;
    out.reserve(grants.size());
    for (const auto& grant : grants) out.push_back(grant.first);
    return out;
}

const char* advancementTriggerName(AdvancementCriterion::Trigger trigger) {
    switch (trigger) {
        case AdvancementCriterion::Trigger::ItemObtained: return "itemObtained";
        case AdvancementCriterion::Trigger::MobKilled: return "mobKilled";
        case AdvancementCriterion::Trigger::DimensionEntered: return "dimensionEntered";
        case AdvancementCriterion::Trigger::PotionBrewed: return "potionBrewed";
        case AdvancementCriterion::Trigger::VillagerTraded: return "villagerTraded";
        case AdvancementCriterion::Trigger::BlockBroken: return "blockBroken";
        case AdvancementCriterion::Trigger::Count: break;
    }
    return "unknown";
}

bool parseAdvancementTriggerName(const std::string& text, AdvancementCriterion::Trigger& out) {
    for (int i = 0; i < static_cast<int>(AdvancementCriterion::Trigger::Count); ++i) {
        AdvancementCriterion::Trigger trigger = static_cast<AdvancementCriterion::Trigger>(i);
        if (text == advancementTriggerName(trigger)) {
            out = trigger;
            return true;
        }
    }
    return false;
}

bool parseAdvancementJson(const JsonValue& root, const std::string& defaultSpace, Advancement& out,
                          std::string& problem) {
    if (!root.isObject()) {
        problem = "an advancement must be an object";
        return false;
    }
    Advancement parsed;
    bool present = false;
    if (!readOptionalResource(root, "parent", defaultSpace, parsed.parent, present, problem))
        return false;

    if (const JsonValue* display = root.find("display")) {
        if (!display->isObject()) {
            problem = "\"display\" must be an object";
            return false;
        }
        parsed.title = display->stringOr("title", "");
        parsed.description = display->stringOr("description", "");
        parsed.hidden = display->boolOr("hidden", false);
    }

    const JsonValue* criteria = root.find("criteria");
    if (criteria == nullptr || !criteria->isObject() || criteria->members().empty()) {
        problem = "\"criteria\" must be a non empty object";
        return false;
    }
    for (const JsonValue::Member& member : criteria->members()) {
        AdvancementCriterion criterion;
        if (!parseCriterionJson(member.first, member.second, defaultSpace, criterion, problem))
            return false;
        parsed.criteria.push_back(criterion);
    }

    if (const JsonValue* requirements = root.find("requirements")) {
        if (!requirements->isArray()) {
            problem = "\"requirements\" must be an array of groups";
            return false;
        }
        for (const JsonValue& group : requirements->elements()) {
            if (!group.isArray() || group.elements().empty()) {
                problem = "every requirement group must be a non empty array";
                return false;
            }
            std::vector<std::string> names;
            for (const JsonValue& name : group.elements()) {
                if (!name.isString()) {
                    problem = "requirement groups hold criterion names";
                    return false;
                }
                names.push_back(name.stringValue());
            }
            parsed.requirements.push_back(names);
        }
        std::vector<std::string> dangling = parsed.danglingRequirements();
        if (!dangling.empty()) {
            problem = "requirement names no criterion declares: " + dangling.front();
            return false;
        }
    }

    if (const JsonValue* rewards = root.find("rewards")) {
        if (!rewards->isObject()) {
            problem = "\"rewards\" must be an object";
            return false;
        }
        parsed.reward.experience = static_cast<int>(rewards->intOr("experience", 0));
        if (parsed.reward.experience < 0) {
            problem = "\"experience\" cannot be negative";
            return false;
        }
        if (!readResourceList(*rewards, "recipes", defaultSpace, parsed.reward.recipes, problem))
            return false;
        if (!readResourceList(*rewards, "loot", defaultSpace, parsed.reward.lootTables, problem))
            return false;
    }

    out = parsed;
    return true;
}

AdvancementTree loadAdvancements(const DataPackLoader& loader, std::vector<DataPackError>& errors) {
    AdvancementTree tree;
    const std::map<ResourceId, JsonValue>* documents = loader.documentsIn("advancements");
    if (documents == nullptr) return tree;
    for (const auto& document : *documents) {
        Advancement advancement;
        std::string problem;
        if (!parseAdvancementJson(document.second, document.first.space, advancement, problem)) {
            DataPackError failure;
            failure.file = "data/" + document.first.space + "/advancements/" + document.first.path +
                           ".json";
            failure.reason = problem;
            errors.push_back(failure);
            continue;
        }
        advancement.id = document.first;
        tree.add(advancement);
    }
    return tree;
}

} // namespace vf
