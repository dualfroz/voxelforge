#include "vf/Behavior.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace vf {

namespace {

constexpr int shadeProbeHeight = 8;

bool cellIsShaded(const World& world, const Vec3i& cell) {
    for (int y = cell.y + 1; y <= cell.y + shadeProbeHeight; ++y) {
        if (world.solidAt(cell.x, y, cell.z)) return true;
    }
    return false;
}

Vec3 horizontalHeading(uint32_t& rng) {
    float angle = behaviorUnitFloat(rng) * 6.2831853f;
    return {std::cos(angle), 0.0f, std::sin(angle)};
}

const SpeciesStats& statsOf(const BehaviorContext& ctx) {
    return speciesStats(ctx.agent->species);
}

std::unique_ptr<BehaviorNode> condition(const char* name, BehaviorPredicate predicate) {
    return std::unique_ptr<BehaviorNode>(new ConditionNode(name, std::move(predicate)));
}

std::unique_ptr<BehaviorNode> action(const char* name, BehaviorAction fn) {
    return std::unique_ptr<BehaviorNode>(new ActionNode(name, std::move(fn)));
}

BehaviorStatus followRouteAction(BehaviorContext& ctx) { return followRouteStep(ctx); }

}

const char* behaviorStatusName(BehaviorStatus status) {
    switch (status) {
        case BehaviorStatus::Failure: return "failure";
        case BehaviorStatus::Success: return "success";
        case BehaviorStatus::Running: return "running";
    }
    return "unknown";
}

uint32_t behaviorNextRandom(uint32_t& state) {
    if (state == 0u) state = 0x9e3779b9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float behaviorUnitFloat(uint32_t& state) {
    return static_cast<float>(behaviorNextRandom(state)) / 4294967296.0f;
}

int behaviorRandomRange(uint32_t& state, int minInclusive, int maxInclusive) {
    if (maxInclusive <= minInclusive) return minInclusive;
    uint32_t span = static_cast<uint32_t>(maxInclusive - minInclusive + 1);
    return minInclusive + static_cast<int>(behaviorNextRandom(state) % span);
}

Vec3i behaviorCellOf(const Vec3& position) {
    return {static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)),
            static_cast<int>(std::floor(position.z))};
}

Vec3 behaviorCellCenter(const Vec3i& cell) {
    return {static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y),
            static_cast<float>(cell.z) + 0.5f};
}

float behaviorHorizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

void Blackboard::setNumber(const std::string& key, float value) { numbers[key] = value; }
void Blackboard::setCount(const std::string& key, int value) { counts[key] = value; }
void Blackboard::setFlag(const std::string& key, bool value) { flags[key] = value; }
void Blackboard::setPoint(const std::string& key, const Vec3& value) { points[key] = value; }
void Blackboard::setCell(const std::string& key, const Vec3i& value) { cells[key] = value; }

void Blackboard::setRoute(const std::string& key, std::vector<Vec3i> cellList) {
    routes[key] = std::move(cellList);
}

bool Blackboard::hasNumber(const std::string& key) const { return numbers.count(key) != 0; }
bool Blackboard::hasCount(const std::string& key) const { return counts.count(key) != 0; }
bool Blackboard::hasFlag(const std::string& key) const { return flags.count(key) != 0; }
bool Blackboard::hasPoint(const std::string& key) const { return points.count(key) != 0; }
bool Blackboard::hasCell(const std::string& key) const { return cells.count(key) != 0; }
bool Blackboard::hasRoute(const std::string& key) const { return routes.count(key) != 0; }

float Blackboard::number(const std::string& key, float fallback) const {
    auto found = numbers.find(key);
    return found == numbers.end() ? fallback : found->second;
}

int Blackboard::count(const std::string& key, int fallback) const {
    auto found = counts.find(key);
    return found == counts.end() ? fallback : found->second;
}

bool Blackboard::flag(const std::string& key, bool fallback) const {
    auto found = flags.find(key);
    return found == flags.end() ? fallback : found->second;
}

Vec3 Blackboard::point(const std::string& key, const Vec3& fallback) const {
    auto found = points.find(key);
    return found == points.end() ? fallback : found->second;
}

Vec3i Blackboard::cell(const std::string& key, const Vec3i& fallback) const {
    auto found = cells.find(key);
    return found == cells.end() ? fallback : found->second;
}

