#include "testHarness.h"
#include "vf/Behavior.h"

#include <memory>
#include <string>
#include <vector>

using namespace vf;

namespace {

World flatWorld(int halfSpan = 24) {
    World world;
    for (int x = -halfSpan; x <= halfSpan; ++x) {
        for (int z = -halfSpan; z <= halfSpan; ++z) world.setBlock(x, 0, z, Block::Stone);
    }
    return world;
}

World grassWorld(int halfSpan = 12) {
    World world;
    for (int x = -halfSpan; x <= halfSpan; ++x) {
        for (int z = -halfSpan; z <= halfSpan; ++z) world.setBlock(x, 0, z, Block::Grass);
    }
    return world;
}

std::unique_ptr<BehaviorNode> constantAction(const char* name, BehaviorStatus status, int* counter) {
    return std::unique_ptr<BehaviorNode>(new ActionNode(name, [status, counter](BehaviorContext&) {
        if (counter != nullptr) ++(*counter);
        return status;
    }));
}

std::unique_ptr<BehaviorNode> flagCondition(const char* name, const bool* source) {
    return std::unique_ptr<BehaviorNode>(
        new ConditionNode(name, [source](BehaviorContext&) { return *source; }));
}

// Reports Running for `runningTicks` calls, then Success once, then restarts.
std::unique_ptr<BehaviorNode> slowAction(const char* name, int runningTicks, int* counter,
                                         int* progress) {
    return std::unique_ptr<BehaviorNode>(
        new ActionNode(name, [runningTicks, counter, progress](BehaviorContext&) {
            ++(*counter);
            ++(*progress);
            if (*progress <= runningTicks) return BehaviorStatus::Running;
            *progress = 0;
            return BehaviorStatus::Success;
        }));
}

BehaviorContext freshContext(const World* world) {
    BehaviorContext ctx;
    ctx.world = world;
    return ctx;
}

BehaviorAgent makeAgent(Species species, const Vec3& position) {
    BehaviorAgent agent;
    agent.species = species;
    agent.position = position;
    agent.homePosition = position;
    const SpeciesStats& stats = speciesStats(species);
    agent.health = stats.maxHealth;
    agent.maxHealth = stats.maxHealth;
    agent.moveStep = 0.25f;
    return agent;
}

} // namespace

VF_TEST(blackboardKeepsEachValueTypeInItsOwnDrawer) {
    Blackboard board;
    board.setNumber("threat", 4.5f);
    board.setCount("meals", 3);
    board.setFlag("engaged", true);
    board.setPoint("lastSeen", Vec3{1.0f, 2.0f, 3.0f});
    board.setCell("target", Vec3i{4, 5, 6});
    board.setRoute("path", {Vec3i{0, 1, 0}, Vec3i{1, 1, 0}});

    CHECK_NEAR(board.number("threat"), 4.5f, 1e-6f);
    CHECK_EQ(board.count("meals"), 3);
    CHECK(board.flag("engaged"));
    CHECK_NEAR(board.point("lastSeen").y, 2.0f, 1e-6f);
    CHECK(board.cell("target") == Vec3i(4, 5, 6));
    CHECK(board.route("path") != nullptr);
    CHECK_EQ(board.route("path")->size(), 2u);
    CHECK_EQ(board.entryCount(), 6u);

    // A key written as one type is invisible to every other type.
    CHECK(!board.hasCount("threat"));
    CHECK(!board.hasNumber("meals"));
    CHECK(board.route("threat") == nullptr);
    CHECK_NEAR(board.number("missing", -1.0f), -1.0f, 1e-6f);

    board.eraseNumber("threat");
    CHECK(!board.hasNumber("threat"));
    CHECK_EQ(board.entryCount(), 5u);
    board.eraseRoute("path");
    CHECK(!board.hasRoute("path"));

    board.clear();
    CHECK_EQ(board.entryCount(), 0u);
}

VF_TEST(sequenceStopsAtTheChildThatFailsAndSkipsTheRest) {
    int firstRuns = 0;
    int lastRuns = 0;
    bool gateOpen = false;

    auto root = std::unique_ptr<SequenceNode>(new SequenceNode("sequence"));
    root->add(constantAction("first", BehaviorStatus::Success, &firstRuns));
    root->add(flagCondition("gate", &gateOpen));
    root->add(constantAction("last", BehaviorStatus::Success, &lastRuns));

    BehaviorTree tree(std::move(root));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(firstRuns, 1);
    CHECK_EQ(lastRuns, 0);

    gateOpen = true;
    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(firstRuns, 2);
    CHECK_EQ(lastRuns, 1);
}

