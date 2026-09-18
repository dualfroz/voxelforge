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

}

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

    auto summon = seq("summonCharge");
    summon->add(cond("stillSummoning",
                     [this](BehaviorContext&) { return boss.phase == BossPhase::Summoning; }));
    summon->add(act("chargeUp", [this](BehaviorContext&) {
        if (boss.phaseTicks < cfg.summonTicks) return BehaviorStatus::Running;
        boss.enterPhase(BossPhase::Explosive);
        for (WitherTarget& target : *activeTargets) {
            if (!target.alive) continue;
            if (distanceBetween(boss.position, target.position) > cfg.summonBlastRadius) continue;
            float dealt = std::min(cfg.summonBlastDamage, target.health);
            target.health -= dealt;
            if (target.health <= 0.0f) target.alive = false;
            damageThisTick += dealt;
        }
        ++boss.attacksLaunched;
        return BehaviorStatus::Success;
    }));

    auto settle = seq("afterBlast");
    settle->add(cond("justExploded",
                     [this](BehaviorContext&) { return boss.phase == BossPhase::Explosive; }));
    settle->add(act("settleIntoCombat", [this](BehaviorContext&) {
        boss.enterPhase(BossPhase::Ranged);
        return BehaviorStatus::Success;
    }));

    auto armorUp = seq("armorUp");
    armorUp->add(cond("halfHealthReached", [this](BehaviorContext&) {
        if (boss.phase != BossPhase::Ranged) return false;
        return boss.health <= boss.maxHealth * cfg.armorThreshold;
    }));
    armorUp->add(act("raiseArmor", [this](BehaviorContext&) {
        boss.enterPhase(BossPhase::Armored);
        return BehaviorStatus::Success;
    }));

    BehaviorAction aim = [this](BehaviorContext&) {
        boss.targetIndex = nearestAliveTarget();
        return boss.targetIndex >= 0 ? BehaviorStatus::Success : BehaviorStatus::Failure;
    };
    BehaviorAction advance = [this](BehaviorContext&) {
        WitherTarget& target = (*activeTargets)[static_cast<std::size_t>(boss.targetIndex)];
        Vec3 delta = target.position - boss.position;
        float span = length(delta);
        if (span > cfg.minTargetDistance) boss.position += delta * (cfg.moveStep / span);
        boss.heading = span > 1e-4f ? normalize(delta) : boss.heading;
        return BehaviorStatus::Success;
    };
    BehaviorAction skull = [this](BehaviorContext&) {
        WitherTarget& target = (*activeTargets)[static_cast<std::size_t>(boss.targetIndex)];
        if (distanceBetween(boss.position, target.position) > cfg.projectileRange)
            return BehaviorStatus::Failure;
        float dealt = std::min(cfg.projectileDamage, target.health);
        target.health -= dealt;
        if (target.health <= 0.0f) target.alive = false;
        damageThisTick += dealt;
        ++boss.attacksLaunched;
        return BehaviorStatus::Success;
    };

    auto armoredVolley = std::unique_ptr<ParallelNode>(new ParallelNode("armoredVolley", 1, 4));
    armoredVolley->add(gated("armoredSkullCooldown",
                             act("skullVolley",
                                 [this, skull](BehaviorContext& context) {
                                     BehaviorStatus last = BehaviorStatus::Failure;
                                     for (int shot = 0; shot < 3; ++shot) last = skull(context);
                                     return last;
                                 }),
                             cfg.projectileCooldownTicks));
    armoredVolley->add(gated("minionCooldown",
                             act("summonMinion",
                                 [this](BehaviorContext&) {
                                     if (aliveMinionCount() >= cfg.maxMinions)
                                         return BehaviorStatus::Failure;
                                     float angle = behaviorUnitFloat(boss.rng) * fullTurn;
                                     BossMinion minion;
                                     minion.position =
                                         boss.position +
                                         Vec3{std::cos(angle) * cfg.minionSpawnRange, 0.0f,
                                              std::sin(angle) * cfg.minionSpawnRange};
                                     minion.spawnedTick = boss.tick;
                                     summoned.push_back(minion);
                                     ++boss.minionsSummoned;
                                     ++minionsThisTick;
                                     return BehaviorStatus::Success;
                                 }),
                             cfg.minionCooldownTicks));
    armoredVolley->add(act("regenerate", [this](BehaviorContext&) {
        float room = boss.maxHealth - boss.health;
        float healed = std::min(cfg.armoredRegenPerTick, std::max(0.0f, room));
        boss.health += healed;
        boss.healingReceived += healed;
        healedThisTick += healed;
        return BehaviorStatus::Success;
    }));

    auto armored = seq("armoredAssault");
    armored->add(
        cond("phaseIsArmored", [this](BehaviorContext&) { return boss.phase == BossPhase::Armored; }));
    armored->add(act("aimAtNearest", aim));
    armored->add(act("advanceOnTarget", advance));
    armored->add(std::move(armoredVolley));

    auto ranged = seq("rangedAssault");
    ranged->add(
        cond("phaseIsRanged", [this](BehaviorContext&) { return boss.phase == BossPhase::Ranged; }));
    ranged->add(act("aimAtNearest", aim));
    ranged->add(act("advanceOnTarget", advance));
    ranged->add(gated("skullCooldown", act("fireSkull", skull), cfg.projectileCooldownTicks));

    auto root = sel("witherRoot");
    root->add(std::move(defeat));
    root->add(std::move(summon));
    root->add(std::move(settle));
    root->add(std::move(armorUp));
    root->add(std::move(armored));
    root->add(std::move(ranged));
    root->add(act("hover", [this](BehaviorContext&) {
        boss.position.y += std::sin(static_cast<float>(boss.tick) * 0.1f) * 0.02f;
        return BehaviorStatus::Success;
    }));

    tree.setRoot(std::move(root));
}

