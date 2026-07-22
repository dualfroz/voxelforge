#include "vf/Boss.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace vf {

namespace {

constexpr float fullTurn = 6.2831853f;

float distanceBetween(const Vec3& a, const Vec3& b) { return length(a - b); }

std::unique_ptr<BehaviorNode> cond(const char* name, BehaviorPredicate predicate) {
    return std::unique_ptr<BehaviorNode>(new ConditionNode(name, std::move(predicate)));
}

std::unique_ptr<BehaviorNode> act(const char* name, BehaviorAction fn) {
    return std::unique_ptr<BehaviorNode>(new ActionNode(name, std::move(fn)));
}

std::unique_ptr<SequenceNode> seq(const char* name) {
    return std::unique_ptr<SequenceNode>(new SequenceNode(name));
}

std::unique_ptr<SelectorNode> sel(const char* name) {
    return std::unique_ptr<SelectorNode>(new SelectorNode(name));
}

std::unique_ptr<BehaviorNode> gated(const char* name, std::unique_ptr<BehaviorNode> child,
                                    int ticks) {
    return std::unique_ptr<BehaviorNode>(new CooldownNode(name, std::move(child), ticks));
}

std::unique_ptr<BehaviorNode> bounded(const char* name, std::unique_ptr<BehaviorNode> child,
                                      int ticks) {
    return std::unique_ptr<BehaviorNode>(new TimeLimitNode(name, std::move(child), ticks));
}

Vec3 crystalCenter(const Vec3i& cell) {
    return {static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y) + 0.5f,
            static_cast<float>(cell.z) + 0.5f};
}

Vec3i witherOffset(const Vec3i& base, int along, int up, bool alongX) {
    return alongX ? Vec3i{base.x + along, base.y + up, base.z}
                  : Vec3i{base.x, base.y + up, base.z + along};
}

} // namespace

const char* bossKindName(BossKind kind) {
    switch (kind) {
        case BossKind::EnderDragon: return "enderDragon";
        case BossKind::Wither: return "wither";
        case BossKind::Count: break;
    }
    return "unknown";
}

const char* bossPhaseName(BossPhase phase) {
    switch (phase) {
        case BossPhase::Dormant: return "dormant";
        case BossPhase::Circling: return "circling";
        case BossPhase::Charging: return "charging";
        case BossPhase::Breathing: return "breathing";
        case BossPhase::Perching: return "perching";
        case BossPhase::Summoning: return "summoning";
        case BossPhase::Explosive: return "explosive";
        case BossPhase::Ranged: return "ranged";
        case BossPhase::Armored: return "armored";
        case BossPhase::Dying: return "dying";
        case BossPhase::Defeated: return "defeated";
        case BossPhase::Count: break;
    }
    return "unknown";
}

int BossBar::notchCount() const {
    switch (style) {
        case BossBarStyle::Solid: return 1;
        case BossBarStyle::Notched6: return 6;
        case BossBarStyle::Notched10: return 10;
        case BossBarStyle::Notched12: return 12;
        case BossBarStyle::Notched20: return 20;
        case BossBarStyle::Count: break;
    }
    return 1;
}

int BossBar::filledNotches() const {
    float exact = progress * static_cast<float>(notchCount());
    int whole = static_cast<int>(std::floor(exact + 1e-4f));
    return std::max(0, std::min(notchCount(), whole));
}

int BossBar::percent() const {
    return static_cast<int>(std::floor(progress * 100.0f + 0.5f));
}

void BossBar::setFromHealth(float health, float maxHealth) {
    if (maxHealth <= 0.0f) {
        progress = 0.0f;
        return;
    }
    float fraction = health / maxHealth;
    progress = std::max(0.0f, std::min(1.0f, fraction));
}

void BossState::enterPhase(BossPhase next) {
    if (phase == next) return;
    phase = next;
    phaseTicks = 0;
    ++phaseChanges;
}

float BossState::healthFraction() const {
    if (maxHealth <= 0.0f) return 0.0f;
    return health / maxHealth;
}

std::vector<CrystalNode> crystalsFromPillars(const std::vector<ObsidianPillar>& pillars) {
    std::vector<CrystalNode> nodes;
    nodes.reserve(pillars.size());
    for (const ObsidianPillar& pillar : pillars) {
        CrystalNode node;
        node.pillarBase = pillar.base;
        node.pillarHeight = pillar.height;
        node.position = {pillar.base.x, pillar.base.y + pillar.height + 1, pillar.base.z};
        node.caged = pillar.crowned;
        node.cageIntact = pillar.crowned;
        nodes.push_back(node);
    }
    return nodes;
}

