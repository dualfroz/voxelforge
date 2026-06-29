#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Behavior.h"
#include "Block.h"
#include "EndGen.h"
#include "Math.h"
#include "World.h"

namespace vf {

// Two staged boss fights driven by the behaviour trees from Behavior.h. Both
// are pure tick machines: feed the same seed and the same sequence of player
// actions and the fight replays move for move.
enum class BossKind : uint8_t {
    EnderDragon = 0,
    Wither = 1,
    Count,
};

const char* bossKindName(BossKind kind);

enum class BossPhase : uint8_t {
    Dormant = 0,
    Circling,
    Charging,
    Breathing,
    Perching,
    Summoning,
    Explosive,
    Ranged,
    Armored,
    Dying,
    Defeated,
    Count,
};

const char* bossPhaseName(BossPhase phase);

enum class BossBarColor : uint8_t { Pink = 0, Blue, Red, Green, Yellow, Purple, White, Count };
enum class BossBarStyle : uint8_t { Solid = 0, Notched6, Notched10, Notched12, Notched20, Count };

struct BossBar {
    std::string title;
    float progress = 1.0f;
    BossBarColor color = BossBarColor::Purple;
    BossBarStyle style = BossBarStyle::Notched10;
    bool visible = false;

    int notchCount() const;
    // Whole segments currently lit; a partially drained segment reads as empty.
    int filledNotches() const;
    int percent() const;
    void setFromHealth(float health, float maxHealth);
};

struct BossState {
    BossKind kind = BossKind::EnderDragon;
    BossPhase phase = BossPhase::Dormant;
    float health = 200.0f;
    float maxHealth = 200.0f;
    Vec3 position;
    Vec3 heading{1.0f, 0.0f, 0.0f};
    float orbitAngle = 0.0f;
    int targetIndex = -1;
    long long tick = 0;
    int phaseTicks = 0;
    int phaseChanges = 0;
    int attacksLaunched = 0;
    int minionsSummoned = 0;
    float healingReceived = 0.0f;
    float damageTaken = 0.0f;
    bool alive = true;
    uint32_t rng = 0x51ed270bu;
    BossBar bar;

    void enterPhase(BossPhase next);
    float healthFraction() const;
};

// One end crystal on top of an obsidian pillar from EndGen.h. The taller
// pillars carry a cage that has to come down before the crystal can be shot.
struct CrystalNode {
    Vec3i pillarBase;
    Vec3i position;
    int pillarHeight = 0;
    bool caged = false;
    bool cageIntact = false;
    bool alive = true;
    float health = 5.0f;
    long long destroyedTick = -1;
};

std::vector<CrystalNode> crystalsFromPillars(const std::vector<ObsidianPillar>& pillars);

struct BossTickReport {
    BossPhase phase = BossPhase::Dormant;
    BossPhase previousPhase = BossPhase::Dormant;
    bool phaseChanged = false;
    float bossHealth = 0.0f;
    float healedThisTick = 0.0f;
    float damageToTargets = 0.0f;
    int crystalsDestroyed = 0;
    int minionsSpawned = 0;
    bool defeated = false;
};

struct DragonFightConfig {
    float maxHealth = 200.0f;
    float orbitRadius = 44.0f;
    float orbitHeight = 26.0f;
    float orbitStep = 0.06f;
    float chargeTrigger = 48.0f;
    float chargeSpeed = 1.4f;
    float chargeContactRange = 3.0f;
    float chargeDamage = 10.0f;
    int chargeCooldownTicks = 60;
    int chargeWindowTicks = 40;
    float breathRange = 14.0f;
    float breathDamage = 3.0f;
    int breathCooldownTicks = 40;
    int swipeCooldownTicks = 25;
    float swipeDamage = 6.0f;
    float swipeRange = 8.0f;
    float crystalHealPerTick = 0.4f;
    float crystalHealRange = 128.0f;
    float crystalBlastRange = 12.0f;
    float crystalBlastDamage = 6.0f;
    float perchHeight = 3.0f;
    float perchDescentStep = 0.9f;
    float meleeReach = 7.0f;
    float flyingDamageFactor = 0.25f;
    int deathTicks = 40;
};

// What the player did on one tick. `shootCrystal` indexes crystals(); a caged
// crystal needs `breakCage` on an earlier tick before a shot lands.
struct DragonPlayerInput {
    Vec3 position;
    bool present = true;
    int shootCrystal = -1;
    bool breakCage = false;
    float meleeDamage = 0.0f;
};

// Crystals heal the dragon while they stand, so the fight cannot be won by
// damage alone. Killing the last one drops it onto the portal, where melee
// lands at full strength; the kill opens the exit portal and leaves the egg.
class DragonFight {
public:
    DragonFight(const EndGenerator& end, uint32_t seed, const DragonFightConfig& config = {});

