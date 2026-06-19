#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Block.h"
#include "Brewing.h"
#include "DataPack.h"
#include "Dimension.h"
#include "Item.h"
#include "Mob.h"
#include "Villager.h"

namespace vf {

// An advancement is addressed the same way as every other loaded resource.
using AdvancementId = ResourceId;

// One condition inside an advancement, fed by gameplay events. A criterion only
// compares the fields its matchMask names, so "kill any mob" and "kill five
// creepers" are the same structure with different masks and counts.
struct AdvancementCriterion {
    enum class Trigger : uint8_t {
        ItemObtained,
        MobKilled,
        DimensionEntered,
        PotionBrewed,
        VillagerTraded,
        BlockBroken,
        Count
    };

    static constexpr uint32_t MatchItem = 1u << 0;
    static constexpr uint32_t MatchBlock = 1u << 1;
    static constexpr uint32_t MatchSpecies = 1u << 2;
    static constexpr uint32_t MatchDimension = 1u << 3;
    static constexpr uint32_t MatchPotion = 1u << 4;
    static constexpr uint32_t MatchProfession = 1u << 5;
    static constexpr uint32_t MatchTool = 1u << 6;

    // One thing that happened in the world, as the simulation reports it.
    struct Event {
        Trigger trigger = Trigger::ItemObtained;
        ItemId item = ItemId::None;
        Block block = Block::Air;
        Species species = Species::Count;
        DimensionId dimension = DimensionId::Overworld;
        PotionKind potion = PotionKind::Count;
        VillagerProfession profession = VillagerProfession::Unemployed;
        ItemId tool = ItemId::None;
        // How much this one event contributes to a counted criterion.
        int amount = 1;

        static Event itemObtained(ItemId item, int amount = 1);
        static Event mobKilled(Species species);
        static Event dimensionEntered(DimensionId dimension);
        static Event potionBrewed(PotionKind potion);
        static Event villagerTraded(VillagerProfession profession, ItemId purchased);
        static Event blockBroken(Block block, ItemId tool);
    };

    std::string name;
    Trigger trigger = Trigger::ItemObtained;
    uint32_t matchMask = 0u;
    ItemId item = ItemId::None;
    Block block = Block::Air;
    Species species = Species::Count;
    DimensionId dimension = DimensionId::Overworld;
    PotionKind potion = PotionKind::Count;
    VillagerProfession profession = VillagerProfession::Unemployed;
    ItemId tool = ItemId::None;
    int requiredCount = 1;

    bool accepts(const Event& event) const;
};

struct AdvancementReward {
    int experience = 0;
    std::vector<ResourceId> recipes;
    std::vector<ResourceId> lootTables;

    bool empty() const {
        return experience == 0 && recipes.empty() && lootTables.empty();
    }
};

// One node of the tree. An empty parent path marks a root.
//
// requirements is a list of groups: the advancement is granted once every group
// has at least one completed criterion, which covers both "all of them" (one
// group per criterion, or an empty list) and "any of these" (one group holding
// several names). An empty requirements list means every criterion is needed.
struct Advancement {
    AdvancementId id;
    AdvancementId parent;
    std::string title;
    std::string description;
    // Stays out of sight until the parent is granted.
    bool hidden = false;
    std::vector<AdvancementCriterion> criteria;
    std::vector<std::vector<std::string>> requirements;
    AdvancementReward reward;

    bool isRoot() const { return parent.path.empty(); }
    const AdvancementCriterion* findCriterion(const std::string& criterionName) const;
    bool satisfiedBy(const std::vector<std::string>& completed) const;
    // Names used in requirements that no criterion declares.
    std::vector<std::string> danglingRequirements() const;
};

// Every advancement of a world, indexed by id, plus the parent links between
// them. The tree tolerates a missing or looping parent: it reports both rather
// than refusing to hold the node.
class AdvancementTree {
public:
    bool add(const Advancement& advancement);
    void clear();

    const Advancement* find(const AdvancementId& id) const;
    std::size_t size() const { return nodes.size(); }
    std::vector<AdvancementId> ids() const;
    std::vector<AdvancementId> roots() const;
    std::vector<AdvancementId> childrenOf(const AdvancementId& id) const;

    // 0 for a root, -1 when the chain runs into a cycle or a missing parent.
    int depthOf(const AdvancementId& id) const;
    bool cycleFrom(const AdvancementId& id) const;
    bool hasCycle() const;
    // Ids naming a parent that was never loaded.
    std::vector<AdvancementId> missingParents() const;

private:
    std::map<AdvancementId, Advancement> nodes;
};

// One player's progress against one tree. Time is the caller's logical tick, so
// replaying the same events in the same order always yields the same record.
class AdvancementProgress {
public:
    explicit AdvancementProgress(const AdvancementTree& tree) : source(&tree) {}

    // Feeds one event to every criterion that accepts it and returns the
    // advancements this event completed, in id order.
    std::vector<AdvancementId> notify(const AdvancementCriterion::Event& event, long long tick);

    bool criterionDone(const AdvancementId& id, const std::string& criterion) const;
    int criterionProgress(const AdvancementId& id, const std::string& criterion) const;
    // Tick the criterion completed on, -1 while it is still open.
    long long criterionTick(const AdvancementId& id, const std::string& criterion) const;
    std::vector<std::string> completedCriteria(const AdvancementId& id) const;

    bool granted(const AdvancementId& id) const;
    long long grantedTick(const AdvancementId& id) const;
    // Hidden advancements stay invisible until their parent is granted.
    bool visible(const AdvancementId& id) const;

    std::vector<AdvancementId> grantedIds() const;
    int grantedCount() const { return static_cast<int>(grants.size()); }
    int totalExperience() const { return experience; }
    const std::vector<ResourceId>& unlockedRecipes() const { return recipes; }
    const std::vector<ResourceId>& awardedLootTables() const { return lootTables; }

    const AdvancementTree& tree() const { return *source; }

private:
    struct CriterionState {
        int progress = 0;
        bool done = false;
        long long tick = -1;
    };

    const AdvancementTree* source;
    std::map<AdvancementId, std::map<std::string, CriterionState>> states;
    std::map<AdvancementId, long long> grants;
    std::vector<ResourceId> recipes;
    std::vector<ResourceId> lootTables;
    int experience = 0;
};

const char* advancementTriggerName(AdvancementCriterion::Trigger trigger);
bool parseAdvancementTriggerName(const std::string& text, AdvancementCriterion::Trigger& out);

bool parseAdvancementJson(const JsonValue& root, const std::string& defaultSpace, Advancement& out,
                          std::string& problem);

// Reads every document the loader kept under data/<space>/advancements/. A file
// that fails to read costs that advancement and appends one error; the rest of
// the tree still loads.
AdvancementTree loadAdvancements(const DataPackLoader& loader, std::vector<DataPackError>& errors);

} // namespace vf
