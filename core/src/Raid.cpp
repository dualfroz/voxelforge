#include "vf/Raid.h"

#include <cmath>

namespace vf {

namespace {

constexpr float raidTickSeconds = 1.0f / static_cast<float>(ticksPerSecond);

uint32_t nextRandom(uint32_t& state) {
    state += 0x9e3779b9u;
    uint32_t z = state;
    z = (z ^ (z >> 16)) * 0x85ebca6bu;
    z = (z ^ (z >> 13)) * 0xc2b2ae35u;
    return z ^ (z >> 16);
}

Vec3 cellCentre(const Vec3i& cell) {
    return {static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y),
            static_cast<float>(cell.z) + 0.5f};
}

float horizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

const Raider::Stats statsTable[static_cast<int>(RaiderKind::Count)] = {
    {24.0f, 3.0f, 2.6f, 1.8f, 20},
    {24.0f, 6.0f, 2.9f, 1.8f, 18},
    {26.0f, 4.0f, 2.4f, 6.0f, 30},
    {100.0f, 12.0f, 3.1f, 2.4f, 30},
};

bool advanceAlong(Vec3& position, const std::vector<Vec3i>& path, int& cursor, float speed,
                  float dt) {
    if (path.empty()) return false;
    int last = static_cast<int>(path.size()) - 1;
    float budget = speed * dt;
    while (budget > 0.0f) {
        Vec3 target = cellCentre(path[cursor]);
        Vec3 delta = target - position;
        float remaining = length(delta);
        if (remaining <= budget) {
            position = target;
            budget -= remaining;
            if (cursor >= last) return true;
            ++cursor;
        } else {
            position += normalize(delta) * budget;
            return false;
        }
    }
    return false;
}

bool standableNear(const World& world, const Vec3i& column, int verticalSearch,
                   const PathConfig& config, Vec3i& outCell) {
    for (int offset = 0; offset <= verticalSearch; ++offset) {
        Vec3i up{column.x, column.y + offset, column.z};
        if (isStandable(world, up, config)) {
            outCell = up;
            return true;
        }
        if (offset == 0) continue;
        Vec3i down{column.x, column.y - offset, column.z};
        if (isStandable(world, down, config)) {
            outCell = down;
            return true;
        }
    }
    outCell = column;
    return false;
}

RaiderKind kindForSlot(const RaidWave& plan, int slot) {
    if (slot < plan.raiders) return RaiderKind::Raider;
    slot -= plan.raiders;
    if (slot < plan.vindicators) return RaiderKind::Vindicator;
    slot -= plan.vindicators;
    if (slot < plan.witches) return RaiderKind::Witch;
    return RaiderKind::Ravager;
}

}

const char* raiderKindName(RaiderKind kind) {
    switch (kind) {
        case RaiderKind::Raider: return "raider";
        case RaiderKind::Vindicator: return "vindicator";
        case RaiderKind::Witch: return "witch";
        case RaiderKind::Ravager: return "ravager";
        case RaiderKind::Count: break;
    }
    return "unknown";
}

const Raider::Stats& raiderStats(RaiderKind kind) { return statsTable[static_cast<int>(kind)]; }

Vec3i Raider::cell() const {
    return {static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y)),
            static_cast<int>(std::floor(position.z))};
}

int RaidWave::countOf(RaiderKind kind) const {
    switch (kind) {
        case RaiderKind::Raider: return raiders;
        case RaiderKind::Vindicator: return vindicators;
        case RaiderKind::Witch: return witches;
        case RaiderKind::Ravager: return ravagers;
        case RaiderKind::Count: break;
    }
    return 0;
}

int raidWaveCount(int difficulty) {
    if (difficulty <= raidDifficultyEasy) return 3;
    if (difficulty == raidDifficultyNormal) return 5;
    return 7;
}

