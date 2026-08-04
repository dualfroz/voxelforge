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

