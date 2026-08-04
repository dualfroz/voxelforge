#include "testHarness.h"
#include "vf/Advancement.h"
#include "vf/Scoreboard.h"

#include <string>

using namespace vf;

namespace {

using Event = AdvancementCriterion::Event;
using Trigger = AdvancementCriterion::Trigger;

ResourceId id(const std::string& text) {
    ResourceId parsed;
    ResourceId::parse(text, "voxelforge", parsed);
    return parsed;
}

const char* const rootAdvancement = R"json({
  "display": {"title": "Getting started", "description": "Chop something down"},
  "criteria": {
    "anyWood": {"trigger": "itemObtained", "item": "voxelforge:wood"}
  },
  "rewards": {"experience": 5}
})json";

const char* const toolsAdvancement = R"json({
  "parent": "voxelforge:root",
  "display": {"title": "A workbench of your own"},
  "criteria": {
    "sticks": {"trigger": "itemObtained", "item": "voxelforge:stick", "count": 4},
    "table": {"trigger": "itemObtained", "item": "voxelforge:crafting_table"}
  },
  "rewards": {"experience": 10, "recipes": ["voxelforge:redstone_torch"]}
})json";

const char* const hunterAdvancement = R"json({
  "parent": "voxelforge:tools",
  "display": {"title": "Monster hunter", "hidden": true},
  "criteria": {
    "zombies": {"trigger": "mobKilled", "species": "zombie", "count": 3},
    "creeper": {"trigger": "mobKilled", "species": "creeper"}
  },
  "requirements": [["zombies", "creeper"]],
  "rewards": {"experience": 20, "loot": ["voxelforge:chests/starter"]}
})json";

const char* const travellerAdvancement = R"json({
  "parent": "voxelforge:root",
  "display": {"title": "Into the fire"},
  "criteria": {
    "portal": {"trigger": "dimensionEntered", "dimension": "nether"},
    "brew": {"trigger": "potionBrewed", "potion": "healing"},
    "trade": {"trigger": "villagerTraded", "profession": "librarian",
              "item": "voxelforge:emerald_gem"},
    "mine": {"trigger": "blockBroken", "block": "voxelforge:obsidian",
             "tool": "voxelforge:diamond_pickaxe"}
  },
  "requirements": [["portal"], ["brew", "trade", "mine"]]
})json";

DataPack contentPack() {
    DataPackMeta meta;
    meta.name = "story";
    DataPack pack(meta);
    pack.addFile("data/voxelforge/advancements/root.json", rootAdvancement);
    pack.addFile("data/voxelforge/advancements/tools.json", toolsAdvancement);
    pack.addFile("data/voxelforge/advancements/hunter.json", hunterAdvancement);
    pack.addFile("data/voxelforge/advancements/traveller.json", travellerAdvancement);
    return pack;
}

AdvancementTree loadTree(const DataPack& pack, std::vector<DataPackError>& errors) {
    DataPackLoader loader;
    loader.addPack(pack);
    loader.load();
    return loadAdvancements(loader, errors);
}

Advancement makeAdvancement(const std::string& self, const std::string& parent) {
    Advancement node;
    node.id = id(self);
    if (!parent.empty()) node.parent = id(parent);
    AdvancementCriterion criterion;
    criterion.name = "tick";
    criterion.trigger = Trigger::ItemObtained;
    node.criteria.push_back(criterion);
    return node;
}

} // namespace