DragonFight::DragonFight(const EndGenerator& end, uint32_t seed, const DragonFightConfig& config)
    : cfg(config) {
    boss.kind = BossKind::EnderDragon;
    boss.maxHealth = cfg.maxHealth;
    boss.health = cfg.maxHealth;
    boss.rng = seed != 0u ? seed : 0x51ed270bu;
    boss.phase = BossPhase::Circling;
    boss.bar.title = "Ender Dragon";
    boss.bar.color = BossBarColor::Pink;
    boss.bar.style = BossBarStyle::Notched10;
    boss.bar.visible = true;
    boss.bar.setFromHealth(boss.health, boss.maxHealth);

    pillarCrystals = crystalsFromPillars(end.pillars());

    int surface = end.surfaceHeight(0, 0);
    portal = {0.5f, static_cast<float>(surface), 0.5f};
    eggCell = {0, surface + 1, 0};

    boss.position = portal + Vec3{cfg.orbitRadius, cfg.orbitHeight, 0.0f};
    ctx.rng = boss.rng;
    buildTree();
}

int DragonFight::aliveCrystalCount() const {
    int total = 0;
    for (const CrystalNode& crystal : pillarCrystals) {
        if (crystal.alive) ++total;
    }
    return total;
}

void DragonFight::buildTree() {
    auto death = seq("deathThroes");
    death->add(cond("healthGone", [this](BehaviorContext&) { return boss.health <= 0.0f; }));
    death->add(act("collapse", [this](BehaviorContext&) {
        if (boss.phase != BossPhase::Dying) {
            boss.enterPhase(BossPhase::Dying);
            return BehaviorStatus::Running;
        }
        if (boss.phaseTicks < cfg.deathTicks) return BehaviorStatus::Running;
        boss.enterPhase(BossPhase::Defeated);
        boss.alive = false;
        boss.bar.visible = false;
        boss.bar.setFromHealth(0.0f, boss.maxHealth);
        portalOpen = true;
        eggDropped = true;
        return BehaviorStatus::Success;
    }));

    auto volley = std::unique_ptr<ParallelNode>(new ParallelNode("perchAssault", 1, 2));
    volley->add(gated("breathCooldown",
                      act("breatheAcid",
                          [this](BehaviorContext& context) {
                              if (!context.playerPresent) return BehaviorStatus::Failure;
                              if (distanceBetween(boss.position, context.playerPosition) >
                                  cfg.breathRange) {
                                  return BehaviorStatus::Failure;
                              }
                              damageThisTick += cfg.breathDamage;
                              ++boss.attacksLaunched;
                              return BehaviorStatus::Success;
                          }),
                      cfg.breathCooldownTicks));
    volley->add(gated("swipeCooldown",
                      act("tailSwipe",
                          [this](BehaviorContext& context) {
                              if (!context.playerPresent) return BehaviorStatus::Failure;
                              if (distanceBetween(boss.position, context.playerPosition) >
                                  cfg.swipeRange) {
                                  return BehaviorStatus::Failure;
                              }
                              damageThisTick += cfg.swipeDamage;
                              ++boss.attacksLaunched;
                              return BehaviorStatus::Success;
                          }),
                      cfg.swipeCooldownTicks));

    auto perchAction = sel("perchAction");
    perchAction->add(std::move(volley));
    perchAction->add(act("holdPerch", [](BehaviorContext&) { return BehaviorStatus::Success; }));

    auto perch = seq("perchAtPortal");
    perch->add(cond("allCrystalsDown",
                    [this](BehaviorContext&) { return aliveCrystalCount() == 0; }));
    perch->add(act("descendToPortal", [this](BehaviorContext&) {
        boss.enterPhase(BossPhase::Perching);
        Vec3 landing = portal + Vec3{0.0f, cfg.perchHeight, 0.0f};
        Vec3 delta = landing - boss.position;
        float span = length(delta);
        if (span <= cfg.perchDescentStep) {
            boss.position = landing;
            return BehaviorStatus::Success;
        }
        boss.position += delta * (cfg.perchDescentStep / span);
        return BehaviorStatus::Running;
    }));
    perch->add(std::move(perchAction));

    auto charge = seq("chargePlayer");
    charge->add(cond("playerWorthCharging", [this](BehaviorContext& context) {
        return context.playerPresent &&
               distanceBetween(boss.position, context.playerPosition) <= cfg.chargeTrigger;
    }));
    charge->add(gated("chargeCooldown",
                      bounded("chargeWindow",
                              act("charge",
                                  [this](BehaviorContext& context) {
                                      if (!context.playerPresent) return BehaviorStatus::Failure;
                                      boss.enterPhase(BossPhase::Charging);
                                      Vec3 delta = context.playerPosition - boss.position;
                                      float span = length(delta);
                                      if (span <= cfg.chargeContactRange) {
                                          damageThisTick += cfg.chargeDamage;
                                          ++boss.attacksLaunched;
                                          return BehaviorStatus::Success;
                                      }
                                      boss.position += delta * (cfg.chargeSpeed / span);
                                      return BehaviorStatus::Running;
                                  }),
                              cfg.chargeWindowTicks),
                      cfg.chargeCooldownTicks));

    auto breath = seq("acidBreath");
    breath->add(cond("playerNearPortal", [this](BehaviorContext& context) {
        return context.playerPresent &&
               distanceBetween(context.playerPosition, portal) <= cfg.breathRange;
    }));
    breath->add(gated("flightBreathCooldown",
                      act("breatheFromAbove",
                          [this](BehaviorContext&) {
                              boss.enterPhase(BossPhase::Breathing);
                              damageThisTick += cfg.breathDamage;
                              ++boss.attacksLaunched;
                              return BehaviorStatus::Success;
                          }),
                      cfg.breathCooldownTicks));

    auto root = sel("dragonRoot");
    root->add(std::move(death));
    root->add(std::move(perch));
    root->add(std::move(charge));
    root->add(std::move(breath));
    root->add(act("orbit", [this](BehaviorContext&) {
        boss.enterPhase(BossPhase::Circling);
        boss.orbitAngle += cfg.orbitStep;
        if (boss.orbitAngle >= fullTurn) boss.orbitAngle -= fullTurn;
        boss.position = portal + Vec3{std::cos(boss.orbitAngle) * cfg.orbitRadius, cfg.orbitHeight,
                                      std::sin(boss.orbitAngle) * cfg.orbitRadius};
        boss.heading = normalize(Vec3{-std::sin(boss.orbitAngle), 0.0f, std::cos(boss.orbitAngle)});
        return BehaviorStatus::Success;
    }));

    tree.setRoot(std::move(root));
}