const std::vector<Vec3i>* Blackboard::route(const std::string& key) const {
    auto found = routes.find(key);
    return found == routes.end() ? nullptr : &found->second;
}

void Blackboard::eraseNumber(const std::string& key) { numbers.erase(key); }
void Blackboard::eraseRoute(const std::string& key) { routes.erase(key); }

void Blackboard::clear() {
    numbers.clear();
    counts.clear();
    flags.clear();
    points.clear();
    cells.clear();
    routes.clear();
}

std::size_t Blackboard::entryCount() const {
    return numbers.size() + counts.size() + flags.size() + points.size() + cells.size() +
           routes.size();
}

BehaviorNode::BehaviorNode(std::string name) : nodeName(std::move(name)) {}
BehaviorNode::~BehaviorNode() = default;

BehaviorNode* BehaviorNode::childAt(int index) const {
    (void)index;
    return nullptr;
}

BehaviorStatus BehaviorNode::tick(BehaviorContext& ctx) {
    ++visits;
    if (ctx.traceEnabled) ctx.trace.push_back(nodeName);
    cachedStatus = onTick(ctx);
    return cachedStatus;
}

void BehaviorNode::reset() {
    std::vector<BehaviorNode*> pending;
    pending.push_back(this);
    while (!pending.empty()) {
        BehaviorNode* node = pending.back();
        pending.pop_back();
        node->cachedStatus = BehaviorStatus::Failure;
        node->onReset();
        for (int i = 0; i < node->childCount(); ++i) pending.push_back(node->childAt(i));
    }
}

ConditionNode::ConditionNode(std::string name, BehaviorPredicate test)
    : BehaviorNode(std::move(name)), predicate(std::move(test)) {}

BehaviorStatus ConditionNode::onTick(BehaviorContext& ctx) {
    return predicate(ctx) ? BehaviorStatus::Success : BehaviorStatus::Failure;
}

ActionNode::ActionNode(std::string name, BehaviorAction fn)
    : BehaviorNode(std::move(name)), action(std::move(fn)) {}

BehaviorStatus ActionNode::onTick(BehaviorContext& ctx) { return action(ctx); }

BehaviorComposite::BehaviorComposite(std::string name) : BehaviorNode(std::move(name)) {}

BehaviorComposite& BehaviorComposite::add(std::unique_ptr<BehaviorNode> child) {
    children.push_back(std::move(child));
    return *this;
}

BehaviorNode* BehaviorComposite::childAt(int index) const {
    return children[static_cast<std::size_t>(index)].get();
}

SelectorNode::SelectorNode(std::string name) : BehaviorComposite(std::move(name)) {}

BehaviorStatus SelectorNode::onTick(BehaviorContext& ctx) {
    int total = childCount();
    for (int i = runningChild; i < total; ++i) {
        BehaviorStatus status = children[static_cast<std::size_t>(i)]->tick(ctx);
        if (status == BehaviorStatus::Running) {
            runningChild = i;
            return BehaviorStatus::Running;
        }
        if (status == BehaviorStatus::Success) {
            runningChild = 0;
            return BehaviorStatus::Success;
        }
    }
    runningChild = 0;
    return BehaviorStatus::Failure;
}

void SelectorNode::onReset() { runningChild = 0; }

SequenceNode::SequenceNode(std::string name) : BehaviorComposite(std::move(name)) {}

BehaviorStatus SequenceNode::onTick(BehaviorContext& ctx) {
    int total = childCount();
    for (int i = runningChild; i < total; ++i) {
        BehaviorStatus status = children[static_cast<std::size_t>(i)]->tick(ctx);
        if (status == BehaviorStatus::Running) {
            runningChild = i;
            return BehaviorStatus::Running;
        }
        if (status == BehaviorStatus::Failure) {
            runningChild = 0;
            return BehaviorStatus::Failure;
        }
    }
    runningChild = 0;
    return BehaviorStatus::Success;
}

void SequenceNode::onReset() { runningChild = 0; }

ParallelNode::ParallelNode(std::string name, int successes, int failures)
    : BehaviorComposite(std::move(name)),
      successThreshold(successes),
      failureThreshold(failures) {}

int ParallelNode::successesSoFar() const {
    int total = 0;
    for (std::size_t i = 0; i < latched.size(); ++i) {
        if (settled[i] && latched[i] == BehaviorStatus::Success) ++total;
    }
    return total;
}

