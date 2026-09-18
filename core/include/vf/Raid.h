#pragma once

#include <cstdint>
#include <vector>
#include "Effect.h"
#include "Gossip.h"
#include "Math.h"
#include "Pathfind.h"
#include "TimeOfDay.h"
#include "Villager.h"
#include "World.h"

namespace vf {

enum class RaiderKind : uint8_t {
    Raider = 0,
    Vindicator,
    Witch,
    Ravager,
    Count,
};

const char* raiderKindName(RaiderKind kind);

struct Raider {
    struct Stats {
        float maxHealth = 24.0f;
        float damage = 3.0f;
        float speed = 2.6f;
        float attackRange = 1.8f;
        int attackIntervalTicks = 20;
    };

    int id = 0;
    RaiderKind kind = RaiderKind::Raider;
    int wave = 0;
    Vec3 position;
    float health = 24.0f;
    std::vector<Vec3i> path;
    int pathCursor = 0;
    int attackCooldown = 0;
    bool alive = true;

    Vec3i cell() const;
};

const Raider::Stats& raiderStats(RaiderKind kind);

struct RaidWave {
    int index = 0;
    int raiders = 0;
    int vindicators = 0;
    int witches = 0;
    int ravagers = 0;

    int countOf(RaiderKind kind) const;
    int total() const { return raiders + vindicators + witches + ravagers; }
};

constexpr int raidDifficultyEasy = 1;
constexpr int raidDifficultyNormal = 2;
constexpr int raidDifficultyHard = 3;

int raidWaveCount(int difficulty);
RaidWave planRaidWave(int waveIndex, int badOmenLevel, int difficulty);

constexpr int badOmenDurationTicks = 100 * ticksPerSecond;
constexpr int heroDurationTicks = 40 * 60 * ticksPerSecond;
constexpr int badOmenMaxLevel = 5;

struct OmenState {
    int level = 0;
    int ticksRemaining = 0;
    int heroLevel = 0;
    int heroTicksRemaining = 0;

    bool hasBadOmen() const { return level > 0 && ticksRemaining > 0; }
    bool isHero() const { return heroLevel > 0 && heroTicksRemaining > 0; }
};

void applyBadOmen(OmenState& omen, int level, int durationTicks = badOmenDurationTicks);
void grantHeroOfTheVillage(OmenState& omen, int level, int durationTicks = heroDurationTicks);
void clearBadOmen(OmenState& omen);
void tickOmen(OmenState& omen, int ticks);

enum class RaidState : uint8_t {
    Inactive = 0,
    Spawning,
    Fighting,
    Victory,
    Defeat,
    Count,
};

const char* raidStateName(RaidState state);

struct GolemSpawner {
    int intervalTicks = 120;
    int cooldownTicks = 0;
    int golemsAlive = 0;
    int spawnedTotal = 0;
    int attackIntervalTicks = 20;
    int attackCooldown = 0;
    float damagePerAttack = 7.0f;
    float maxHealth = 100.0f;
    bool hostileToPlayer = false;
};

constexpr int golemMinimumVillagers = 3;

int golemCapFor(int villagerCount);

bool tickGolemSpawner(GolemSpawner& spawner, int villagerCount, int ticks);
void updateGolemHostility(GolemSpawner& spawner, const ReputationView& reputation);

constexpr int raidWaveDelayTicks = 100;
constexpr int raidWaveTimeoutTicks = 2400;
constexpr float raidSpawnRadius = 24.0f;
constexpr float raidGolemEngageRadius = 7.0f;

class RaidManager {
public:
    explicit RaidManager(const Vec3i& center = {}, uint32_t seed = 1u,
                         int difficulty = raidDifficultyNormal);

    bool tryStart(OmenState& omen, const Vec3& playerPos, float villageRadius);

    void tick(const World& world, std::vector<Villager>& villagers, PoiRegistry& pois,
              OmenState& omen, const VillagerSchedule& schedule, const TimeOfDay& clock, int ticks,
              const PathConfig& config = {});

    RaidState state() const { return raidState; }
    bool active() const { return raidState == RaidState::Spawning || raidState == RaidState::Fighting; }
    bool villageThreatened() const { return active(); }

    int waveIndex() const { return waveIdx; }
    int waveCount() const { return waves; }
    const RaidWave& wave() const { return currentWave; }

    const std::vector<Raider>& raiders() const { return attackers; }
    int livingRaiders() const;
    int totalSpawned() const { return spawned; }
    int villagersLost() const { return lostVillagers; }
    int badOmenLevel() const { return omenLevel; }
    int elapsedTicks() const { return elapsed; }
    const GolemSpawner& golems() const { return spawner; }
    const Vec3i& center() const { return villageCenter; }

    bool damageRaider(int index, float amount);

private:
    void spawnWave(const World& world, const PathConfig& config);
    void moveRaiders(const std::vector<Villager>& villagers);
    void resolveRaiderAttacks(std::vector<Villager>& villagers, PoiRegistry& pois);
    void resolveGolems(int livingVillagers);
    void finishVictory(std::vector<Villager>& villagers, OmenState& omen);
    void stepOnce(const World& world, std::vector<Villager>& villagers, PoiRegistry& pois,
                  OmenState& omen, const VillagerSchedule& schedule, const TimeOfDay& clock,
                  const PathConfig& config);

    Vec3i villageCenter;
    Vec3i centerStand;
    uint32_t seed = 1u;
    int difficulty = raidDifficultyNormal;
    RaidState raidState = RaidState::Inactive;
    RaidWave currentWave;
    std::vector<Raider> attackers;
    std::vector<float> golemHealth;
    GolemSpawner spawner;
    int waves = 0;
    int waveIdx = -1;
    int waveTicks = 0;
    int clearedTicks = 0;
    int elapsed = 0;
    int spawned = 0;
    int lostVillagers = 0;
    int omenLevel = 0;
};

}
