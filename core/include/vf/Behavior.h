#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Item.h"
#include "Math.h"
#include "Mob.h"
#include "Pathfind.h"
#include "World.h"

namespace vf {

// Behaviour trees layered on top of the reactive creature AI in Mob.h. A node
// answers with one of three verdicts every tick and the tree remembers which
// node was left Running, so the next tick resumes there instead of restarting
// the whole traversal.
enum class BehaviorStatus : uint8_t {
    Failure = 0,
    Success = 1,
    Running = 2,
};

const char* behaviorStatusName(BehaviorStatus status);

// Traversal is recursive, so a malformed tree would blow the stack. Trees are
// measured when the root is installed and refuse to run past this.
constexpr int maxBehaviorDepth = 48;

// Deterministic xorshift shared by every module built on this one. No clock,
// no global state: the caller owns the stream.
uint32_t behaviorNextRandom(uint32_t& state);
float behaviorUnitFloat(uint32_t& state);
int behaviorRandomRange(uint32_t& state, int minInclusive, int maxInclusive);

Vec3i behaviorCellOf(const Vec3& position);
Vec3 behaviorCellCenter(const Vec3i& cell);
float behaviorHorizontalDistance(const Vec3& a, const Vec3& b);

// Typed shared storage between nodes. One map per value type rather than a
// variant, so a key written as a route can never be read back as a number.
// Ordered maps keep iteration stable for anyone dumping the contents.
class Blackboard {
public:
    void setNumber(const std::string& key, float value);
    void setCount(const std::string& key, int value);
    void setFlag(const std::string& key, bool value);
    void setPoint(const std::string& key, const Vec3& value);
    void setCell(const std::string& key, const Vec3i& value);
    void setRoute(const std::string& key, std::vector<Vec3i> cells);

    bool hasNumber(const std::string& key) const;
    bool hasCount(const std::string& key) const;
    bool hasFlag(const std::string& key) const;
    bool hasPoint(const std::string& key) const;
    bool hasCell(const std::string& key) const;
    bool hasRoute(const std::string& key) const;

    float number(const std::string& key, float fallback = 0.0f) const;
    int count(const std::string& key, int fallback = 0) const;
    bool flag(const std::string& key, bool fallback = false) const;
    Vec3 point(const std::string& key, const Vec3& fallback = {}) const;
    Vec3i cell(const std::string& key, const Vec3i& fallback = {}) const;
    const std::vector<Vec3i>* route(const std::string& key) const;

    void eraseNumber(const std::string& key);
    void eraseRoute(const std::string& key);
    void clear();
    std::size_t entryCount() const;

private:
    std::map<std::string, float> numbers;
    std::map<std::string, int> counts;
    std::map<std::string, bool> flags;
    std::map<std::string, Vec3> points;
    std::map<std::string, Vec3i> cells;
    std::map<std::string, std::vector<Vec3i>> routes;
};

// Tunables the stock trees read. Kept out of the node code so a caller can
// retune a behaviour without rebuilding the tree.
struct BehaviorTuning {
    float arriveEpsilon = 0.35f;
    float fleeRadius = 8.0f;
    float followFoodRadius = 8.0f;
    float breedRadius = 3.0f;
    float homeReturnRadius = 1.2f;
    int breedCooldownTicks = 120;
    int grazeDurationTicks = 40;
    int attackCooldownTicks = 20;
    int repathIntervalTicks = 10;
    int shadeSearchRadius = 6;
    int shadeRunTicks = 120;
    int sunLightThreshold = 12;
    int shelterHoldTicks = 60;
};

// The mob the stock trees steer. Movement is expressed in blocks per tick so
// the whole simulation stays integer-tick driven and reproducible.
struct BehaviorAgent {
    Species species = Species::Zombie;
    Vec3 position;
    Vec3 homePosition;
    Vec3 heading{1.0f, 0.0f, 0.0f};
    float health = 20.0f;
    float maxHealth = 20.0f;
    float moveStep = 0.25f;
    int grazeTicks = 0;
    int attacksLanded = 0;
    int breedCount = 0;
    int shelterTicks = 0;
    bool sheltered = false;
    std::vector<Vec3i> route;
    int routeCursor = 0;
    long long routePlannedTick = -4096;
    Vec3i routeGoal;
    uint32_t rng = 0x9e3779b9u;

    Vec3i cell() const { return behaviorCellOf(position); }
    bool routeFinished() const { return routeCursor >= static_cast<int>(route.size()); }
};

// Everything a node may read or write. `agent` is null for trees that drive
// something other than a mob (the boss fights capture their own state in the
// node lambdas instead).
struct BehaviorContext {
    const World* world = nullptr;
    BehaviorAgent* agent = nullptr;
    Blackboard blackboard;
    BehaviorTuning tuning;
    PathConfig path;