int DragonFight::resolvePlayerShot(const DragonPlayerInput& input) {
    if (input.shootCrystal < 0) return 0;
    if (input.shootCrystal >= static_cast<int>(pillarCrystals.size())) return 0;

    CrystalNode& crystal = pillarCrystals[static_cast<std::size_t>(input.shootCrystal)];
    if (!crystal.alive) return 0;
    if (crystal.cageIntact) {
        if (input.breakCage) crystal.cageIntact = false;
        return 0;
    }

    crystal.alive = false;
    crystal.health = 0.0f;
    crystal.destroyedTick = boss.tick;

    if (distanceBetween(crystalCenter(crystal.position), boss.position) <= cfg.crystalBlastRange) {
        float dealt = std::min(cfg.crystalBlastDamage, boss.health);
        boss.health -= dealt;
        boss.damageTaken += dealt;
    }
    return 1;
}

void DragonFight::applyPlayerMelee(const DragonPlayerInput& input) {
    if (!input.present || input.meleeDamage <= 0.0f || !boss.alive) return;
    if (distanceBetween(boss.position, input.position) > cfg.meleeReach) return;

    float factor = boss.phase == BossPhase::Perching ? 1.0f : cfg.flyingDamageFactor;
    float dealt = std::min(input.meleeDamage * factor, boss.health);
    boss.health -= dealt;
    boss.damageTaken += dealt;
}

float DragonFight::applyCrystalHealing() {
    if (boss.health <= 0.0f) return 0.0f;

    float healed = 0.0f;
    for (const CrystalNode& crystal : pillarCrystals) {
        if (!crystal.alive) continue;
        if (distanceBetween(crystalCenter(crystal.position), boss.position) > cfg.crystalHealRange)
            continue;
        float room = boss.maxHealth - boss.health - healed;
        if (room <= 0.0f) break;
        healed += std::min(cfg.crystalHealPerTick, room);
    }
    boss.health += healed;
    boss.healingReceived += healed;
    return healed;
}