RaidWave planRaidWave(int waveIndex, int badOmenLevel, int difficulty) {
    RaidWave plan;
    plan.index = waveIndex < 0 ? 0 : waveIndex;
    int omen = badOmenLevel < 1 ? 1 : badOmenLevel;

    plan.raiders = 2 + plan.index + (difficulty >= raidDifficultyHard ? 1 : 0);
    if (plan.index >= 1)
        plan.vindicators = plan.index / 2 + (difficulty >= raidDifficultyNormal ? 1 : 0);
    if (plan.index >= 2) plan.witches = 1 + (difficulty >= raidDifficultyHard ? 1 : 0);
    if (plan.index >= 3) plan.ravagers = 1;
    if (omen >= 3) plan.vindicators += 1;
    if (omen >= 4 && plan.index >= 2) plan.ravagers += 1;
    return plan;
}

void applyBadOmen(OmenState& omen, int level, int durationTicks) {
    int capped = level > badOmenMaxLevel ? badOmenMaxLevel : level;
    if (capped < 1) return;
    if (capped >= omen.level) {
        omen.level = capped;
        omen.ticksRemaining = durationTicks;
    }
}

void grantHeroOfTheVillage(OmenState& omen, int level, int durationTicks) {
    int granted = level < 1 ? 1 : level;
    if (granted >= omen.heroLevel) {
        omen.heroLevel = granted;
        omen.heroTicksRemaining = durationTicks;
    }
}

void clearBadOmen(OmenState& omen) {
    omen.level = 0;
    omen.ticksRemaining = 0;
}

void tickOmen(OmenState& omen, int ticks) {
    if (ticks <= 0) return;
    if (omen.ticksRemaining > 0) {
        omen.ticksRemaining -= ticks;
        if (omen.ticksRemaining <= 0) {
            omen.ticksRemaining = 0;
            omen.level = 0;
        }
    }
    if (omen.heroTicksRemaining > 0) {
        omen.heroTicksRemaining -= ticks;
        if (omen.heroTicksRemaining <= 0) {
            omen.heroTicksRemaining = 0;
            omen.heroLevel = 0;
        }
    }
}

const char* raidStateName(RaidState state) {
    switch (state) {
        case RaidState::Inactive: return "inactive";
        case RaidState::Spawning: return "spawning";
        case RaidState::Fighting: return "fighting";
        case RaidState::Victory: return "victory";
        case RaidState::Defeat: return "defeat";
        case RaidState::Count: break;
    }
    return "unknown";
}

int golemCapFor(int villagerCount) {
    if (villagerCount < golemMinimumVillagers) return 0;
    int cap = villagerCount / 4;
    return cap < 1 ? 1 : cap;
}

bool tickGolemSpawner(GolemSpawner& spawner, int villagerCount, int ticks) {
    if (ticks <= 0) return false;
    if (spawner.golemsAlive >= golemCapFor(villagerCount)) {
        spawner.cooldownTicks = spawner.intervalTicks;
        return false;
    }
    spawner.cooldownTicks -= ticks;
    if (spawner.cooldownTicks > 0) return false;
    spawner.cooldownTicks = spawner.intervalTicks;
    ++spawner.golemsAlive;
    ++spawner.spawnedTotal;
    return true;
}

void updateGolemHostility(GolemSpawner& spawner, const ReputationView& reputation) {
    spawner.hostileToPlayer = reputation.golemHostile();
}

RaidManager::RaidManager(const Vec3i& center, uint32_t seed, int difficulty)
    : villageCenter(center), centerStand(center), seed(seed), difficulty(difficulty) {}

bool RaidManager::tryStart(OmenState& omen, const Vec3& playerPos, float villageRadius) {
    if (raidState != RaidState::Inactive) return false;
    if (!omen.hasBadOmen()) return false;
    if (horizontalDistance(playerPos, cellCentre(villageCenter)) > villageRadius) return false;

    omenLevel = omen.level;
    waves = raidWaveCount(difficulty);
    waveIdx = 0;
    waveTicks = 0;
    clearedTicks = 0;
    elapsed = 0;
    spawned = 0;
    lostVillagers = 0;
    attackers.clear();
    golemHealth.clear();
    spawner = GolemSpawner{};
    raidState = RaidState::Spawning;
    clearBadOmen(omen);
    return true;
}