int ParallelNode::failuresSoFar() const {
    int total = 0;
    for (std::size_t i = 0; i < latched.size(); ++i) {
        if (settled[i] && latched[i] == BehaviorStatus::Failure) ++total;
    }
    return total;
}

BehaviorStatus ParallelNode::onTick(BehaviorContext& ctx) {
    std::size_t total = children.size();
    if (latched.size() != total) {
        latched.assign(total, BehaviorStatus::Running);
        settled.assign(total, false);
    }

    for (std::size_t i = 0; i < total; ++i) {
        if (settled[i]) continue;
        BehaviorStatus status = children[i]->tick(ctx);
        if (status == BehaviorStatus::Running) continue;
        latched[i] = status;
        settled[i] = true;
    }

    int successes = successesSoFar();
    int failures = failuresSoFar();
    if (successes >= successThreshold || failures >= failureThreshold) {
        for (std::size_t i = 0; i < total; ++i) {
            if (!settled[i]) children[i]->reset();
        }
        latched.assign(total, BehaviorStatus::Running);
        settled.assign(total, false);
        return successes >= successThreshold ? BehaviorStatus::Success : BehaviorStatus::Failure;
    }
    return BehaviorStatus::Running;
}

void ParallelNode::onReset() {
    latched.assign(children.size(), BehaviorStatus::Running);
    settled.assign(children.size(), false);
}

DecoratorNode::DecoratorNode(std::string name, std::unique_ptr<BehaviorNode> child)
    : BehaviorNode(std::move(name)), wrapped(std::move(child)) {}

BehaviorNode* DecoratorNode::childAt(int index) const {
    (void)index;
    return wrapped.get();
}

InverterNode::InverterNode(std::string name, std::unique_ptr<BehaviorNode> child)
    : DecoratorNode(std::move(name), std::move(child)) {}

BehaviorStatus InverterNode::onTick(BehaviorContext& ctx) {
    BehaviorStatus status = child().tick(ctx);
    if (status == BehaviorStatus::Success) return BehaviorStatus::Failure;
    if (status == BehaviorStatus::Failure) return BehaviorStatus::Success;
    return BehaviorStatus::Running;
}

RepeaterNode::RepeaterNode(std::string name, std::unique_ptr<BehaviorNode> child, int limit,
                           bool stopWhenChildFails)
    : DecoratorNode(std::move(name), std::move(child)),
      repeatLimit(limit),
      stopOnFailure(stopWhenChildFails) {}

BehaviorStatus RepeaterNode::onTick(BehaviorContext& ctx) {
    BehaviorStatus status = child().tick(ctx);
    if (status == BehaviorStatus::Running) return BehaviorStatus::Running;
    if (status == BehaviorStatus::Failure && stopOnFailure) {
        completed = 0;
        return BehaviorStatus::Failure;
    }
    ++completed;
    if (completed >= repeatLimit) {
        completed = 0;
        return BehaviorStatus::Success;
    }
    return BehaviorStatus::Running;
}

void RepeaterNode::onReset() { completed = 0; }

RetryUntilSuccessNode::RetryUntilSuccessNode(std::string name, std::unique_ptr<BehaviorNode> child,
                                             int attemptLimit)
    : DecoratorNode(std::move(name), std::move(child)), maxAttempts(attemptLimit) {}

BehaviorStatus RetryUntilSuccessNode::onTick(BehaviorContext& ctx) {
    BehaviorStatus status = child().tick(ctx);
    if (status == BehaviorStatus::Running) return BehaviorStatus::Running;
    if (status == BehaviorStatus::Success) {
        attempts = 0;
        return BehaviorStatus::Success;
    }
    ++attempts;
    if (attempts >= maxAttempts) {
        attempts = 0;
        return BehaviorStatus::Failure;
    }
    child().reset();
    return BehaviorStatus::Running;
}

void RetryUntilSuccessNode::onReset() { attempts = 0; }

CooldownNode::CooldownNode(std::string name, std::unique_ptr<BehaviorNode> child, int ticks)
    : DecoratorNode(std::move(name), std::move(child)), cooldownTicks(ticks) {}

BehaviorStatus CooldownNode::onTick(BehaviorContext& ctx) {
    if (remaining > 0) {
        --remaining;
        return BehaviorStatus::Failure;
    }
    BehaviorStatus status = child().tick(ctx);
    if (status != BehaviorStatus::Running) remaining = cooldownTicks;
    return status;
}