BossTickReport DragonFight::tick(const DragonPlayerInput& input) {
    BossTickReport report;
    report.previousPhase = boss.phase;
    report.phase = boss.phase;
    report.bossHealth = boss.health;

    if (boss.phase == BossPhase::Defeated) {
        report.defeated = true;
        return report;
    }

    lastInput = input;
    damageThisTick = 0.0f;

    int crystalsKilled = resolvePlayerShot(input);
    applyPlayerMelee(input);
    float healed = applyCrystalHealing();

    bool lastCrystalDown = crystalsKilled > 0 && aliveCrystalCount() == 0;
    bool lethalBlow = boss.health <= 0.0f && boss.phase != BossPhase::Dying;
    if (lastCrystalDown || lethalBlow) tree.reset();

    ctx.playerPresent = input.present;
    ctx.playerPosition = input.position;

    ++boss.phaseTicks;
    int changesBefore = boss.phaseChanges;
    tree.tick(ctx);
    boss.tick = ctx.tick;
    if (boss.phase != BossPhase::Defeated) boss.bar.setFromHealth(boss.health, boss.maxHealth);

    report.phase = boss.phase;
    report.phaseChanged = boss.phaseChanges != changesBefore;
    report.bossHealth = boss.health;
    report.healedThisTick = healed;
    report.damageToTargets = damageThisTick;
    report.crystalsDestroyed = crystalsKilled;
    report.defeated = boss.phase == BossPhase::Defeated;
    return report;
}

bool DragonFight::placeVictoryStructures(World& world, const EndGenerator& end) const {
    if (!portalOpen) return false;
    end.placeExitPortal(world);
    return true;
}

Block witherBaseBlock() { return Block::SoulSand; }
Block witherHeadBlock() { return Block::Pumpkin; }

bool witherPatternAt(const World& world, const Vec3i& baseCell, bool alongX) {
    if (world.blockAt(baseCell.x, baseCell.y, baseCell.z) != witherBaseBlock()) return false;
    for (int along = -1; along <= 1; ++along) {
        Vec3i arm = witherOffset(baseCell, along, 1, alongX);
        if (world.blockAt(arm.x, arm.y, arm.z) != witherBaseBlock()) return false;
        Vec3i head = witherOffset(baseCell, along, 2, alongX);
        if (world.blockAt(head.x, head.y, head.z) != witherHeadBlock()) return false;
    }
    return true;
}

bool findWitherPattern(const World& world, const Vec3i& baseCell, bool& outAlongX) {
    if (witherPatternAt(world, baseCell, true)) {
        outAlongX = true;
        return true;
    }
    if (witherPatternAt(world, baseCell, false)) {
        outAlongX = false;
        return true;
    }
    return false;
}

void consumeWitherPattern(World& world, const Vec3i& baseCell, bool alongX) {
    world.setBlock(baseCell.x, baseCell.y, baseCell.z, Block::Air);
    for (int along = -1; along <= 1; ++along) {
        Vec3i arm = witherOffset(baseCell, along, 1, alongX);
        world.setBlock(arm.x, arm.y, arm.z, Block::Air);
        Vec3i head = witherOffset(baseCell, along, 2, alongX);
        world.setBlock(head.x, head.y, head.z, Block::Air);
    }
}

WitherFight::WitherFight(uint32_t seed, const WitherFightConfig& config) : cfg(config) {
    boss.kind = BossKind::Wither;
    boss.maxHealth = cfg.maxHealth;
    boss.health = cfg.maxHealth;
    boss.rng = seed != 0u ? seed : 0x2f6b3ca1u;
    boss.phase = BossPhase::Dormant;
    boss.bar.title = "Wither";
    boss.bar.color = BossBarColor::Purple;
    boss.bar.style = BossBarStyle::Notched6;
    boss.bar.visible = false;
    ctx.rng = boss.rng;
    buildTree();
}

int WitherFight::aliveMinionCount() const {
    int total = 0;
    for (const BossMinion& minion : summoned) {
        if (minion.alive) ++total;
    }
    return total;
}

int WitherFight::nearestAliveTarget() const {
    if (activeTargets == nullptr) return -1;
    int best = -1;
    float bestSpan = 0.0f;
    for (std::size_t i = 0; i < activeTargets->size(); ++i) {
        const WitherTarget& candidate = (*activeTargets)[i];
        if (!candidate.alive) continue;
        float span = distanceBetween(boss.position, candidate.position);
        if (best >= 0 && span >= bestSpan) continue;
        best = static_cast<int>(i);
        bestSpan = span;
    }
    return best;
}

void WitherFight::buildTree() {
    auto defeat = seq("defeat");
    defeat->add(cond("healthGone", [this](BehaviorContext&) { return boss.health <= 0.0f; }));
    defeat->add(act("collapse", [this](BehaviorContext&) {
        boss.enterPhase(BossPhase::Defeated);
        boss.alive = false;
        boss.bar.visible = false;
        boss.bar.setFromHealth(0.0f, boss.maxHealth);
        return BehaviorStatus::Success;
    }));