VF_TEST(advancementsLoadOutOfAPackAndKeepTheirTreeShape) {
    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(contentPack(), errors);
    CHECK(errors.empty());
    CHECK_EQ(tree.size(), static_cast<std::size_t>(4));

    const Advancement* root = tree.find(id("voxelforge:root"));
    CHECK(root != nullptr);
    CHECK(root->isRoot());
    CHECK_EQ(root->title, std::string("Getting started"));
    CHECK_EQ(root->description, std::string("Chop something down"));
    CHECK_EQ(root->criteria.size(), static_cast<std::size_t>(1));
    CHECK(root->criteria[0].item == ItemId::Wood);
    CHECK_EQ(root->criteria[0].matchMask, AdvancementCriterion::MatchItem);
    CHECK_EQ(root->reward.experience, 5);

    const Advancement* tools = tree.find(id("voxelforge:tools"));
    CHECK(tools != nullptr);
    CHECK(!tools->isRoot());
    CHECK(tools->parent == id("voxelforge:root"));
    CHECK_EQ(tools->criteria.size(), static_cast<std::size_t>(2));
    CHECK_EQ(tools->findCriterion("sticks")->requiredCount, 4);
    CHECK(tools->findCriterion("absent") == nullptr);
    CHECK_EQ(tools->reward.recipes.size(), static_cast<std::size_t>(1));
    CHECK(tools->reward.recipes[0] == id("voxelforge:redstone_torch"));

    const Advancement* traveller = tree.find(id("voxelforge:traveller"));
    CHECK(traveller->findCriterion("portal")->dimension == DimensionId::Nether);
    CHECK(traveller->findCriterion("brew")->potion == PotionKind::Healing);
    CHECK(traveller->findCriterion("trade")->profession == VillagerProfession::Librarian);
    CHECK(traveller->findCriterion("trade")->item == ItemId::EmeraldGem);
    CHECK(traveller->findCriterion("mine")->block == Block::Obsidian);
    CHECK(traveller->findCriterion("mine")->tool == ItemId::DiamondPickaxe);
    CHECK_EQ(traveller->requirements.size(), static_cast<std::size_t>(2));

    CHECK_EQ(tree.roots().size(), static_cast<std::size_t>(1));
    CHECK(tree.roots()[0] == id("voxelforge:root"));
    CHECK_EQ(tree.childrenOf(id("voxelforge:root")).size(), static_cast<std::size_t>(2));
    CHECK_EQ(tree.childrenOf(id("voxelforge:tools")).size(), static_cast<std::size_t>(1));
    CHECK_EQ(tree.depthOf(id("voxelforge:root")), 0);
    CHECK_EQ(tree.depthOf(id("voxelforge:tools")), 1);
    CHECK_EQ(tree.depthOf(id("voxelforge:hunter")), 2);
    CHECK_EQ(tree.depthOf(id("voxelforge:absent")), -1);
    CHECK(!tree.hasCycle());
    CHECK(tree.missingParents().empty());
    CHECK_EQ(tree.ids().size(), static_cast<std::size_t>(4));
}

VF_TEST(anAdvancementIsGrantedOnlyWhenItsLastCriterionCompletes) {
    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(contentPack(), errors);
    AdvancementProgress progress(tree);
    const AdvancementId tools = id("voxelforge:tools");

    CHECK(!progress.granted(tools));
    CHECK_EQ(progress.grantedCount(), 0);

    std::vector<AdvancementId> granted = progress.notify(Event::itemObtained(ItemId::Stick, 2), 100);
    CHECK(granted.empty());
    CHECK(!progress.criterionDone(tools, "sticks"));
    CHECK_EQ(progress.criterionProgress(tools, "sticks"), 2);
    CHECK_EQ(progress.criterionTick(tools, "sticks"), -1);

    granted = progress.notify(Event::itemObtained(ItemId::Stick, 2), 140);
    CHECK(granted.empty());
    CHECK(progress.criterionDone(tools, "sticks"));
    CHECK_EQ(progress.criterionProgress(tools, "sticks"), 4);
    CHECK_EQ(progress.criterionTick(tools, "sticks"), 140);
    CHECK(!progress.granted(tools));

    // An unrelated item moves nothing.
    granted = progress.notify(Event::itemObtained(ItemId::Dirt, 64), 150);
    CHECK(granted.empty());
    CHECK(!progress.granted(tools));

    granted = progress.notify(Event::itemObtained(ItemId::CraftingTable), 200);
    CHECK_EQ(granted.size(), static_cast<std::size_t>(1));
    if (!granted.empty()) CHECK(granted[0] == tools);
    CHECK(progress.granted(tools));
    CHECK_EQ(progress.grantedTick(tools), 200);
    CHECK_EQ(progress.completedCriteria(tools).size(), static_cast<std::size_t>(2));
    CHECK_EQ(progress.totalExperience(), 10);

    // Granting is a one shot: repeating the last event awards nothing again.
    granted = progress.notify(Event::itemObtained(ItemId::CraftingTable), 260);
    CHECK(granted.empty());
    CHECK_EQ(progress.grantedTick(tools), 200);
    CHECK_EQ(progress.totalExperience(), 10);
    CHECK_EQ(progress.grantedCount(), 1);
}