void CooldownNode::onReset() { remaining = 0; }

TimeLimitNode::TimeLimitNode(std::string name, std::unique_ptr<BehaviorNode> child, int ticks)
    : DecoratorNode(std::move(name), std::move(child)), limitTicks(ticks) {}

BehaviorStatus TimeLimitNode::onTick(BehaviorContext& ctx) {
    ++elapsed;
    BehaviorStatus status = child().tick(ctx);
    if (status != BehaviorStatus::Running) {
        elapsed = 0;
        return status;
    }
    if (elapsed >= limitTicks) {
        elapsed = 0;
        child().reset();
        return BehaviorStatus::Failure;
    }
    return BehaviorStatus::Running;
}

void TimeLimitNode::onReset() { elapsed = 0; }

BehaviorTreeStats measureBehaviorTree(const BehaviorNode* root) {
    BehaviorTreeStats stats;
    if (root == nullptr) return stats;

    std::vector<std::pair<const BehaviorNode*, int>> pending;
    pending.push_back({root, 1});
    while (!pending.empty()) {
        std::pair<const BehaviorNode*, int> entry = pending.back();
        pending.pop_back();
        ++stats.nodeCount;
        if (entry.second > stats.depth) stats.depth = entry.second;
        for (int i = 0; i < entry.first->childCount(); ++i) {
            pending.push_back({entry.first->childAt(i), entry.second + 1});
        }
    }
    stats.withinDepthLimit = stats.depth <= maxBehaviorDepth;
    return stats;
}

BehaviorTree::BehaviorTree(std::unique_ptr<BehaviorNode> newRoot) { setRoot(std::move(newRoot)); }

void BehaviorTree::setRoot(std::unique_ptr<BehaviorNode> newRoot) {
    rootNode = std::move(newRoot);
    treeStats = measureBehaviorTree(rootNode.get());
    ticks = 0;
}

BehaviorStatus BehaviorTree::tick(BehaviorContext& ctx) {
    if (!structureValid()) return BehaviorStatus::Failure;
    ++ticks;
    ++ctx.tick;
    return rootNode->tick(ctx);
}

void BehaviorTree::reset() {
    if (rootNode) rootNode->reset();
    ticks = 0;
}

const char* behaviorRoleName(BehaviorRole role) {
    switch (role) {
        case BehaviorRole::HostileHunter: return "hostileHunter";
        case BehaviorRole::GrazingAnimal: return "grazingAnimal";
        case BehaviorRole::SunAvoider: return "sunAvoider";
        case BehaviorRole::Count: break;
    }
    return "unknown";
}

BehaviorRole roleForSpecies(Species species) {
    switch (species) {
        case Species::Zombie:
        case Species::Creeper:
            return BehaviorRole::HostileHunter;
        case Species::Skeleton:
            return BehaviorRole::SunAvoider;
        default:
            return BehaviorRole::GrazingAnimal;
    }
}

ItemId breedingFoodFor(Species species) {
    switch (species) {
        case Species::Sheep:
        case Species::Cow:
            return ItemId::Bread;
        case Species::Pig:
            return ItemId::Apple;
        default:
            return ItemId::None;
    }
}

bool isBreedingFood(Species species, ItemId held) {
    ItemId wanted = breedingFoodFor(species);
    return wanted != ItemId::None && held == wanted;
}

bool stepTowardPoint(BehaviorAgent& agent, const Vec3& goal, float arriveEpsilon) {
    Vec3 delta = goal - agent.position;
    float distance = length(delta);
    if (distance <= arriveEpsilon) {
        agent.position = goal;
        return true;
    }
    Vec3 direction = delta * (1.0f / distance);
    Vec3 flat{direction.x, 0.0f, direction.z};
    if (length(flat) > 1e-4f) agent.heading = normalize(flat);
    if (agent.moveStep >= distance) {
        agent.position = goal;
        return true;
    }
    agent.position += direction * agent.moveStep;
    return false;
}

bool planRouteTo(BehaviorContext& ctx, const Vec3i& goal, bool force) {
    BehaviorAgent& agent = *ctx.agent;
    bool sameGoal = agent.routeGoal == goal;
    bool fresh = (ctx.tick - agent.routePlannedTick) < ctx.tuning.repathIntervalTicks;
    if (!force && sameGoal && fresh && agent.route.size() >= 2) return true;

    agent.route = findPath(*ctx.world, agent.cell(), goal, ctx.path);
    agent.routeCursor = agent.route.empty() ? 0 : 1;
    agent.routeGoal = goal;
    agent.routePlannedTick = ctx.tick;
    return agent.route.size() >= 2;
}