VF_TEST(selectorTakesTheFirstBranchThatSucceedsAndFailsWhenNoneDo) {
    int firstRuns = 0;
    int secondRuns = 0;
    int thirdRuns = 0;

    auto root = std::unique_ptr<SelectorNode>(new SelectorNode("selector"));
    root->add(constantAction("first", BehaviorStatus::Failure, &firstRuns));
    root->add(constantAction("second", BehaviorStatus::Success, &secondRuns));
    root->add(constantAction("third", BehaviorStatus::Success, &thirdRuns));

    BehaviorTree tree(std::move(root));
    BehaviorContext ctx;
    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(firstRuns, 1);
    CHECK_EQ(secondRuns, 1);
    CHECK_EQ(thirdRuns, 0);

    auto allFail = std::unique_ptr<SelectorNode>(new SelectorNode("selector"));
    allFail->add(constantAction("a", BehaviorStatus::Failure, nullptr));
    allFail->add(constantAction("b", BehaviorStatus::Failure, nullptr));
    BehaviorTree hopeless(std::move(allFail));
    CHECK(hopeless.tick(ctx) == BehaviorStatus::Failure);
}

VF_TEST(aRunningNodeResumesNextTickInsteadOfRestartingTheSequence) {
    int firstRuns = 0;
    int slowRuns = 0;
    int slowProgress = 0;

    auto root = std::unique_ptr<SequenceNode>(new SequenceNode("sequence"));
    root->add(constantAction("first", BehaviorStatus::Success, &firstRuns));
    root->add(slowAction("slow", 2, &slowRuns, &slowProgress));

    BehaviorTree tree(std::move(root));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK_EQ(firstRuns, 1);
    CHECK_EQ(slowRuns, 1);

    // The whole point: the first child must not run again while the second is
    // still Running.
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK_EQ(firstRuns, 1);
    CHECK_EQ(slowRuns, 2);

    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(firstRuns, 1);
    CHECK_EQ(slowRuns, 3);

    // Once settled the sequence starts over from the top.
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK_EQ(firstRuns, 2);
    CHECK_EQ(slowRuns, 4);
}

VF_TEST(selectorResumesTheRunningBranchWithoutRecheckingEarlierOnes) {
    int guardChecks = 0;
    int slowRuns = 0;
    int slowProgress = 0;

    auto root = std::unique_ptr<SelectorNode>(new SelectorNode("selector"));
    root->add(std::unique_ptr<BehaviorNode>(
        new ConditionNode("guard", [&guardChecks](BehaviorContext&) {
            ++guardChecks;
            return false;
        })));
    root->add(slowAction("slow", 3, &slowRuns, &slowProgress));

    BehaviorTree tree(std::move(root));
    BehaviorContext ctx;

    tree.tick(ctx);
    CHECK_EQ(guardChecks, 1);
    tree.tick(ctx);
    tree.tick(ctx);
    CHECK_EQ(guardChecks, 1);
    CHECK_EQ(slowRuns, 3);
    CHECK_EQ(tree.root()->childAt(1)->name(), std::string("slow"));
}

VF_TEST(cooldownBlocksForExactlyTheRequestedNumberOfTicks) {
    int runs = 0;
    auto gated = std::unique_ptr<BehaviorNode>(
        new CooldownNode("cooldown", constantAction("fire", BehaviorStatus::Success, &runs), 3));
    BehaviorTree tree(std::move(gated));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(runs, 1);

    for (int blocked = 0; blocked < 3; ++blocked) {
        CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
        CHECK_EQ(runs, 1);
    }

    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(runs, 2);

    // And the block starts again immediately after the second firing.
    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(runs, 2);
}

VF_TEST(resettingTheTreeClearsALiveCooldown) {
    int runs = 0;
    auto gated = std::unique_ptr<BehaviorNode>(
        new CooldownNode("cooldown", constantAction("fire", BehaviorStatus::Success, &runs), 5));
    BehaviorTree tree(std::move(gated));
    BehaviorContext ctx;

    tree.tick(ctx);
    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    tree.reset();
    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(runs, 2);
}