int RaidManager::livingRaiders() const {
    int count = 0;
    for (const Raider& attacker : attackers)
        if (attacker.alive) ++count;
    return count;
}

bool RaidManager::damageRaider(int index, float amount) {
    if (index < 0 || index >= static_cast<int>(attackers.size())) return false;
    Raider& attacker = attackers[index];
    if (!attacker.alive) return false;
    attacker.health -= amount;
    if (attacker.health > 0.0f) return false;
    attacker.health = 0.0f;
    attacker.alive = false;
    return true;
}

void RaidManager::spawnWave(const World& world, const PathConfig& config) {
    currentWave = planRaidWave(waveIdx, omenLevel, difficulty);
    int count = currentWave.total();
    uint32_t rng = seed ^ (static_cast<uint32_t>(waveIdx + 1) * 0x85ebca6bu);

    for (int slot = 0; slot < count; ++slot) {
        Raider attacker;
        attacker.id = spawned++;
        attacker.kind = kindForSlot(currentWave, slot);
        attacker.wave = waveIdx;
        const Raider::Stats& stats = raiderStats(attacker.kind);
        attacker.health = stats.maxHealth;
        attacker.attackCooldown = static_cast<int>(nextRandom(rng) % 8u);

        float angle = 6.2831853f * static_cast<float>(slot) / static_cast<float>(count) +
                      static_cast<float>(waveIdx) * 0.37f;
        Vec3i column{villageCenter.x + static_cast<int>(std::lround(std::cos(angle) * raidSpawnRadius)),
                     villageCenter.y,
                     villageCenter.z + static_cast<int>(std::lround(std::sin(angle) * raidSpawnRadius))};
        Vec3i spawnCell;
        standableNear(world, column, 8, config, spawnCell);
        attacker.position = cellCentre(spawnCell);
        attacker.path = findPath(world, spawnCell, centerStand, config);
        attacker.pathCursor = 0;
        attackers.push_back(std::move(attacker));
    }
}

void RaidManager::moveRaiders(const std::vector<Villager>& villagers) {
    for (Raider& attacker : attackers) {
        if (!attacker.alive) continue;
        const Raider::Stats& stats = raiderStats(attacker.kind);
        if (!attacker.path.empty() &&
            attacker.pathCursor < static_cast<int>(attacker.path.size()) - 1) {
            advanceAlong(attacker.position, attacker.path, attacker.pathCursor, stats.speed,
                         raidTickSeconds);
            continue;
        }

        Vec3 target = cellCentre(centerStand);
        float bestDistance = 0.0f;
        bool found = false;
        for (const Villager& villager : villagers) {
            if (!villager.alive) continue;
            float distance = horizontalDistance(attacker.position, villager.body.position);
            if (!found || distance < bestDistance) {
                target = villager.body.position;
                bestDistance = distance;
                found = true;
            }
        }

        Vec3 delta = target - attacker.position;
        float remaining = length(delta);
        float budget = stats.speed * raidTickSeconds;
        if (remaining <= budget) attacker.position = target;
        else attacker.position += normalize(delta) * budget;
    }
}

void RaidManager::resolveRaiderAttacks(std::vector<Villager>& villagers, PoiRegistry& pois) {
    Vec3 centrePoint = cellCentre(centerStand);
    for (Raider& attacker : attackers) {
        if (!attacker.alive) continue;
        if (attacker.attackCooldown > 0) {
            --attacker.attackCooldown;
            continue;
        }
        const Raider::Stats& stats = raiderStats(attacker.kind);

        int victim = -1;
        float bestDistance = 0.0f;
        for (std::size_t i = 0; i < villagers.size(); ++i) {
            if (!villagers[i].alive) continue;
            float distance = horizontalDistance(attacker.position, villagers[i].body.position);
            if (distance > stats.attackRange) continue;
            if (victim < 0 || distance < bestDistance) {
                victim = static_cast<int>(i);
                bestDistance = distance;
            }
        }

        if (victim >= 0) {
            if (damageVillager(villagers[victim], pois, stats.damage)) ++lostVillagers;
            attacker.attackCooldown = stats.attackIntervalTicks;
            continue;
        }

        if (spawner.golemsAlive > 0 &&
            horizontalDistance(attacker.position, centrePoint) <= raidGolemEngageRadius) {
            for (std::size_t i = 0; i < golemHealth.size(); ++i) {
                if (golemHealth[i] <= 0.0f) continue;
                golemHealth[i] -= stats.damage;
                if (golemHealth[i] <= 0.0f) {
                    golemHealth[i] = 0.0f;
                    --spawner.golemsAlive;
                }
                break;
            }
            attacker.attackCooldown = stats.attackIntervalTicks;
        }
    }
}