BehaviorStatus followRouteStep(BehaviorContext& ctx) {
    BehaviorAgent& agent = *ctx.agent;
    if (agent.routeFinished()) return BehaviorStatus::Success;

    Vec3i next = agent.route[static_cast<std::size_t>(agent.routeCursor)];
    if (stepTowardPoint(agent, behaviorCellCenter(next), ctx.tuning.arriveEpsilon)) {
        ++agent.routeCursor;
    }
    return agent.routeFinished() ? BehaviorStatus::Success : BehaviorStatus::Running;
}

bool findShadedCell(const World& world, const Vec3i& from, int radius, const PathConfig& config,
                    Vec3i& outCell) {
    int best = -1;
    Vec3i bestCell;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dy = -2; dy <= 2; ++dy) {
                Vec3i probe{from.x + dx, from.y + dy, from.z + dz};
                if (!cellIsShaded(world, probe)) continue;
                if (!isStandable(world, probe, config)) continue;
                int cost = dx * dx + dz * dz + dy * dy * 4;
                if (best >= 0 && cost >= best) continue;
                best = cost;
                bestCell = probe;
            }
        }
    }
    if (best < 0) return false;
    outCell = bestCell;
    return true;
}

BehaviorTree makeHostileHunterTree(const BehaviorTuning& tuning) {
    auto strike = std::unique_ptr<SequenceNode>(new SequenceNode("strike"));
    strike->add(condition("withinAttackReach", [](BehaviorContext& ctx) {
        float reach = statsOf(ctx).attackRange;
        return behaviorHorizontalDistance(ctx.agent->position, ctx.playerPosition) <= reach;
    }));
    strike->add(std::unique_ptr<BehaviorNode>(new CooldownNode(
        "attackCooldown",
        action("attack",
               [](BehaviorContext& ctx) {
                   ++ctx.agent->attacksLanded;
                   ctx.blackboard.setNumber("lastAttackDamage", statsOf(ctx).attackDamage);
                   return BehaviorStatus::Success;
               }),
        tuning.attackCooldownTicks)));

    auto approach = std::unique_ptr<SequenceNode>(new SequenceNode("approach"));
    approach->add(action("planRouteToTarget", [](BehaviorContext& ctx) {
        Vec3i goal = ctx.blackboard.cell("targetCell", behaviorCellOf(ctx.playerPosition));
        return planRouteTo(ctx, goal, false) ? BehaviorStatus::Success : BehaviorStatus::Failure;
    }));
    approach->add(action("followRoute", followRouteAction));

    auto closeIn = std::unique_ptr<SelectorNode>(new SelectorNode("strikeOrApproach"));
    closeIn->add(std::move(strike));
    closeIn->add(std::move(approach));

    auto engage = std::unique_ptr<SequenceNode>(new SequenceNode("engageTarget"));
    engage->add(condition("targetInRange", [](BehaviorContext& ctx) {
        if (!ctx.playerPresent) {
            ctx.blackboard.setFlag("engaged", false);
            return false;
        }
        const SpeciesStats& stats = statsOf(ctx);
        bool engaged = ctx.blackboard.flag("engaged");
        float radius = engaged ? stats.disengageRadius : stats.alertRadius;
        float distance = behaviorHorizontalDistance(ctx.agent->position, ctx.playerPosition);
        bool inRange = distance <= radius;
        ctx.blackboard.setFlag("engaged", inRange);
        if (inRange) {
            ctx.blackboard.setPoint("lastSeen", ctx.playerPosition);
            ctx.blackboard.setCell("targetCell", behaviorCellOf(ctx.playerPosition));
        }
        return inRange;
    }));
    engage->add(std::move(closeIn));

    auto goHome = std::unique_ptr<SequenceNode>(new SequenceNode("returnHome"));
    goHome->add(condition("awayFromHome", [](BehaviorContext& ctx) {
        if (ctx.blackboard.flag("engaged")) return false;
        return behaviorHorizontalDistance(ctx.agent->position, ctx.agent->homePosition) >
               ctx.tuning.homeReturnRadius;
    }));
    goHome->add(action("planRouteHome", [](BehaviorContext& ctx) {
        Vec3i goal = behaviorCellOf(ctx.agent->homePosition);
        return planRouteTo(ctx, goal, false) ? BehaviorStatus::Success : BehaviorStatus::Failure;
    }));
    goHome->add(action("followRoute", followRouteAction));

    auto root = std::unique_ptr<SelectorNode>(new SelectorNode("hostileRoot"));
    root->add(std::move(engage));
    root->add(std::move(goHome));
    root->add(action("idle", [](BehaviorContext&) { return BehaviorStatus::Success; }));

    return BehaviorTree(std::move(root));
}

