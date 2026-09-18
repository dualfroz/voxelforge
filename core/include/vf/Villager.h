#pragma once

#include <cstdint>
#include <vector>
#include "Block.h"
#include "Entity.h"
#include "Gossip.h"
#include "Math.h"
#include "Pathfind.h"
#include "TimeOfDay.h"
#include "World.h"

namespace vf {

enum class VillagerProfession : uint8_t {
    Unemployed = 0,
    Nitwit,
    Farmer,
    Fisherman,
    Shepherd,
    Fletcher,
    Librarian,
    Cartographer,
    Cleric,
    Armorer,
    Weaponsmith,
    Toolsmith,
    Butcher,
    Leatherworker,
    Mason,
    Count,
};

const char* villagerProfessionName(VillagerProfession profession);
bool professionEmployable(VillagerProfession profession);

Block workstationForProfession(VillagerProfession profession);
VillagerProfession professionForWorkstation(Block block);
bool isWorkstationBlock(Block block);

constexpr int villagerMaxLevel = 5;

int villagerExperienceForLevel(int level);
int villagerLevelForExperience(int experience);

enum class VillagerActivity : uint8_t {
    Idle = 0,
    SeekWorkstation,
    Work,
    Meet,
    Sleep,
    Panic,
    Count,
};

const char* villagerActivityName(VillagerActivity activity);

enum class VillagerState : uint8_t {
    Idle = 0,
    Travelling,
    AtTarget,
    Dead,
    Count,
};

struct WorkstationPoi {
    Vec3i pos;
    Vec3i standCell;
    Block block = Block::Air;
    VillagerProfession profession = VillagerProfession::Unemployed;
    int ownerId = -1;
    bool reachable = false;
    bool valid = false;

    bool isBed() const { return block == Block::Bed; }
    bool isBell() const { return block == Block::Bell; }
    bool isWorkstation() const { return profession != VillagerProfession::Unemployed; }
    bool free() const { return valid && ownerId < 0; }
};

class PoiRegistry {
public:
    void scan(const World& world, const Vec3i& center, int radius, int verticalRadius,
              const PathConfig& config = {});

    int size() const { return static_cast<int>(pois.size()); }
    const WorkstationPoi& at(int index) const { return pois[index]; }
    int indexAt(const Vec3i& pos) const;
    int bellIndex() const { return bell; }

    int workstationCount() const;
    int bedCount() const;
    int freeWorkstationCount() const;
    int freeBedCount() const;

    int nearestFreeWorkstation(const Vec3i& from,
                               VillagerProfession wanted = VillagerProfession::Unemployed) const;
    int nearestFreeBed(const Vec3i& from) const;

    bool claim(int index, int villagerId);
    void release(int index);
    void releaseOwner(int villagerId);

    int revalidate(const World& world, const PathConfig& config = {});

    void clear();

private:
    int countPois(bool beds, bool onlyFree) const;
    int nearestFree(const Vec3i& from, bool beds, VillagerProfession wanted) const;

    std::vector<WorkstationPoi> pois;
    int bell = -1;
};

struct Villager {
    int id = 0;
    Mob body;
    VillagerProfession profession = VillagerProfession::Unemployed;
    int level = 1;
    int experience = 0;
    VillagerActivity activity = VillagerActivity::Idle;
    VillagerState state = VillagerState::Idle;
    int workstationIndex = -1;
    int bedIndex = -1;
    int targetPoiIndex = -1;
    Vec3i goalCell;
    bool hasGoal = false;
    std::vector<Vec3i> path;
    int pathCursor = 0;
    GossipLedger gossip;
    int food = 0;
    int workTicks = 0;
    int restocksToday = 0;
    long long lastRestockDay = -1;
    float health = 20.0f;
    bool alive = true;
    uint32_t rng = 0x9e3779b9u;

    Vec3i cell() const;
    bool employed() const { return profession != VillagerProfession::Unemployed &&
                                   profession != VillagerProfession::Nitwit; }
};

Villager makeVillager(int id, const Vec3& position, uint32_t seed);

constexpr float villagerWalkSpeed = 2.4f;
constexpr int villagerExperiencePerTrade = 2;

bool assignVillagerWorkstation(Villager& villager, PoiRegistry& pois);
bool assignVillagerBed(Villager& villager, PoiRegistry& pois);
void releaseVillagerPois(Villager& villager, PoiRegistry& pois);

bool syncVillagerPois(Villager& villager, PoiRegistry& pois);

bool grantVillagerExperience(Villager& villager, int amount);

bool damageVillager(Villager& villager, PoiRegistry& pois, float amount);
void killVillager(Villager& villager, PoiRegistry& pois);

struct VillagerSchedule {
    double wakeFraction = 0.24;
    double workStart = 0.30;
    double meetStart = 0.46;
    double meetEnd = 0.54;
    double workEnd = 0.68;
    double sleepStart = 0.75;

    VillagerActivity activityAt(double dayFraction) const;
};

VillagerActivity chooseVillagerActivity(const Villager& villager, const VillagerSchedule& schedule,
                                        const TimeOfDay& clock, bool villageThreatened);

bool villagerGoalFor(const Villager& villager, const PoiRegistry& pois, const Vec3i& villageCenter,
                     VillagerActivity activity, Vec3i& outGoal, int& outPoiIndex);

bool repathVillager(const World& world, Villager& villager, const Vec3i& goal,
                    const PathConfig& config = {});

bool stepVillagerAlongPath(Villager& villager, float speed, float dt);

void tickVillager(const World& world, Villager& villager, PoiRegistry& pois, const TimeOfDay& clock,
                  const VillagerSchedule& schedule, const Vec3i& villageCenter,
                  bool villageThreatened, float dt, const PathConfig& config = {});

constexpr float villagerGossipRange = 3.0f;
bool exchangeVillagerGossip(Villager& first, Villager& second);
int spreadVillageGossip(std::vector<Villager>& villagers);

constexpr int villagerBreedingFoodCost = 3;

struct VillageCensus {
    int villagers = 0;
    int living = 0;
    int employed = 0;
    int beds = 0;
    int freeBeds = 0;
    int workstations = 0;
    int freeWorkstations = 0;
    int foodStock = 0;
    int willingBreeders = 0;

    bool canGrow() const {
        return living >= 2 && freeBeds > 0 && willingBreeders >= 2;
    }
    int capacity() const { return beds; }
};

VillageCensus takeVillageCensus(const std::vector<Villager>& villagers, const PoiRegistry& pois,
                                const Vec3i& center, int radius);

int breedVillagers(std::vector<Villager>& villagers, PoiRegistry& pois, const Vec3i& center,
                   int radius, uint32_t& rng);

}