VF_TEST(requirementGroupsAreSatisfiedByAnyOneOfTheirCriteria) {
    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(contentPack(), errors);
    const Advancement* hunter = tree.find(id("voxelforge:hunter"));
    CHECK(hunter != nullptr);
    CHECK(!hunter->satisfiedBy({}));
    CHECK(hunter->satisfiedBy({"creeper"}));
    CHECK(hunter->satisfiedBy({"zombies"}));
    CHECK(hunter->danglingRequirements().empty());

    // Without requirement groups every criterion is needed.
    const Advancement* tools = tree.find(id("voxelforge:tools"));
    CHECK(tools->requirements.empty());
    CHECK(!tools->satisfiedBy({"sticks"}));
    CHECK(tools->satisfiedBy({"sticks", "table"}));

    const Advancement* traveller = tree.find(id("voxelforge:traveller"));
    CHECK(!traveller->satisfiedBy({"portal"}));
    CHECK(!traveller->satisfiedBy({"brew", "trade", "mine"}));
    CHECK(traveller->satisfiedBy({"portal", "mine"}));

    AdvancementProgress progress(tree);
    std::vector<AdvancementId> granted =
        progress.notify(Event::dimensionEntered(DimensionId::Nether), 10);
    CHECK(granted.empty());
    granted = progress.notify(Event::dimensionEntered(DimensionId::End), 20);
    CHECK(granted.empty());
    granted = progress.notify(Event::villagerTraded(VillagerProfession::Farmer,
                                                    ItemId::EmeraldGem),
                              30);
    CHECK(granted.empty());
    granted = progress.notify(Event::villagerTraded(VillagerProfession::Librarian,
                                                    ItemId::EmeraldGem),
                              40);
    CHECK_EQ(granted.size(), static_cast<std::size_t>(1));
    if (!granted.empty()) CHECK(granted[0] == id("voxelforge:traveller"));
}

VF_TEST(countedCriteriaNeedEveryOneOfTheirEvents) {
    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(contentPack(), errors);
    AdvancementProgress progress(tree);
    const AdvancementId hunter = id("voxelforge:hunter");

    progress.notify(Event::mobKilled(Species::Zombie), 1);
    progress.notify(Event::mobKilled(Species::Skeleton), 2);
    CHECK_EQ(progress.criterionProgress(hunter, "zombies"), 1);
    progress.notify(Event::mobKilled(Species::Zombie), 3);
    CHECK_EQ(progress.criterionProgress(hunter, "zombies"), 2);
    CHECK(!progress.granted(hunter));

    std::vector<AdvancementId> granted = progress.notify(Event::mobKilled(Species::Zombie), 4);
    CHECK_EQ(granted.size(), static_cast<std::size_t>(1));
    if (!granted.empty()) CHECK(granted[0] == hunter);
    CHECK(progress.criterionDone(hunter, "zombies"));
    CHECK(!progress.criterionDone(hunter, "creeper"));
    CHECK_EQ(progress.grantedTick(hunter), 4);
    CHECK_EQ(progress.totalExperience(), 20);
    CHECK_EQ(progress.awardedLootTables().size(), static_cast<std::size_t>(1));
    CHECK(progress.awardedLootTables()[0] == id("voxelforge:chests/starter"));
    CHECK(progress.unlockedRecipes().empty());
}