BehaviorTree makeGrazingAnimalTree(const BehaviorTuning& tuning) {
    auto flee = std::unique_ptr<SequenceNode>(new SequenceNode("flee"));
    flee->add(condition("threatened", [](BehaviorContext& ctx) {
        if (ctx.agent->health < ctx.agent->maxHealth) return true;
        if (!ctx.playerPresent) return false;
        if (isBreedingFood(ctx.agent->species, ctx.playerHeldItem)) return false;
        return behaviorHorizontalDistance(ctx.agent->position, ctx.playerPosition) <
               ctx.tuning.fleeRadius;
    }));
    flee->add(action("fleeStep", [](BehaviorContext& ctx) {
        BehaviorAgent& agent = *ctx.agent;
        Vec3 away{agent.position.x - ctx.playerPosition.x, 0.0f,
                  agent.position.z - ctx.playerPosition.z};
        if (length(away) < 1e-3f) away = horizontalHeading(agent.rng);
        away = normalize(away);
        stepTowardPoint(agent, agent.position + away * 4.0f, ctx.tuning.arriveEpsilon);
        return BehaviorStatus::Running;
    }));

    auto lured = std::unique_ptr<SequenceNode>(new SequenceNode("followHeldFood"));
    lured->add(condition("playerOffersFood", [](BehaviorContext& ctx) {
        if (!ctx.playerPresent) return false;
        if (!isBreedingFood(ctx.agent->species, ctx.playerHeldItem)) return false;
        return behaviorHorizontalDistance(ctx.agent->position, ctx.playerPosition) <=
               ctx.tuning.followFoodRadius;
    }));
    lured->add(action("stepTowardPlayer", [](BehaviorContext& ctx) {
        BehaviorAgent& agent = *ctx.agent;
        float distance = behaviorHorizontalDistance(agent.position, ctx.playerPosition);
        if (distance <= ctx.tuning.breedRadius) {
            ctx.blackboard.setFlag("inLove", true);
            return BehaviorStatus::Success;
        }
        stepTowardPoint(agent, ctx.playerPosition, ctx.tuning.arriveEpsilon);
        return BehaviorStatus::Running;
    }));

    auto breed = std::unique_ptr<SequenceNode>(new SequenceNode("breed"));
    breed->add(condition("mateReady", [](BehaviorContext& ctx) {
        if (!ctx.matePresent) return false;
        if (!ctx.blackboard.flag("inLove")) return false;
        return behaviorHorizontalDistance(ctx.agent->position, ctx.matePosition) <=
               ctx.tuning.breedRadius;
    }));
    breed->add(std::unique_ptr<BehaviorNode>(new CooldownNode(
        "breedCooldown",
        action("breed",
               [](BehaviorContext& ctx) {
                   ++ctx.agent->breedCount;
                   ctx.blackboard.setFlag("inLove", false);
                   ctx.blackboard.setCount("lastBreedTick", static_cast<int>(ctx.tick));
                   return BehaviorStatus::Success;
               }),
        tuning.breedCooldownTicks)));

    auto graze = std::unique_ptr<SequenceNode>(new SequenceNode("graze"));
    graze->add(condition("standingOnGrass", [](BehaviorContext& ctx) {
        Vec3i below = ctx.agent->cell();
        return ctx.world->blockAt(below.x, below.y - 1, below.z) == Block::Grass;
    }));
    graze->add(std::unique_ptr<BehaviorNode>(new TimeLimitNode(
        "grazeWindow",
        action("chew",
               [](BehaviorContext& ctx) {
                   BehaviorAgent& agent = *ctx.agent;
                   ++agent.grazeTicks;
                   if (agent.grazeTicks < ctx.tuning.grazeDurationTicks)
                       return BehaviorStatus::Running;
                   agent.grazeTicks = 0;
                   ctx.blackboard.setCount("meals", ctx.blackboard.count("meals") + 1);
                   return BehaviorStatus::Success;
               }),
        tuning.grazeDurationTicks * 5)));

    auto root = std::unique_ptr<SelectorNode>(new SelectorNode("animalRoot"));
    root->add(std::move(flee));
    root->add(std::move(breed));
    root->add(std::move(lured));
    root->add(std::move(graze));
    root->add(action("wander", [](BehaviorContext& ctx) {
        BehaviorAgent& agent = *ctx.agent;
        if (length(agent.heading) < 1e-3f || behaviorNextRandom(agent.rng) % 32u == 0u)
            agent.heading = horizontalHeading(agent.rng);
        stepTowardPoint(agent, agent.position + agent.heading * 2.0f, ctx.tuning.arriveEpsilon);
        return BehaviorStatus::Success;
    }));

    return BehaviorTree(std::move(root));
}