    long long tick = 0;
    uint32_t rng = 0x2545f491u;

    Vec3 playerPosition;
    bool playerPresent = false;
    ItemId playerHeldItem = ItemId::None;

    Vec3 matePosition;
    bool matePresent = false;

    int skyLight = 15;
    bool daylight = true;

    bool traceEnabled = false;
    std::vector<std::string> trace;
};

using BehaviorPredicate = std::function<bool(BehaviorContext&)>;
using BehaviorAction = std::function<BehaviorStatus(BehaviorContext&)>;

// Base node. `tick` and `reset` are the non-virtual entry points: they keep the
// trace and the cached verdict honest, then hand over to the subclass hooks.
class BehaviorNode {
public:
    explicit BehaviorNode(std::string nodeName);
    virtual ~BehaviorNode();

    BehaviorNode(const BehaviorNode&) = delete;
    BehaviorNode& operator=(const BehaviorNode&) = delete;

    BehaviorStatus tick(BehaviorContext& ctx);
    void reset();

    const std::string& name() const { return nodeName; }
    BehaviorStatus lastStatus() const { return cachedStatus; }
    long long visitCount() const { return visits; }

    virtual int childCount() const { return 0; }
    virtual BehaviorNode* childAt(int index) const;

protected:
    virtual BehaviorStatus onTick(BehaviorContext& ctx) = 0;
    virtual void onReset() {}

private:
    std::string nodeName;
    BehaviorStatus cachedStatus = BehaviorStatus::Failure;
    long long visits = 0;
};

class ConditionNode : public BehaviorNode {
public:
    ConditionNode(std::string nodeName, BehaviorPredicate predicate);

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;

private:
    BehaviorPredicate predicate;
};

class ActionNode : public BehaviorNode {
public:
    ActionNode(std::string nodeName, BehaviorAction action);

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;

private:
    BehaviorAction action;
};

class BehaviorComposite : public BehaviorNode {
public:
    explicit BehaviorComposite(std::string nodeName);

    BehaviorComposite& add(std::unique_ptr<BehaviorNode> child);
    int childCount() const override { return static_cast<int>(children.size()); }
    BehaviorNode* childAt(int index) const override;

protected:
    std::vector<std::unique_ptr<BehaviorNode>> children;
};

// Runs children in order until one succeeds. A child left Running is resumed
// on the next tick; earlier children are not re-evaluated until the selector
// settles on Success or Failure. Settling clears only the composite's own
// cursor - a child's latched state (a cooldown, a repeat counter) survives, so
// only BehaviorTree::reset wipes a whole tree back to its starting state.
class SelectorNode : public BehaviorComposite {
public:
    explicit SelectorNode(std::string nodeName);
    int resumeIndex() const { return runningChild; }

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;
    void onReset() override;

private:
    int runningChild = 0;
};

// Runs children in order until one fails. Same resumption rule as the selector.
class SequenceNode : public BehaviorComposite {
public:
    explicit SequenceNode(std::string nodeName);
    int resumeIndex() const { return runningChild; }

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;
    void onReset() override;

private:
    int runningChild = 0;
};

// Ticks every unfinished child each tick and latches the ones that settled.
// Succeeds once `successThreshold` children have succeeded, fails once
// `failureThreshold` have failed.
class ParallelNode : public BehaviorComposite {
public:
    ParallelNode(std::string nodeName, int successThreshold, int failureThreshold);

    int successesSoFar() const;
    int failuresSoFar() const;

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;
    void onReset() override;

private:
    int successThreshold;
    int failureThreshold;
    std::vector<BehaviorStatus> latched;
    std::vector<bool> settled;
};

class DecoratorNode : public BehaviorNode {
public:
    DecoratorNode(std::string nodeName, std::unique_ptr<BehaviorNode> child);

    int childCount() const override { return wrapped ? 1 : 0; }
    BehaviorNode* childAt(int index) const override;

protected:
    BehaviorNode& child() const { return *wrapped; }

private:
    std::unique_ptr<BehaviorNode> wrapped;
};

class InverterNode : public DecoratorNode {
public:
    InverterNode(std::string nodeName, std::unique_ptr<BehaviorNode> child);

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;
};

// Ticks the child once per tick and counts completions. Succeeds after
// `repeatLimit` completions; with `stopOnFailure` a failing child ends the
// repeat immediately.
class RepeaterNode : public DecoratorNode {
public:
    RepeaterNode(std::string nodeName, std::unique_ptr<BehaviorNode> child, int repeatLimit,
                 bool stopOnFailure = false);

    int completions() const { return completed; }

protected:
    BehaviorStatus onTick(BehaviorContext& ctx) override;
    void onReset() override;

private:
    int repeatLimit;
    bool stopOnFailure;
    int completed = 0;
};