VF_TEST(hiddenAdvancementsStayOutOfSightUntilTheParentIsGranted) {
    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(contentPack(), errors);
    AdvancementProgress progress(tree);
    const AdvancementId hunter = id("voxelforge:hunter");
    const AdvancementId tools = id("voxelforge:tools");

    CHECK(progress.visible(id("voxelforge:root")));
    CHECK(progress.visible(tools));
    CHECK(!progress.visible(hunter));

    progress.notify(Event::itemObtained(ItemId::Stick, 4), 10);
    CHECK(!progress.visible(hunter));
    progress.notify(Event::itemObtained(ItemId::CraftingTable), 20);
    CHECK(progress.granted(tools));
    CHECK(progress.visible(hunter));

    // A hidden advancement is also its own reveal once it is earned.
    AdvancementTree lone;
    Advancement secret = makeAdvancement("voxelforge:secret", "");
    secret.hidden = true;
    secret.criteria[0].matchMask = AdvancementCriterion::MatchItem;
    secret.criteria[0].item = ItemId::DiamondGem;
    CHECK(lone.add(secret));
    AdvancementProgress hidden(lone);
    CHECK(!hidden.visible(secret.id));
    hidden.notify(Event::itemObtained(ItemId::DiamondGem), 5);
    CHECK(hidden.granted(secret.id));
    CHECK(hidden.visible(secret.id));
    CHECK(!hidden.visible(id("voxelforge:nobody")));
}

VF_TEST(aParentLoopIsReportedInsteadOfWalkedForever) {
    AdvancementTree tree;
    CHECK(tree.add(makeAdvancement("voxelforge:first", "voxelforge:third")));
    CHECK(tree.add(makeAdvancement("voxelforge:second", "voxelforge:first")));
    CHECK(tree.add(makeAdvancement("voxelforge:third", "voxelforge:second")));
    CHECK(tree.add(makeAdvancement("voxelforge:selfish", "voxelforge:selfish")));
    CHECK(tree.add(makeAdvancement("voxelforge:orphan", "voxelforge:nowhere")));

    CHECK(tree.hasCycle());
    CHECK(tree.cycleFrom(id("voxelforge:first")));
    CHECK(tree.cycleFrom(id("voxelforge:second")));
    CHECK(tree.cycleFrom(id("voxelforge:selfish")));
    CHECK(!tree.cycleFrom(id("voxelforge:orphan")));
    CHECK_EQ(tree.depthOf(id("voxelforge:first")), -1);
    CHECK_EQ(tree.depthOf(id("voxelforge:selfish")), -1);
    CHECK_EQ(tree.depthOf(id("voxelforge:orphan")), -1);
    CHECK_EQ(tree.missingParents().size(), static_cast<std::size_t>(1));
    CHECK(tree.missingParents()[0] == id("voxelforge:orphan"));
    CHECK(tree.roots().empty());

    // A looping node still answers questions instead of hanging the caller.
    AdvancementProgress progress(tree);
    CHECK(progress.visible(id("voxelforge:first")));
    std::vector<AdvancementId> granted = progress.notify(Event::itemObtained(ItemId::Dirt), 1);
    CHECK_EQ(granted.size(), static_cast<std::size_t>(5));
    CHECK(progress.granted(id("voxelforge:first")));

    // Adding the same id twice is refused, so a duplicate cannot rewrite a node.
    CHECK(!tree.add(makeAdvancement("voxelforge:first", "voxelforge:third")));
    Advancement nameless;
    CHECK(!tree.add(nameless));
    CHECK_EQ(tree.size(), static_cast<std::size_t>(5));
    tree.clear();
    CHECK_EQ(tree.size(), static_cast<std::size_t>(0));
}

