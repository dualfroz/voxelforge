#include "Wildlife.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "vf/Explosion.h"

namespace vf {

namespace {

constexpr float spawnInterval = 2.0f;
constexpr float corpseLinger = 0.6f;
constexpr float creeperPower = 3.0f;
constexpr float creeperMaxDamage = 22.0f;

uint32_t nextRandom(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}

float rayBoxDistance(const Vec3& origin, const Vec3& direction, const Aabb& box) {
    float nearest = 0.0f;
    float farthest = std::numeric_limits<float>::infinity();
    const float start[3] = {origin.x, origin.y, origin.z};
    const float step[3] = {direction.x, direction.y, direction.z};
    const float low[3] = {box.min.x, box.min.y, box.min.z};
    const float high[3] = {box.max.x, box.max.y, box.max.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(step[axis]) < 1e-6f) {
            if (start[axis] < low[axis] || start[axis] > high[axis]) return -1.0f;
            continue;
        }
        float t0 = (low[axis] - start[axis]) / step[axis];
        float t1 = (high[axis] - start[axis]) / step[axis];
        if (t0 > t1) std::swap(t0, t1);
        nearest = std::max(nearest, t0);
        farthest = std::min(farthest, t1);
        if (nearest > farthest) return -1.0f;
    }
    return nearest;
}

BiomeKind spawnBiomeFor(Biome biome) {
    switch (biome) {
        case Biome::Ocean: return BiomeKind::Ocean;
        case Biome::Beach: return BiomeKind::Beach;
        case Biome::Plains: return BiomeKind::Plains;
        case Biome::Forest: return BiomeKind::Forest;
        case Biome::Desert: return BiomeKind::Desert;
        case Biome::Mountains: return BiomeKind::Mountains;
        case Biome::Snow: return BiomeKind::Tundra;
        default: return BiomeKind::Plains;
    }
}

const char* biomeDisplayName(Biome biome) {
    switch (biome) {
        case Biome::Ocean: return "Ocean";
        case Biome::Beach: return "Beach";
        case Biome::Plains: return "Plains";
        case Biome::Forest: return "Forest";
        case Biome::Desert: return "Desert";
        case Biome::Mountains: return "Mountains";
        case Biome::Snow: return "Snowy peaks";
        default: return "Unknown";
    }
}

Wildlife::Wildlife(World& world, TerrainView& terrain, const WorldGen& generator, uint32_t seed)
    : world(world), terrain(terrain), generator(generator), rng(seed | 1u) {
    std::vector<SpawnRule> rules = defaultSpawnRules();
    for (SpawnRule& rule : rules) {
        rule.maxY = generator.settings().maxY - 1;
        if (rule.category == SpawnCategory::Passive) rule.minY = generator.settings().seaLevel + 1;
    }
    MobCap caps;
    caps.baseCap = {{40, 30, 5, 15}};
    spawner = Spawner(std::move(rules), caps);
    creatureConfig.physics.path.maxExpansions = 600;
    despawnPolicy.instantDistance = 96.0f;
}

int Wildlife::livingIn(SpawnCategory category) const {
    int living = 0;
    for (const WildCreature& wild : herd)
        if (wild.creature.alive && categoryForSpecies(wild.creature.species) == category) ++living;
    return living;
}

void Wildlife::spawnFrom(const SpawnResult& result) {
    for (const Vec3& position : result.positions) {
        WildCreature wild;
        wild.creature = spawnCreature(result.species, position, nextRandom(rng));
        herd.push_back(wild);
    }
}

void Wildlife::runSpawnCycle(const Vec3& playerPosition, bool daytime, int columnsPerCategory) {
    SpawnEnvironment env;
    env.world = &world;
    env.daytime = daytime;
    env.playerPosition = playerPosition;
    env.loadedChunks = static_cast<int>(terrain.residentColumns());
    env.topY = generator.settings().maxY - 2;
    env.bottomY = 1;
    for (int category = 0; category < spawnCategoryCount; ++category)
        env.population[static_cast<std::size_t>(category)] =
            livingIn(static_cast<SpawnCategory>(category));

    ColumnCoord centre{floorDiv(static_cast<int>(std::floor(playerPosition.x)), chunkSize),
                       floorDiv(static_cast<int>(std::floor(playerPosition.z)), chunkSize)};
    int reach = std::max(2, terrain.loadRadius() - 1);
    const SpawnCategory categories[2] = {SpawnCategory::Passive, SpawnCategory::Hostile};
    for (SpawnCategory category : categories) {
        if (category == SpawnCategory::Hostile && daytime) continue;
        for (int attempt = 0; attempt < columnsPerCategory; ++attempt) {
            ++cycle;
            int dx = static_cast<int>(nextRandom(rng) % static_cast<uint32_t>(2 * reach + 1)) - reach;
            int dz = static_cast<int>(nextRandom(rng) % static_cast<uint32_t>(2 * reach + 1)) - reach;
            ColumnCoord column{centre.x + dx, centre.z + dz};
            if (!terrain.columnReady(column)) continue;
            env.light = &terrain.rawLight(column);
            env.fallbackBiome = spawnBiomeFor(generator.biomeAt(column.x * chunkSize + chunkSize / 2,
                                                                column.z * chunkSize + chunkSize / 2));
            SpawnResult result =
                spawner.runColumn(env, category, {column.x, 0, column.z}, cycle * 7919u, attempt);
            if (result.spawned) spawnFrom(result);
        }
    }
}

void Wildlife::populate(const Vec3& playerPosition, bool daytime) {
    for (int round = 0; round < 6; ++round) runSpawnCycle(playerPosition, daytime, 6);
}

WildlifeImpact Wildlife::detonate(WildCreature& creeper, const Vec3& playerPosition) {
    WildlifeImpact impact;
    Vec3i centre = mobFeetCell(creeper.creature.body);
    ExplosionConfig blast;
    blast.power = creeperPower;
    ExplosionResult result = explode(world, centre, blast);
    for (const Vec3i& cell : result.destroyedBlocks) terrain.markCellChanged(cell.x, cell.y, cell.z);
    Vec3 offset = playerPosition - creeper.creature.body.position;
    float proximity = 1.0f - length(offset) / (creeperPower * 2.0f);
    if (proximity > 0.0f) impact.damage = proximity * creeperMaxDamage;
    impact.knockback = explosionKnockback(centre, playerPosition, creeperPower);
    impact.exploded = true;
    creeper.creature.alive = false;
    creeper.corpseSeconds = corpseLinger;
    return impact;
}

WildlifeImpact Wildlife::tick(const Vec3& playerPosition, bool daytime, float dt) {
    spawnTimer += dt;
    if (spawnTimer >= spawnInterval) {
        spawnTimer = 0.0f;
        runSpawnCycle(playerPosition, daytime, 2);
    }

    WildlifeImpact impact;
    for (WildCreature& wild : herd) {
        Creature& creature = wild.creature;
        if (!creature.alive) {
            wild.corpseSeconds += dt;
            continue;
        }
        wild.hurtSeconds = std::max(0.0f, wild.hurtSeconds - dt);
        wild.panicSeconds = std::max(0.0f, wild.panicSeconds - dt);

        bool hostile = isHostile(creature.species);
        bool hasThreat = hostile || wild.panicSeconds > 0.0f;
        updateCreature(world, creature, playerPosition, hasThreat, creatureConfig, dt);

        bool sameLevel = std::fabs(creature.body.position.y - playerPosition.y) < 2.0f;
        if (hostile && sameLevel && creatureCanAttack(creature, playerPosition)) {
            creatureAttack(creature);
            if (creature.species == Species::Creeper) {
                WildlifeImpact blast = detonate(wild, playerPosition);
                impact.damage += blast.damage;
                impact.knockback += blast.knockback;
                impact.exploded = true;
            } else {
                impact.damage += speciesStats(creature.species).attackDamage;
            }
        }

        DespawnCandidate candidate;
        candidate.position = creature.body.position;
        candidate.persistent = !hostile;
        bool gone = hostile && shouldDespawn(despawnPolicy, candidate, playerPosition, rng);
        ColumnCoord column{floorDiv(static_cast<int>(std::floor(creature.body.position.x)), chunkSize),
                           floorDiv(static_cast<int>(std::floor(creature.body.position.z)), chunkSize)};
        if (!terrain.columnReady(column) || creature.body.position.y < -8.0f) gone = true;
        if (gone) {
            creature.alive = false;
            wild.corpseSeconds = corpseLinger;
        }
    }
    herd.erase(std::remove_if(herd.begin(), herd.end(),
                              [](const WildCreature& wild) {
                                  return !wild.creature.alive && wild.corpseSeconds >= corpseLinger;
                              }),
               herd.end());
    return impact;
}

int Wildlife::pick(const Vec3& origin, const Vec3& direction, float maxDistance,
                   float& distance) const {
    int best = -1;
    distance = maxDistance;
    for (std::size_t i = 0; i < herd.size(); ++i) {
        if (!herd[i].creature.alive) continue;
        float hit = rayBoxDistance(origin, direction, mobBox(herd[i].creature.body));
        if (hit < 0.0f || hit >= distance) continue;
        distance = hit;
        best = static_cast<int>(i);
    }
    return best;
}

std::vector<ItemSlot> Wildlife::strike(int index, const Vec3& attacker, float damage) {
    WildCreature& wild = herd[static_cast<std::size_t>(index)];
    Creature& creature = wild.creature;
    wild.hurtSeconds = 0.3f;
    wild.panicSeconds = 6.0f;
    Vec3 away = normalize(Vec3{creature.body.position.x - attacker.x, 0.0f,
                               creature.body.position.z - attacker.z});
    creature.body.wanderDir = away;
    if (creature.body.onGround) creature.body.velocity.y = 5.0f;
    if (!damageCreature(creature, damage)) return {};
    return rollDrops(creature.species, creature.rng);
}

}