BehaviorTree makeSunAvoiderTree(const BehaviorTuning& tuning) {
    auto seekShade = std::unique_ptr<SequenceNode>(new SequenceNode("seekShade"));
    seekShade->add(condition("exposedToSun", [](BehaviorContext& ctx) {
        BehaviorAgent& agent = *ctx.agent;
        agent.sheltered = cellIsShaded(*ctx.world, agent.cell());
        if (!ctx.daylight) return false;
        if (ctx.skyLight < ctx.tuning.sunLightThreshold) return false;
        return !agent.sheltered;
    }));
    seekShade->add(action("findShade", [](BehaviorContext& ctx) {
        Vec3i shade;
        if (!findShadedCell(*ctx.world, ctx.agent->cell(), ctx.tuning.shadeSearchRadius, ctx.path,
                            shade)) {
            return BehaviorStatus::Failure;
        }
        ctx.blackboard.setCell("shadeCell", shade);
        return BehaviorStatus::Success;
    }));
    seekShade->add(std::unique_ptr<BehaviorNode>(new TimeLimitNode(
        "shadeRun",
        action("walkToShade",
               [](BehaviorContext& ctx) {
                   Vec3i shade = ctx.blackboard.cell("shadeCell");
                   if (!planRouteTo(ctx, shade, false)) {
                       ctx.agent->sheltered = cellIsShaded(*ctx.world, ctx.agent->cell());
                       return ctx.agent->sheltered ? BehaviorStatus::Success
                                                   : BehaviorStatus::Failure;
                   }
                   BehaviorStatus status = followRouteStep(ctx);
                   if (status == BehaviorStatus::Success) {
                       ctx.agent->sheltered = cellIsShaded(*ctx.world, ctx.agent->cell());
                   }
                   return status;
               }),
        tuning.shadeRunTicks)));

    auto hold = std::unique_ptr<SequenceNode>(new SequenceNode("holdShelter"));
    hold->add(condition("daylightNow", [](BehaviorContext& ctx) { return ctx.daylight; }));
    hold->add(action("waitOutDay", [](BehaviorContext& ctx) {
        ++ctx.agent->shelterTicks;
        if (ctx.agent->shelterTicks >= ctx.tuning.shelterHoldTicks) return BehaviorStatus::Success;
        return BehaviorStatus::Running;
    }));

    auto root = std::unique_ptr<SelectorNode>(new SelectorNode("sunAvoiderRoot"));
    root->add(std::move(seekShade));
    root->add(std::move(hold));
    root->add(action("nightPatrol", [](BehaviorContext& ctx) {
        BehaviorAgent& agent = *ctx.agent;
        agent.shelterTicks = 0;
        if (length(agent.heading) < 1e-3f || behaviorNextRandom(agent.rng) % 24u == 0u)
            agent.heading = horizontalHeading(agent.rng);
        stepTowardPoint(agent, agent.position + agent.heading * 2.0f, ctx.tuning.arriveEpsilon);
        return BehaviorStatus::Success;
    }));

    return BehaviorTree(std::move(root));
}

BehaviorTree makeTreeForRole(BehaviorRole role, const BehaviorTuning& tuning) {
    switch (role) {
        case BehaviorRole::HostileHunter: return makeHostileHunterTree(tuning);
        case BehaviorRole::SunAvoider: return makeSunAvoiderTree(tuning);
        default: return makeGrazingAnimalTree(tuning);
    }
}

}