VF_TEST(criteriaOnlyAcceptTheEventsTheyDescribe) {
    AdvancementCriterion anyKill;
    anyKill.trigger = Trigger::MobKilled;
    CHECK(anyKill.accepts(Event::mobKilled(Species::Pig)));
    CHECK(anyKill.accepts(Event::mobKilled(Species::Creeper)));
    CHECK(!anyKill.accepts(Event::itemObtained(ItemId::Stick)));

    AdvancementCriterion creepersOnly = anyKill;
    creepersOnly.matchMask = AdvancementCriterion::MatchSpecies;
    creepersOnly.species = Species::Creeper;
    CHECK(creepersOnly.accepts(Event::mobKilled(Species::Creeper)));
    CHECK(!creepersOnly.accepts(Event::mobKilled(Species::Pig)));

    AdvancementCriterion minedWithIron;
    minedWithIron.trigger = Trigger::BlockBroken;
    minedWithIron.matchMask = AdvancementCriterion::MatchBlock | AdvancementCriterion::MatchTool;
    minedWithIron.block = Block::DiamondOre;
    minedWithIron.tool = ItemId::IronPickaxe;
    CHECK(minedWithIron.accepts(Event::blockBroken(Block::DiamondOre, ItemId::IronPickaxe)));
    CHECK(!minedWithIron.accepts(Event::blockBroken(Block::DiamondOre, ItemId::StonePickaxe)));
    CHECK(!minedWithIron.accepts(Event::blockBroken(Block::CoalOre, ItemId::IronPickaxe)));

    AdvancementCriterion brewed;
    brewed.trigger = Trigger::PotionBrewed;
    brewed.matchMask = AdvancementCriterion::MatchPotion;
    brewed.potion = PotionKind::Swiftness;
    CHECK(brewed.accepts(Event::potionBrewed(PotionKind::Swiftness)));
    CHECK(!brewed.accepts(Event::potionBrewed(PotionKind::Healing)));
    CHECK(!brewed.accepts(Event::dimensionEntered(DimensionId::End)));

    for (int i = 0; i < static_cast<int>(Trigger::Count); ++i) {
        Trigger trigger = static_cast<Trigger>(i);
        Trigger reparsed = Trigger::Count;
        CHECK(parseAdvancementTriggerName(advancementTriggerName(trigger), reparsed));
        CHECK(reparsed == trigger);
    }
    Trigger unknown = Trigger::Count;
    CHECK(!parseAdvancementTriggerName("telekinesis", unknown));
}

VF_TEST(oneUnreadableAdvancementDoesNotCostTheRest) {
    DataPack pack = contentPack();
    pack.addFile("data/voxelforge/advancements/broken.json",
                 R"json({"criteria": {"x": {"trigger": "telekinesis"}}})json");
    pack.addFile("data/voxelforge/advancements/empty.json", R"json({"criteria": {}})json");
    pack.addFile("data/voxelforge/advancements/ghost.json",
                 R"json({"criteria": {"x": {"trigger": "itemObtained",
                          "item": "voxelforge:cheese"}}})json");
    pack.addFile("data/voxelforge/advancements/mismatch.json",
                 R"json({"criteria": {"x": {"trigger": "itemObtained"}},
                          "requirements": [["y"]]})json");

    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(pack, errors);
    CHECK_EQ(errors.size(), static_cast<std::size_t>(4));
    CHECK_EQ(tree.size(), static_cast<std::size_t>(4));
    CHECK(tree.find(id("voxelforge:root")) != nullptr);
    CHECK(tree.find(id("voxelforge:broken")) == nullptr);
    for (const DataPackError& failure : errors) {
        CHECK(failure.file.find("data/voxelforge/advancements/") == 0u);
        CHECK(!failure.reason.empty());
    }
}