    DragonFight(const DragonFight&) = delete;
    DragonFight& operator=(const DragonFight&) = delete;
    DragonFight(DragonFight&&) = delete;
    DragonFight& operator=(DragonFight&&) = delete;

    BossTickReport tick(const DragonPlayerInput& input);

    const BossState& state() const { return boss; }
    const DragonFightConfig& settings() const { return cfg; }
    const std::vector<CrystalNode>& crystals() const { return pillarCrystals; }
    int aliveCrystalCount() const;
    // Taken from the End generator: the fight orbits and perches on the real
    // exit portal column, not on an invented origin.
    Vec3 portalCenter() const { return portal; }

    bool exitPortalOpen() const { return portalOpen; }
    bool dragonEggDropped() const { return eggDropped; }
    Vec3i eggPosition() const { return eggCell; }

    const BehaviorTree& brain() const { return tree; }
    const BehaviorContext& context() const { return ctx; }

    // Writes the reward the fight earned: the real End exit portal from
    // EndGen. Refuses while the dragon still lives.
    bool placeVictoryStructures(World& world, const EndGenerator& end) const;

private:
    void buildTree();
    int resolvePlayerShot(const DragonPlayerInput& input);
    void applyPlayerMelee(const DragonPlayerInput& input);
    float applyCrystalHealing();

    DragonFightConfig cfg;
    BossState boss;
    std::vector<CrystalNode> pillarCrystals;
    BehaviorTree tree;
    BehaviorContext ctx;
    DragonPlayerInput lastInput;
    Vec3 portal;
    Vec3i eggCell;
    bool portalOpen = false;
    bool eggDropped = false;
    float damageThisTick = 0.0f;
};

struct WitherFightConfig {
    float maxHealth = 300.0f;
    int summonTicks = 220;
    float summonBlastRadius = 8.0f;
    float summonBlastDamage = 12.0f;
    float armorThreshold = 0.5f;
    float projectileDamage = 5.0f;
    float projectileRange = 32.0f;
    int projectileCooldownTicks = 20;
    int minionCooldownTicks = 100;
    int maxMinions = 3;
    float minionSpawnRange = 6.0f;
    float armoredRegenPerTick = 0.06f;
    float moveStep = 0.35f;
    float minTargetDistance = 6.0f;
};

struct WitherTarget {
    Vec3 position;
    float health = 20.0f;
    bool alive = true;
};

struct BossMinion {
    Vec3 position;
    float health = 6.0f;
    bool alive = true;
    long long spawnedTick = 0;
};

struct WitherHit {
    float amount = 0.0f;
    bool projectile = false;
};

// The block pattern that calls the wither: a soul sand T topped with three
// heads, readable along either horizontal axis.
Block witherBaseBlock();
Block witherHeadBlock();
bool witherPatternAt(const World& world, const Vec3i& baseCell, bool alongX);
bool findWitherPattern(const World& world, const Vec3i& baseCell, bool& outAlongX);
void consumeWitherPattern(World& world, const Vec3i& baseCell, bool alongX);

// Summons out of a block pattern, blasts everything around it on arrival, then
// hunts the nearest living target. Below half health it armours up: projectiles
// stop landing entirely and it starts regenerating and calling minions.
class WitherFight {
public:
    explicit WitherFight(uint32_t seed, const WitherFightConfig& config = {});

    WitherFight(const WitherFight&) = delete;
    WitherFight& operator=(const WitherFight&) = delete;
    WitherFight(WitherFight&&) = delete;
    WitherFight& operator=(WitherFight&&) = delete;

    void begin(const Vec3& spawnPosition);
    // Consumes the pattern in the world and starts the fight at its centre.
    bool summonFromWorld(World& world, const Vec3i& baseCell);

    BossTickReport tick(const std::vector<WitherHit>& incoming,
                        std::vector<WitherTarget>& targets);

    const BossState& state() const { return boss; }
    const WitherFightConfig& settings() const { return cfg; }
    const std::vector<BossMinion>& minions() const { return summoned; }
    bool armored() const { return boss.phase == BossPhase::Armored; }
    int aliveMinionCount() const;

    const BehaviorTree& brain() const { return tree; }
    const BehaviorContext& context() const { return ctx; }

private:
    void buildTree();
    float applyIncoming(const std::vector<WitherHit>& incoming);
    int nearestAliveTarget() const;

    WitherFightConfig cfg;
    BossState boss;
    std::vector<BossMinion> summoned;
    BehaviorTree tree;
    BehaviorContext ctx;
    std::vector<WitherTarget>* activeTargets = nullptr;
    float damageThisTick = 0.0f;
    float healedThisTick = 0.0f;
    int minionsThisTick = 0;
};

} // namespace vf
