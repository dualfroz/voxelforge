#include "vf/Villager.h"

#include <cmath>

namespace vf {

namespace {

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

int squaredDistance(const Vec3i& a, const Vec3i& b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    int dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

const int levelThresholds[villagerMaxLevel] = {0, 10, 70, 150, 250};

const int horizontalOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

bool resolveStandCell(const World& world, const Vec3i& pos, Block block, const PathConfig& config,
                      Vec3i& outStand) {
    if (!isSolid(block) && isStandable(world, pos, config)) {
        outStand = pos;
        return true;
    }
    Vec3i above{pos.x, pos.y + 1, pos.z};
    if (isStandable(world, above, config)) {
        outStand = above;
        return true;
    }
    for (int level = 0; level <= 1; ++level) {
        for (const auto& offset : horizontalOffsets) {
            Vec3i candidate{pos.x + offset[0], pos.y + level, pos.z + offset[1]};
            if (isStandable(world, candidate, config)) {
                outStand = candidate;
                return true;
            }
        }
    }
    outStand = above;
    return false;
}

bool poiBlock(Block block) {
    return block == Block::Bed || block == Block::Bell || isWorkstationBlock(block);
}

bool settleVillager(const World& world, Villager& villager, const PathConfig& config) {
    Vec3i cell = villager.cell();
    if (isStandable(world, cell, config)) return false;
    for (int drop = 1; drop <= 8; ++drop) {
        Vec3i below{cell.x, cell.y - drop, cell.z};
        if (!isStandable(world, below, config)) continue;
        villager.body.position = cellCentre(below);
        villager.path.clear();
        villager.pathCursor = 0;
        villager.hasGoal = false;
        return true;
    }
    return false;
}

bool withinVillage(const Vec3i& cell, const Vec3i& center, int radius) {
    return std::abs(cell.x - center.x) <= radius && std::abs(cell.z - center.z) <= radius &&
           std::abs(cell.y - center.y) <= radius;
}

}

const char* villagerProfessionName(VillagerProfession profession) {
    switch (profession) {
        case VillagerProfession::Unemployed: return "unemployed";
        case VillagerProfession::Nitwit: return "nitwit";
        case VillagerProfession::Farmer: return "farmer";
        case VillagerProfession::Fisherman: return "fisherman";
        case VillagerProfession::Shepherd: return "shepherd";
        case VillagerProfession::Fletcher: return "fletcher";
        case VillagerProfession::Librarian: return "librarian";
        case VillagerProfession::Cartographer: return "cartographer";
        case VillagerProfession::Cleric: return "cleric";
        case VillagerProfession::Armorer: return "armorer";
        case VillagerProfession::Weaponsmith: return "weaponsmith";
        case VillagerProfession::Toolsmith: return "toolsmith";
        case VillagerProfession::Butcher: return "butcher";
        case VillagerProfession::Leatherworker: return "leatherworker";
        case VillagerProfession::Mason: return "mason";
        case VillagerProfession::Count: break;
    }
    return "unknown";
}

bool professionEmployable(VillagerProfession profession) {
    return profession != VillagerProfession::Unemployed && profession != VillagerProfession::Nitwit;
}

Block workstationForProfession(VillagerProfession profession) {
    switch (profession) {
        case VillagerProfession::Farmer: return Block::Composter;
        case VillagerProfession::Fisherman: return Block::Barrel;
        case VillagerProfession::Shepherd: return Block::Loom;
        case VillagerProfession::Fletcher: return Block::CraftingTable;
        case VillagerProfession::Librarian: return Block::Lectern;
        case VillagerProfession::Cartographer: return Block::CartographyTable;
        case VillagerProfession::Cleric: return Block::BrewingStand;
        case VillagerProfession::Armorer: return Block::Furnace;
        case VillagerProfession::Weaponsmith: return Block::Grindstone;
        case VillagerProfession::Toolsmith: return Block::SmithingTable;
        case VillagerProfession::Butcher: return Block::FurnaceLit;
        case VillagerProfession::Leatherworker: return Block::Cauldron;
        case VillagerProfession::Mason: return Block::Stonecutter;
        default: break;
    }
    return Block::Air;
}

VillagerProfession professionForWorkstation(Block block) {
    switch (block) {
        case Block::Composter: return VillagerProfession::Farmer;
        case Block::Barrel: return VillagerProfession::Fisherman;
        case Block::Loom: return VillagerProfession::Shepherd;
        case Block::CraftingTable: return VillagerProfession::Fletcher;
        case Block::Lectern: return VillagerProfession::Librarian;
        case Block::CartographyTable: return VillagerProfession::Cartographer;
        case Block::BrewingStand: return VillagerProfession::Cleric;
        case Block::Furnace: return VillagerProfession::Armorer;
        case Block::Grindstone: return VillagerProfession::Weaponsmith;
        case Block::SmithingTable: return VillagerProfession::Toolsmith;
        case Block::FurnaceLit: return VillagerProfession::Butcher;
        case Block::Cauldron: return VillagerProfession::Leatherworker;
        case Block::Stonecutter: return VillagerProfession::Mason;
        default: break;
    }
    return VillagerProfession::Unemployed;
}

bool isWorkstationBlock(Block block) {
    return professionForWorkstation(block) != VillagerProfession::Unemployed;
}

int villagerExperienceForLevel(int level) {
    if (level <= 1) return 0;
    if (level >= villagerMaxLevel) return levelThresholds[villagerMaxLevel - 1];
    return levelThresholds[level - 1];
}

int villagerLevelForExperience(int experience) {
    int level = 1;
    for (int candidate = 2; candidate <= villagerMaxLevel; ++candidate)
        if (experience >= levelThresholds[candidate - 1]) level = candidate;
    return level;
}

const char* villagerActivityName(VillagerActivity activity) {
    switch (activity) {
        case VillagerActivity::Idle: return "idle";
        case VillagerActivity::SeekWorkstation: return "seekWorkstation";
        case VillagerActivity::Work: return "work";
        case VillagerActivity::Meet: return "meet";
        case VillagerActivity::Sleep: return "sleep";
        case VillagerActivity::Panic: return "panic";
        case VillagerActivity::Count: break;
    }
    return "unknown";
}

void PoiRegistry::scan(const World& world, const Vec3i& center, int radius, int verticalRadius,
                       const PathConfig& config) {
    clear();
    for (int x = center.x - radius; x <= center.x + radius; ++x) {
        for (int y = center.y - verticalRadius; y <= center.y + verticalRadius; ++y) {
            for (int z = center.z - radius; z <= center.z + radius; ++z) {
                Block block = world.blockAt(x, y, z);
                if (!poiBlock(block)) continue;
                WorkstationPoi poi;
                poi.pos = {x, y, z};
                poi.block = block;
                poi.profession = professionForWorkstation(block);
                poi.valid = true;
                poi.reachable = resolveStandCell(world, poi.pos, block, config, poi.standCell);
                if (block == Block::Bell && bell < 0) bell = static_cast<int>(pois.size());
                pois.push_back(poi);
            }
        }
    }
}

int PoiRegistry::indexAt(const Vec3i& pos) const {
    for (std::size_t i = 0; i < pois.size(); ++i)
        if (pois[i].valid && pois[i].pos == pos) return static_cast<int>(i);
    return -1;
}

int PoiRegistry::workstationCount() const { return countPois(false, false); }

int PoiRegistry::bedCount() const { return countPois(true, false); }

int PoiRegistry::freeWorkstationCount() const { return countPois(false, true); }

int PoiRegistry::freeBedCount() const { return countPois(true, true); }

int PoiRegistry::countPois(bool beds, bool onlyFree) const {
    int count = 0;
    for (const WorkstationPoi& poi : pois) {
        if (beds ? !poi.isBed() : !poi.isWorkstation()) continue;
        if (onlyFree ? !poi.free() : !poi.valid) continue;
        ++count;
    }
    return count;
}

int PoiRegistry::nearestFree(const Vec3i& from, bool beds, VillagerProfession wanted) const {
    int best = -1;
    int bestDistance = 0;
    for (std::size_t i = 0; i < pois.size(); ++i) {
        const WorkstationPoi& poi = pois[i];
        if (!poi.free() || !poi.reachable) continue;
        if (beds ? !poi.isBed() : !poi.isWorkstation()) continue;
        if (!beds && wanted != VillagerProfession::Unemployed && poi.profession != wanted) continue;
        int distance = squaredDistance(poi.standCell, from);
        if (best < 0 || distance < bestDistance) {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

int PoiRegistry::nearestFreeWorkstation(const Vec3i& from, VillagerProfession wanted) const {
    return nearestFree(from, false, wanted);
}

int PoiRegistry::nearestFreeBed(const Vec3i& from) const {
    return nearestFree(from, true, VillagerProfession::Unemployed);
}

bool PoiRegistry::claim(int index, int villagerId) {
    if (index < 0 || index >= size()) return false;
    WorkstationPoi& poi = pois[index];
    if (!poi.valid || poi.ownerId >= 0) return false;
    poi.ownerId = villagerId;
    return true;
}

void PoiRegistry::release(int index) {
    if (index < 0 || index >= size()) return;
    pois[index].ownerId = -1;
}

void PoiRegistry::releaseOwner(int villagerId) {
    for (WorkstationPoi& poi : pois)
        if (poi.ownerId == villagerId) poi.ownerId = -1;
}

int PoiRegistry::revalidate(const World& world, const PathConfig& config) {
    int lost = 0;
    for (std::size_t i = 0; i < pois.size(); ++i) {
        WorkstationPoi& poi = pois[i];
        if (!poi.valid) continue;
        if (world.blockAt(poi.pos.x, poi.pos.y, poi.pos.z) != poi.block) {
            poi.valid = false;
            poi.ownerId = -1;
            if (bell == static_cast<int>(i)) bell = -1;
            ++lost;
            continue;
        }
        poi.reachable = resolveStandCell(world, poi.pos, poi.block, config, poi.standCell);
    }
    return lost;
}

void PoiRegistry::clear() {
    pois.clear();
    bell = -1;
}

Vec3i Villager::cell() const {
    return {static_cast<int>(std::floor(body.position.x)),
            static_cast<int>(std::floor(body.position.y)),
            static_cast<int>(std::floor(body.position.z))};
}

Villager makeVillager(int id, const Vec3& position, uint32_t seed) {
    Villager villager;
    villager.id = id;
    villager.body.position = position;
    villager.body.width = 0.6f;
    villager.body.height = 1.95f;
    villager.rng = seed ^ (static_cast<uint32_t>(id) * 0x27d4eb2du);
    uint32_t draw = nextRandom(villager.rng);
    if (draw % 8u == 0u) villager.profession = VillagerProfession::Nitwit;
    return villager;
}

bool assignVillagerWorkstation(Villager& villager, PoiRegistry& pois) {
    if (!villager.alive || villager.profession == VillagerProfession::Nitwit) return false;
    if (villager.workstationIndex >= 0) return false;
    int index = pois.nearestFreeWorkstation(villager.cell(), villager.profession);
    if (index < 0) return false;
    if (!pois.claim(index, villager.id)) return false;
    villager.workstationIndex = index;
    if (villager.profession == VillagerProfession::Unemployed)
        villager.profession = pois.at(index).profession;
    return true;
}

bool assignVillagerBed(Villager& villager, PoiRegistry& pois) {
    if (!villager.alive || villager.bedIndex >= 0) return false;
    int index = pois.nearestFreeBed(villager.cell());
    if (index < 0) return false;
    if (!pois.claim(index, villager.id)) return false;
    villager.bedIndex = index;
    return true;
}

void releaseVillagerPois(Villager& villager, PoiRegistry& pois) {
    pois.release(villager.workstationIndex);
    pois.release(villager.bedIndex);
    villager.workstationIndex = -1;
    villager.bedIndex = -1;
    villager.targetPoiIndex = -1;
}

bool syncVillagerPois(Villager& villager, PoiRegistry& pois) {
    bool professionChanged = false;
    if (villager.workstationIndex >= 0) {
        const WorkstationPoi& poi = pois.at(villager.workstationIndex);
        if (!poi.valid || poi.ownerId != villager.id) {
            villager.workstationIndex = -1;
            if (villager.level <= 1) {
                villager.profession = VillagerProfession::Unemployed;
                villager.experience = 0;
                professionChanged = true;
            }
        }
    }
    if (villager.bedIndex >= 0) {
        const WorkstationPoi& bed = pois.at(villager.bedIndex);
        if (!bed.valid || bed.ownerId != villager.id) villager.bedIndex = -1;
    }
    return professionChanged;
}

bool grantVillagerExperience(Villager& villager, int amount) {
    if (amount <= 0) return false;
    int before = villager.level;
    villager.experience += amount;
    villager.level = villagerLevelForExperience(villager.experience);
    return villager.level > before;
}

void killVillager(Villager& villager, PoiRegistry& pois) {
    villager.alive = false;
    villager.health = 0.0f;
    villager.state = VillagerState::Dead;
    villager.activity = VillagerActivity::Idle;
    villager.path.clear();
    villager.pathCursor = 0;
    villager.hasGoal = false;
    releaseVillagerPois(villager, pois);
}

bool damageVillager(Villager& villager, PoiRegistry& pois, float amount) {
    if (!villager.alive) return false;
    villager.health -= amount;
    if (villager.health > 0.0f) return false;
    killVillager(villager, pois);
    return true;
}

VillagerActivity VillagerSchedule::activityAt(double dayFraction) const {
    if (dayFraction < wakeFraction) return VillagerActivity::Sleep;
    if (dayFraction < workStart) return VillagerActivity::Idle;
    if (dayFraction < meetStart) return VillagerActivity::Work;
    if (dayFraction < meetEnd) return VillagerActivity::Meet;
    if (dayFraction < workEnd) return VillagerActivity::Work;
    if (dayFraction < sleepStart) return VillagerActivity::Idle;
    return VillagerActivity::Sleep;
}

VillagerActivity chooseVillagerActivity(const Villager& villager, const VillagerSchedule& schedule,
                                        const TimeOfDay& clock, bool villageThreatened) {
    if (!villager.alive) return VillagerActivity::Idle;
    if (villageThreatened) return VillagerActivity::Panic;

    VillagerActivity planned = schedule.activityAt(clock.dayFraction());
    switch (planned) {
        case VillagerActivity::Work:
            if (villager.profession == VillagerProfession::Nitwit) return VillagerActivity::Idle;
            if (villager.workstationIndex < 0) return VillagerActivity::SeekWorkstation;
            return VillagerActivity::Work;
        case VillagerActivity::Sleep:
            if (villager.bedIndex < 0) return VillagerActivity::Idle;
            return VillagerActivity::Sleep;
        default:
            return planned;
    }
}

bool villagerGoalFor(const Villager& villager, const PoiRegistry& pois, const Vec3i& villageCenter,
                     VillagerActivity activity, Vec3i& outGoal, int& outPoiIndex) {
    outPoiIndex = -1;
    switch (activity) {
        case VillagerActivity::Work:
            if (villager.workstationIndex < 0) return false;
            outPoiIndex = villager.workstationIndex;
            outGoal = pois.at(outPoiIndex).standCell;
            return true;
        case VillagerActivity::SeekWorkstation: {
            int index = pois.nearestFreeWorkstation(villager.cell(), villager.profession);
            if (index < 0) return false;
            outPoiIndex = index;
            outGoal = pois.at(index).standCell;
            return true;
        }
        case VillagerActivity::Meet: {
            int index = pois.bellIndex();
            if (index >= 0 && pois.at(index).valid) {
                outPoiIndex = index;
                outGoal = pois.at(index).standCell;
            } else {
                outGoal = villageCenter;
            }
            return true;
        }
        case VillagerActivity::Sleep:
            if (villager.bedIndex < 0) return false;
            outPoiIndex = villager.bedIndex;
            outGoal = pois.at(outPoiIndex).standCell;
            return true;
        case VillagerActivity::Panic:
            if (villager.bedIndex >= 0 && pois.at(villager.bedIndex).valid) {
                outPoiIndex = villager.bedIndex;
                outGoal = pois.at(outPoiIndex).standCell;
            } else {
                outGoal = villageCenter;
            }
            return true;
        default:
            return false;
    }
}

bool repathVillager(const World& world, Villager& villager, const Vec3i& goal,
                    const PathConfig& config) {
    std::vector<Vec3i> route = findPath(world, villager.cell(), goal, config);
    if (route.empty()) {
        villager.hasGoal = false;
        return false;
    }
    villager.path = std::move(route);
    villager.pathCursor = 0;
    villager.goalCell = goal;
    villager.hasGoal = true;
    villager.state = villager.path.size() == 1 ? VillagerState::AtTarget : VillagerState::Travelling;
    return true;
}

bool stepVillagerAlongPath(Villager& villager, float speed, float dt) {
    if (villager.path.empty()) return false;
    int last = static_cast<int>(villager.path.size()) - 1;
    float budget = speed * dt;
    while (budget > 0.0f) {
        Vec3 target = cellCentre(villager.path[villager.pathCursor]);
        Vec3 delta = target - villager.body.position;
        float remaining = length(delta);
        if (remaining <= budget) {
            villager.body.position = target;
            budget -= remaining;
            if (villager.pathCursor >= last) {
                villager.state = VillagerState::AtTarget;
                return true;
            }
            ++villager.pathCursor;
        } else {
            villager.body.position += normalize(delta) * budget;
            villager.state = VillagerState::Travelling;
            return false;
        }
    }
    villager.state = VillagerState::Travelling;
    return false;
}

void tickVillager(const World& world, Villager& villager, PoiRegistry& pois, const TimeOfDay& clock,
                  const VillagerSchedule& schedule, const Vec3i& villageCenter,
                  bool villageThreatened, float dt, const PathConfig& config) {
    if (!villager.alive) {
        villager.state = VillagerState::Dead;
        return;
    }

    syncVillagerPois(villager, pois);
    settleVillager(world, villager, config);
    if (villager.bedIndex < 0) assignVillagerBed(villager, pois);

    VillagerActivity next = chooseVillagerActivity(villager, schedule, clock, villageThreatened);
    if (next != villager.activity) {
        villager.activity = next;
        villager.hasGoal = false;
        villager.path.clear();
        villager.pathCursor = 0;
        villager.state = VillagerState::Idle;
    }

    Vec3i goal;
    int poiIndex = -1;
    if (!villagerGoalFor(villager, pois, villageCenter, next, goal, poiIndex)) {
        villager.hasGoal = false;
        villager.path.clear();
        villager.pathCursor = 0;
        villager.targetPoiIndex = -1;
        villager.state = VillagerState::Idle;
        return;
    }

    if (!villager.hasGoal || villager.goalCell != goal) {
        villager.targetPoiIndex = poiIndex;
        repathVillager(world, villager, goal, config);
    }

    bool arrived = stepVillagerAlongPath(villager, villagerWalkSpeed, dt);
    if (!arrived && villager.state != VillagerState::AtTarget) return;

    switch (next) {
        case VillagerActivity::Work:
            ++villager.workTicks;
            break;
        case VillagerActivity::SeekWorkstation:
            if (villager.targetPoiIndex >= 0 && pois.at(villager.targetPoiIndex).free()) {
                pois.claim(villager.targetPoiIndex, villager.id);
                villager.workstationIndex = villager.targetPoiIndex;
                if (villager.profession == VillagerProfession::Unemployed)
                    villager.profession = pois.at(villager.targetPoiIndex).profession;
            } else {
                villager.hasGoal = false;
            }
            break;
        default:
            break;
    }
}

bool exchangeVillagerGossip(Villager& first, Villager& second) {
    if (!first.alive || !second.alive || first.id == second.id) return false;
    Vec3 delta = first.body.position - second.body.position;
    if (length(delta) > villagerGossipRange) return false;
    bool changed = first.gossip.shareInto(second.gossip);
    changed = second.gossip.shareInto(first.gossip) || changed;
    return changed;
}

int spreadVillageGossip(std::vector<Villager>& villagers) {
    int exchanges = 0;
    for (std::size_t i = 0; i < villagers.size(); ++i)
        for (std::size_t j = i + 1; j < villagers.size(); ++j)
            if (exchangeVillagerGossip(villagers[i], villagers[j])) ++exchanges;
    return exchanges;
}

VillageCensus takeVillageCensus(const std::vector<Villager>& villagers, const PoiRegistry& pois,
                                const Vec3i& center, int radius) {
    VillageCensus census;
    for (const Villager& villager : villagers) {
        if (!withinVillage(villager.cell(), center, radius)) continue;
        ++census.villagers;
        if (!villager.alive) continue;
        ++census.living;
        if (villager.employed()) ++census.employed;
        census.foodStock += villager.food;
        if (villager.food >= villagerBreedingFoodCost) ++census.willingBreeders;
    }
    for (int i = 0; i < pois.size(); ++i) {
        const WorkstationPoi& poi = pois.at(i);
        if (!poi.valid || !withinVillage(poi.pos, center, radius)) continue;
        if (poi.isBed()) {
            ++census.beds;
            if (poi.free()) ++census.freeBeds;
        } else if (poi.isWorkstation()) {
            ++census.workstations;
            if (poi.free()) ++census.freeWorkstations;
        }
    }
    return census;
}

int breedVillagers(std::vector<Villager>& villagers, PoiRegistry& pois, const Vec3i& center,
                   int radius, uint32_t& rng) {
    int born = 0;
    while (takeVillageCensus(villagers, pois, center, radius).canGrow()) {
        int firstParent = -1;
        int secondParent = -1;
        for (std::size_t i = 0; i < villagers.size(); ++i) {
            const Villager& candidate = villagers[i];
            if (!candidate.alive || candidate.food < villagerBreedingFoodCost) continue;
            if (!withinVillage(candidate.cell(), center, radius)) continue;
            if (firstParent < 0) firstParent = static_cast<int>(i);
            else if (secondParent < 0) secondParent = static_cast<int>(i);
        }
        if (secondParent < 0) break;

        villagers[firstParent].food -= villagerBreedingFoodCost;
        villagers[secondParent].food -= villagerBreedingFoodCost;

        Vec3 spawnPoint = (villagers[firstParent].body.position +
                           villagers[secondParent].body.position) * 0.5f;
        int nextId = 0;
        for (const Villager& existing : villagers)
            if (existing.id >= nextId) nextId = existing.id + 1;

        Villager child = makeVillager(nextId, spawnPoint, nextRandom(rng));
        if (!assignVillagerBed(child, pois)) break;
        villagers.push_back(child);
        ++born;
    }
    return born;
}

}