VF_TEST(advancementDocumentsAreCheckedFieldByField) {
    Advancement parsed;
    std::string problem;
    const char* const rejected[] = {
        R"json([])json",
        R"json({})json",
        R"json({"criteria": {}})json",
        R"json({"criteria": {"x": {}}})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained", "count": 0}}})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained", "item": "NOT AN ID"}}})json",
        R"json({"criteria": {"x": {"trigger": "mobKilled", "species": "dragon"}}})json",
        R"json({"criteria": {"x": {"trigger": "dimensionEntered", "dimension": "attic"}}})json",
        R"json({"criteria": {"x": {"trigger": "potionBrewed", "potion": "coffee"}}})json",
        R"json({"criteria": {"x": {"trigger": "villagerTraded", "profession": "banker"}}})json",
        R"json({"criteria": {"x": {"trigger": "blockBroken", "block": "voxelforge:cheese"}}})json",
        R"json({"parent": "BAD ID", "criteria": {"x": {"trigger": "itemObtained"}}})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained"}}, "requirements": [[]]})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained"}},
                 "rewards": {"experience": -1}})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained"}},
                 "rewards": {"recipes": ["NOT AN ID"]}})json",
        R"json({"criteria": {"x": {"trigger": "itemObtained"}}, "display": "a string"})json",
    };
    for (const char* text : rejected) {
        JsonParseResult document = parseJson(text);
        CHECK(document.ok);
        CHECK(!parseAdvancementJson(document.root, "voxelforge", parsed, problem));
        CHECK(!problem.empty());
    }

    JsonParseResult minimal =
        parseJson(R"json({"criteria": {"x": {"trigger": "itemObtained"}}})json");
    CHECK(parseAdvancementJson(minimal.root, "voxelforge", parsed, problem));
    CHECK(parsed.isRoot());
    CHECK(!parsed.hidden);
    CHECK_EQ(parsed.criteria.size(), static_cast<std::size_t>(1));
    CHECK_EQ(parsed.criteria[0].matchMask, 0u);
    CHECK_EQ(parsed.criteria[0].requiredCount, 1);
    CHECK(parsed.reward.empty());

    // A bare namespace default applies to every id inside the document.
    JsonParseResult defaulted = parseJson(
        R"json({"parent": "root", "criteria": {"x": {"trigger": "itemObtained",
                 "item": "stick"}}, "rewards": {"recipes": ["torch"]}})json");
    CHECK(parseAdvancementJson(defaulted.root, "extras", parsed, problem));
    CHECK(parsed.parent == ResourceId("extras", "root"));
    CHECK(parsed.criteria[0].item == ItemId::Stick);
    CHECK(parsed.reward.recipes[0] == ResourceId("extras", "torch"));
}

VF_TEST(oneMiningRunFeedsBothTheAdvancementTreeAndTheStatSheet) {
    DataPackMeta meta;
    meta.name = "mining";
    DataPack pack(meta);
    pack.addFile("data/voxelforge/advancements/quarry.json",
                 R"json({"display": {"title": "Quarry"},
                          "criteria": {"stone": {"trigger": "blockBroken",
                                                 "block": "voxelforge:stone", "count": 5}},
                          "rewards": {"experience": 15}})json");

    std::vector<DataPackError> errors;
    AdvancementTree tree = loadTree(pack, errors);
    CHECK(errors.empty());
    AdvancementProgress progress(tree);

    Scoreboard board;
    ScoreObjective mined;
    mined.name = "mined";
    mined.criterion = ScoreObjective::Criterion::Statistic;
    mined.statKind = StatKind::BlocksBroken;
    mined.statSubject = static_cast<int>(Block::Stone);
    CHECK(board.addObjective(mined));

    const AdvancementId quarry = id("voxelforge:quarry");
    for (int strike = 1; strike <= 4; ++strike) {
        progress.notify(Event::blockBroken(Block::Stone, ItemId::IronPickaxe), strike * 10);
        board.stats("foksiu").addBlockBroken(Block::Stone);
        CHECK_EQ(board.score("mined", "foksiu"), strike);
        CHECK(!progress.granted(quarry));
    }

    std::vector<AdvancementId> granted =
        progress.notify(Event::blockBroken(Block::Stone, ItemId::IronPickaxe), 50);
    board.stats("foksiu").addBlockBroken(Block::Stone);
    CHECK_EQ(granted.size(), static_cast<std::size_t>(1));
    CHECK(progress.granted(quarry));
    CHECK_EQ(progress.totalExperience(), 15);
    CHECK_EQ(board.score("mined", "foksiu"), 5);
    CHECK_EQ(board.stats("foksiu").blocksBroken(Block::Stone), 5);

    // Breaking something else moves the sheet but not this advancement.
    board.stats("foksiu").addBlockBroken(Block::Dirt, 3);
    CHECK_EQ(board.score("mined", "foksiu"), 5);
    CHECK_EQ(board.stats("foksiu").total(StatKind::BlocksBroken), 8);
}