void RaidManager::resolveGolems(int livingVillagers) {
    if (tickGolemSpawner(spawner, livingVillagers, 1)) golemHealth.push_back(spawner.maxHealth);

    if (spawner.golemsAlive <= 0) return;
    if (spawner.attackCooldown > 0) {
        --spawner.attackCooldown;
        return;
    }
    spawner.attackCooldown = spawner.attackIntervalTicks;

    Vec3 centrePoint = cellCentre(centerStand);
    for (float health : golemHealth) {
        if (health <= 0.0f) continue;
        int target = -1;
        float bestDistance = 0.0f;
        for (std::size_t i = 0; i < attackers.size(); ++i) {
            if (!attackers[i].alive) continue;
            float distance = horizontalDistance(attackers[i].position, centrePoint);
            if (target < 0 || distance < bestDistance) {
                target = static_cast<int>(i);
                bestDistance = distance;
            }
        }
        if (target < 0) return;
        damageRaider(target, spawner.damagePerAttack);
    }
}

void RaidManager::finishVictory(std::vector<Villager>& villagers, OmenState& omen) {
    raidState = RaidState::Victory;
    for (Villager& villager : villagers) {
        if (!villager.alive) continue;
        villager.gossip.add(GossipKind::MajorPositive, gossipMajorPositiveGain);
    }
    grantHeroOfTheVillage(omen, omenLevel, heroDurationTicks);
}

void RaidManager::stepOnce(const World& world, std::vector<Villager>& villagers, PoiRegistry& pois,
                           OmenState& omen, const VillagerSchedule& schedule,
                           const TimeOfDay& clock, const PathConfig& config) {
    ++elapsed;

    for (Villager& villager : villagers)
        tickVillager(world, villager, pois, clock, schedule, villageCenter, true, raidTickSeconds,
                     config);

    if (raidState == RaidState::Spawning) {
        spawnWave(world, config);
        raidState = RaidState::Fighting;
        waveTicks = 0;
        clearedTicks = 0;
    }

    if (raidState != RaidState::Fighting) return;

    moveRaiders(villagers);
    resolveRaiderAttacks(villagers, pois);

    int livingVillagers = 0;
    for (const Villager& villager : villagers)
        if (villager.alive) ++livingVillagers;

    resolveGolems(livingVillagers);
    ++waveTicks;

    if (livingVillagers == 0) {
        raidState = RaidState::Defeat;
        return;
    }

    int living = livingRaiders();
    if (living == 0) {
        ++clearedTicks;
        if (waveIdx + 1 >= waves) {
            finishVictory(villagers, omen);
            return;
        }
        if (clearedTicks >= raidWaveDelayTicks) {
            ++waveIdx;
            raidState = RaidState::Spawning;
        }
        return;
    }

    if (waveTicks >= raidWaveTimeoutTicks && waveIdx + 1 < waves) {
        ++waveIdx;
        raidState = RaidState::Spawning;
    }
}

void RaidManager::tick(const World& world, std::vector<Villager>& villagers, PoiRegistry& pois,
                       OmenState& omen, const VillagerSchedule& schedule, const TimeOfDay& clock,
                       int ticks, const PathConfig& config) {
    if (!active()) return;
    standableNear(world, villageCenter, 8, config, centerStand);
    for (int i = 0; i < ticks && active(); ++i)
        stepOnce(world, villagers, pois, omen, schedule, clock, config);
}

}