VF_TEST(inverterFlipsSuccessAndFailureButLetsRunningThrough) {
    int progress = 0;
    int runs = 0;

    BehaviorContext ctx;
    BehaviorTree yes(std::unique_ptr<BehaviorNode>(new InverterNode(
        "invert", constantAction("succeeds", BehaviorStatus::Success, nullptr))));
    CHECK(yes.tick(ctx) == BehaviorStatus::Failure);

    BehaviorTree no(std::unique_ptr<BehaviorNode>(
        new InverterNode("invert", constantAction("fails", BehaviorStatus::Failure, nullptr))));
    CHECK(no.tick(ctx) == BehaviorStatus::Success);

    BehaviorTree waiting(std::unique_ptr<BehaviorNode>(
        new InverterNode("invert", slowAction("slow", 2, &runs, &progress))));
    CHECK(waiting.tick(ctx) == BehaviorStatus::Running);
}

VF_TEST(repeaterSucceedsOnlyAfterTheRequestedNumberOfCompletions) {
    int runs = 0;
    auto repeat = std::unique_ptr<BehaviorNode>(new RepeaterNode(
        "repeat", constantAction("beat", BehaviorStatus::Success, &runs), 3));
    BehaviorTree tree(std::move(repeat));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(runs, 3);

    int failingRuns = 0;
    auto stopping = std::unique_ptr<BehaviorNode>(new RepeaterNode(
        "repeat", constantAction("beat", BehaviorStatus::Failure, &failingRuns), 3, true));
    BehaviorTree strict(std::move(stopping));
    CHECK(strict.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(failingRuns, 1);
}

VF_TEST(retryUntilSuccessGivesUpAfterTheAttemptBudget) {
    int runs = 0;
    auto retry = std::unique_ptr<BehaviorNode>(new RetryUntilSuccessNode(
        "retry", constantAction("attempt", BehaviorStatus::Failure, &runs), 3));
    BehaviorTree tree(std::move(retry));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(runs, 3);
}

VF_TEST(timeLimitCutsOffAChildThatNeverSettles) {
    int runs = 0;
    auto limited = std::unique_ptr<BehaviorNode>(new TimeLimitNode(
        "window", constantAction("stall", BehaviorStatus::Running, &runs), 3));
    BehaviorTree tree(std::move(limited));
    BehaviorContext ctx;

    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(runs, 3);

    // The budget is refilled once the decorator gave its verdict.
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK_EQ(runs, 4);
}

VF_TEST(parallelSettlesOnItsSuccessAndFailureThresholds) {
    int slowRuns = 0;
    int slowProgress = 0;

    auto wide = std::unique_ptr<ParallelNode>(new ParallelNode("parallel", 2, 2));
    wide->add(constantAction("ok", BehaviorStatus::Success, nullptr));
    wide->add(slowAction("slow", 2, &slowRuns, &slowProgress));
    wide->add(constantAction("nope", BehaviorStatus::Failure, nullptr));

    BehaviorTree tree(std::move(wide));
    BehaviorContext ctx;

    // One success, one failure, one still running: neither threshold is met.
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    CHECK(tree.tick(ctx) == BehaviorStatus::Running);
    // The slow child succeeds on its third visit, which is the second success.
    CHECK(tree.tick(ctx) == BehaviorStatus::Success);
    CHECK_EQ(slowRuns, 3);

    auto doomed = std::unique_ptr<ParallelNode>(new ParallelNode("parallel", 3, 2));
    doomed->add(constantAction("no", BehaviorStatus::Failure, nullptr));
    doomed->add(constantAction("alsoNo", BehaviorStatus::Failure, nullptr));
    doomed->add(constantAction("yes", BehaviorStatus::Success, nullptr));
    BehaviorTree unlucky(std::move(doomed));
    CHECK(unlucky.tick(ctx) == BehaviorStatus::Failure);
}

VF_TEST(anOverDeepTreeIsRejectedInsteadOfOverflowingTheStack) {
    std::unique_ptr<BehaviorNode> node = constantAction("leaf", BehaviorStatus::Success, nullptr);
    for (int level = 0; level < maxBehaviorDepth + 12; ++level) {
        node = std::unique_ptr<BehaviorNode>(new InverterNode("wrap", std::move(node)));
    }

    BehaviorTree tree(std::move(node));
    CHECK(!tree.structureValid());
    CHECK(tree.stats().depth > maxBehaviorDepth);
    CHECK_EQ(tree.stats().nodeCount, maxBehaviorDepth + 13);

    BehaviorContext ctx;
    CHECK(tree.tick(ctx) == BehaviorStatus::Failure);
    CHECK_EQ(ctx.tick, 0);
    CHECK_EQ(tree.tickCount(), 0);
}

VF_TEST(aTreeExactlyAtTheDepthLimitStillRuns) {
    std::unique_ptr<BehaviorNode> node = constantAction("leaf", BehaviorStatus::Success, nullptr);
    for (int level = 0; level < maxBehaviorDepth - 1; ++level) {
        node = std::unique_ptr<BehaviorNode>(new InverterNode("wrap", std::move(node)));
    }

    BehaviorTree tree(std::move(node));
    CHECK_EQ(tree.stats().depth, maxBehaviorDepth);
    CHECK(tree.structureValid());

    BehaviorContext ctx;
    tree.tick(ctx);
    CHECK_EQ(ctx.tick, 1);
}

VF_TEST(theTraceRecordsEveryNodeVisitedInOrder) {
    auto root = std::unique_ptr<SequenceNode>(new SequenceNode("root"));
    root->add(constantAction("first", BehaviorStatus::Success, nullptr));
    root->add(constantAction("second", BehaviorStatus::Failure, nullptr));
    root->add(constantAction("third", BehaviorStatus::Success, nullptr));

    BehaviorTree tree(std::move(root));
    BehaviorContext ctx;
    ctx.traceEnabled = true;
    tree.tick(ctx);

    std::vector<std::string> expected{"root", "first", "second"};
    CHECK_EQ(ctx.trace.size(), expected.size());
    for (std::size_t i = 0; i < expected.size() && i < ctx.trace.size(); ++i) {
        CHECK_EQ(ctx.trace[i], expected[i]);
    }
}

VF_TEST(theHostileTreePathsToThePlayerAndLandsRepeatedBlows) {
    World world = flatWorld();
    BehaviorAgent agent = makeAgent(Species::Zombie, {2.5f, 1.0f, 2.5f});

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.playerPresent = true;
    ctx.playerPosition = {10.5f, 1.0f, 2.5f};

    BehaviorTree tree = makeHostileHunterTree();
    CHECK(tree.structureValid());

    float startDistance = behaviorHorizontalDistance(agent.position, ctx.playerPosition);
    for (int step = 0; step < 120; ++step) tree.tick(ctx);

    CHECK(behaviorHorizontalDistance(agent.position, ctx.playerPosition) < startDistance);
    CHECK(behaviorHorizontalDistance(agent.position, ctx.playerPosition) <=
          speciesStats(Species::Zombie).attackRange);
    CHECK(agent.attacksLanded >= 2);
    CHECK(ctx.blackboard.flag("engaged"));
    CHECK_NEAR(ctx.blackboard.number("lastAttackDamage"),
               speciesStats(Species::Zombie).attackDamage, 1e-6f);
    CHECK(agent.routeGoal == behaviorCellOf(ctx.playerPosition));
    CHECK(agent.routePlannedTick > 0);
}

VF_TEST(theHostileTreeWalksHomeOnceTheTargetIsGone) {
    World world = flatWorld();
    BehaviorAgent agent = makeAgent(Species::Zombie, {9.5f, 1.0f, 2.5f});
    agent.homePosition = {2.5f, 1.0f, 2.5f};

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.playerPresent = false;

    BehaviorTree tree = makeHostileHunterTree();
    for (int step = 0; step < 120; ++step) tree.tick(ctx);

    CHECK(behaviorHorizontalDistance(agent.position, agent.homePosition) <=
          ctx.tuning.homeReturnRadius);
    CHECK_EQ(agent.attacksLanded, 0);
}

VF_TEST(theAnimalTreeRunsAwayFromAnEmptyHandedPlayer) {
    World world = grassWorld();
    BehaviorAgent agent = makeAgent(Species::Sheep, {0.5f, 1.0f, 0.5f});

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.playerPresent = true;
    ctx.playerPosition = {2.5f, 1.0f, 0.5f};
    ctx.playerHeldItem = ItemId::None;

    BehaviorTree tree = makeGrazingAnimalTree();
    float before = behaviorHorizontalDistance(agent.position, ctx.playerPosition);
    for (int step = 0; step < 20; ++step) tree.tick(ctx);

    CHECK(behaviorHorizontalDistance(agent.position, ctx.playerPosition) > before + 2.0f);
    CHECK_EQ(agent.breedCount, 0);
}

VF_TEST(theAnimalTreeFollowsHeldFoodAndBreedsOnlyOncePerCooldown) {
    World world = grassWorld();
    BehaviorAgent agent = makeAgent(Species::Sheep, {0.5f, 1.0f, 0.5f});

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.playerPresent = true;
    ctx.playerPosition = {6.5f, 1.0f, 0.5f};
    ctx.playerHeldItem = breedingFoodFor(Species::Sheep);
    ctx.matePresent = true;
    ctx.matePosition = {5.0f, 1.0f, 0.5f};

    BehaviorTree tree = makeGrazingAnimalTree();

    std::vector<long long> breedTicks;
    int lastCount = 0;
    for (int step = 0; step < 400; ++step) {
        tree.tick(ctx);
        if (agent.breedCount != lastCount) {
            lastCount = agent.breedCount;
            breedTicks.push_back(ctx.tick);
        }
    }

    CHECK(behaviorHorizontalDistance(agent.position, ctx.playerPosition) <= ctx.tuning.breedRadius);
    CHECK(agent.breedCount >= 2);
    for (std::size_t i = 1; i < breedTicks.size(); ++i) {
        CHECK(breedTicks[i] - breedTicks[i - 1] > ctx.tuning.breedCooldownTicks);
    }
}

VF_TEST(theAnimalTreeGrazesGrassForTheFullChewDuration) {
    World world = grassWorld();
    BehaviorAgent agent = makeAgent(Species::Cow, {0.5f, 1.0f, 0.5f});

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.playerPresent = false;
    ctx.matePresent = false;

    BehaviorTree tree = makeGrazingAnimalTree();
    for (int step = 0; step < ctx.tuning.grazeDurationTicks - 1; ++step) tree.tick(ctx);
    CHECK_EQ(ctx.blackboard.count("meals"), 0);
    CHECK(agent.grazeTicks > 0);

    tree.tick(ctx);
    CHECK_EQ(ctx.blackboard.count("meals"), 1);
    CHECK_EQ(agent.grazeTicks, 0);
}

VF_TEST(theSunAvoiderWalksIntoShadeAndWaitsOutTheDay) {
    World world = flatWorld();
    for (int x = 3; x <= 6; ++x) {
        for (int z = -1; z <= 1; ++z) world.setBlock(x, 4, z, Block::Stone);
    }

    BehaviorAgent agent = makeAgent(Species::Skeleton, {0.5f, 1.0f, 0.5f});
    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.daylight = true;
    ctx.skyLight = 15;

    BehaviorTree tree = makeSunAvoiderTree();
    CHECK(!agent.sheltered);
    for (int step = 0; step < 80; ++step) tree.tick(ctx);

    CHECK(agent.sheltered);
    CHECK(agent.position.x >= 3.0f);
    CHECK(agent.shelterTicks > 0);
    CHECK(ctx.blackboard.hasCell("shadeCell"));
}

VF_TEST(theSunAvoiderPatrolsInsteadOfHidingAtNight) {
    World world = flatWorld();
    BehaviorAgent agent = makeAgent(Species::Skeleton, {0.5f, 1.0f, 0.5f});
    Vec3 start = agent.position;

    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;
    ctx.daylight = false;
    ctx.skyLight = 4;

    BehaviorTree tree = makeSunAvoiderTree();
    for (int step = 0; step < 30; ++step) tree.tick(ctx);

    CHECK(behaviorHorizontalDistance(agent.position, start) > 1.0f);
    CHECK_EQ(agent.shelterTicks, 0);
    CHECK(!ctx.blackboard.hasCell("shadeCell"));
}

VF_TEST(twoRunsOfTheSameTreeOnTheSameSeedProduceTheSameNodeSequence) {
    World world = grassWorld();

    auto runOnce = [&world](std::vector<std::string>& trace, Vec3& endPosition) {
        BehaviorAgent agent = makeAgent(Species::Pig, {0.5f, 1.0f, 0.5f});
        agent.rng = 0x1234abcdu;
        BehaviorContext ctx;
        ctx.world = &world;
        ctx.agent = &agent;
        ctx.traceEnabled = true;
        ctx.playerPresent = true;
        ctx.playerPosition = {4.5f, 1.0f, 0.5f};
        ctx.playerHeldItem = ItemId::None;

        BehaviorTree tree = makeGrazingAnimalTree();
        for (int step = 0; step < 60; ++step) tree.tick(ctx);
        trace = ctx.trace;
        endPosition = agent.position;
    };

    std::vector<std::string> firstTrace;
    std::vector<std::string> secondTrace;
    Vec3 firstEnd;
    Vec3 secondEnd;
    runOnce(firstTrace, firstEnd);
    runOnce(secondTrace, secondEnd);

    CHECK(firstTrace.size() > 100u);
    CHECK_EQ(firstTrace.size(), secondTrace.size());
    for (std::size_t i = 0; i < firstTrace.size() && i < secondTrace.size(); ++i) {
        CHECK_EQ(firstTrace[i], secondTrace[i]);
    }
    CHECK_NEAR(firstEnd.x, secondEnd.x, 1e-6f);
    CHECK_NEAR(firstEnd.z, secondEnd.z, 1e-6f);
}

VF_TEST(eachSpeciesMapsToTheTreeThatFitsIt) {
    CHECK(roleForSpecies(Species::Zombie) == BehaviorRole::HostileHunter);
    CHECK(roleForSpecies(Species::Creeper) == BehaviorRole::HostileHunter);
    CHECK(roleForSpecies(Species::Skeleton) == BehaviorRole::SunAvoider);
    CHECK(roleForSpecies(Species::Sheep) == BehaviorRole::GrazingAnimal);

    CHECK(breedingFoodFor(Species::Cow) == ItemId::Bread);
    CHECK(breedingFoodFor(Species::Pig) == ItemId::Apple);
    CHECK(breedingFoodFor(Species::Zombie) == ItemId::None);
    CHECK(isBreedingFood(Species::Cow, ItemId::Bread));
    CHECK(!isBreedingFood(Species::Cow, ItemId::Apple));
    CHECK(!isBreedingFood(Species::Zombie, ItemId::None));

    for (int index = 0; index < static_cast<int>(BehaviorRole::Count); ++index) {
        BehaviorRole role = static_cast<BehaviorRole>(index);
        BehaviorTree tree = makeTreeForRole(role);
        CHECK(tree.structureValid());
        CHECK(tree.stats().nodeCount > 4);
        CHECK(std::string(behaviorRoleName(role)) != std::string("unknown"));
    }
}

VF_TEST(theSharedRandomStreamIsReproducibleAndStaysInRange) {
    uint32_t first = 0x2545f491u;
    uint32_t second = 0x2545f491u;
    for (int draw = 0; draw < 64; ++draw) {
        CHECK_EQ(behaviorNextRandom(first), behaviorNextRandom(second));
    }

    uint32_t stream = 7u;
    for (int draw = 0; draw < 200; ++draw) {
        float unit = behaviorUnitFloat(stream);
        CHECK(unit >= 0.0f);
        CHECK(unit < 1.0f);
        int ranged = behaviorRandomRange(stream, -3, 3);
        CHECK(ranged >= -3);
        CHECK(ranged <= 3);
    }
    CHECK_EQ(behaviorRandomRange(stream, 5, 5), 5);

    CHECK(behaviorCellOf(Vec3{-0.5f, 1.9f, 3.2f}) == Vec3i(-1, 1, 3));
    CHECK_NEAR(behaviorCellCenter(Vec3i{2, 3, 4}).x, 2.5f, 1e-6f);
    CHECK_NEAR(behaviorCellCenter(Vec3i{2, 3, 4}).y, 3.0f, 1e-6f);
    CHECK(std::string(behaviorStatusName(BehaviorStatus::Running)) == std::string("running"));
}

VF_TEST(routeFollowingWalksCellByCellAndReportsSuccessAtTheEnd) {
    World world = flatWorld();
    BehaviorAgent agent = makeAgent(Species::Cow, {0.5f, 1.0f, 0.5f});
    BehaviorContext ctx = freshContext(&world);
    ctx.agent = &agent;

    CHECK(planRouteTo(ctx, Vec3i{4, 1, 0}, true));
    CHECK(agent.route.size() >= 5u);
    CHECK_EQ(agent.routeCursor, 1);

    BehaviorStatus status = BehaviorStatus::Running;
    int guard = 0;
    while (status == BehaviorStatus::Running && guard < 200) {
        status = followRouteStep(ctx);
        ++guard;
    }
    CHECK(status == BehaviorStatus::Success);
    CHECK(agent.routeFinished());
    CHECK_NEAR(agent.position.x, 4.5f, 1e-4f);
    CHECK(guard < 200);
}