void WitherFight::begin(const Vec3& spawnPosition) {
    boss.position = spawnPosition;
    boss.maxHealth = cfg.maxHealth;
    boss.health = cfg.maxHealth;
    boss.alive = true;
    boss.targetIndex = -1;
    boss.attacksLaunched = 0;
    boss.minionsSummoned = 0;
    boss.phase = BossPhase::Dormant;
    boss.enterPhase(BossPhase::Summoning);
    boss.bar.visible = true;
    boss.bar.setFromHealth(boss.health, boss.maxHealth);
    summoned.clear();
    tree.reset();
}

bool WitherFight::summonFromWorld(World& world, const Vec3i& baseCell) {
    bool alongX = true;
    if (!findWitherPattern(world, baseCell, alongX)) return false;
    consumeWitherPattern(world, baseCell, alongX);
    begin(behaviorCellCenter({baseCell.x, baseCell.y + 1, baseCell.z}));
    return true;
}

float WitherFight::applyIncoming(const std::vector<WitherHit>& incoming) {
    if (boss.phase == BossPhase::Summoning || boss.phase == BossPhase::Defeated) return 0.0f;

    float taken = 0.0f;
    for (const WitherHit& hit : incoming) {
        if (hit.amount <= 0.0f) continue;
        if (armored() && hit.projectile) continue;
        float dealt = std::min(hit.amount, boss.health);
        boss.health -= dealt;
        taken += dealt;
    }
    boss.damageTaken += taken;
    return taken;
}

BossTickReport WitherFight::tick(const std::vector<WitherHit>& incoming,
                                 std::vector<WitherTarget>& targets) {
    BossTickReport report;
    report.previousPhase = boss.phase;
    report.phase = boss.phase;
    report.bossHealth = boss.health;

    if (boss.phase == BossPhase::Dormant || boss.phase == BossPhase::Defeated) {
        report.defeated = boss.phase == BossPhase::Defeated;
        return report;
    }

    activeTargets = &targets;
    damageThisTick = 0.0f;
    healedThisTick = 0.0f;
    minionsThisTick = 0;

    applyIncoming(incoming);

    int nearest = nearestAliveTarget();
    ctx.playerPresent = nearest >= 0;
    ctx.playerPosition = nearest >= 0 ? targets[static_cast<std::size_t>(nearest)].position
                                      : boss.position;

    ++boss.phaseTicks;
    int changesBefore = boss.phaseChanges;
    tree.tick(ctx);
    boss.tick = ctx.tick;
    if (boss.phase != BossPhase::Defeated) boss.bar.setFromHealth(boss.health, boss.maxHealth);
    activeTargets = nullptr;

    report.phase = boss.phase;
    report.phaseChanged = boss.phaseChanges != changesBefore;
    report.bossHealth = boss.health;
    report.healedThisTick = healedThisTick;
    report.damageToTargets = damageThisTick;
    report.minionsSpawned = minionsThisTick;
    report.defeated = boss.phase == BossPhase::Defeated;
    return report;
}

}
