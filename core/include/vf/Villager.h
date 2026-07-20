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

// A villager's trade specialisation. Unemployed and Nitwit never trade: the
// first is looking for a workstation, the second never takes one.
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

// Bidirectional map between a profession and the block that grants it.
Block workstationForProfession(VillagerProfession profession);
VillagerProfession professionForWorkstation(Block block);
bool isWorkstationBlock(Block block);

constexpr int villagerMaxLevel = 5;

// Total experience needed to reach `level`. Level 1 starts at zero.
int villagerExperienceForLevel(int level);
int villagerLevelForExperience(int experience);

// What the villager is trying to do right now. The schedule picks it from the
// clock; a raid overrides everything with Panic.
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

// How the villager is executing that intent.
enum class VillagerState : uint8_t {
    Idle = 0,
    Travelling,
    AtTarget,
    Dead,
    Count,
};

// A claimable point of interest: a workstation block, a bed or the village
// bell. Exactly one villager may own it at a time. Entries are never erased
// once scanned, only invalidated, so an index stays valid for the whole life of
// the registry.
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

// Index of every workstation, bed and bell in a village. Scanning is a pure
// function of the world contents, and results are ordered by cell coordinate so
// claim order never depends on iteration luck.
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

    // Nearest free entry, measured from `from` by squared distance with a
    // scan-order tie-break. VillagerProfession::Unemployed as `wanted` accepts
    // any workstation. Returns -1 when none is free.
    int nearestFreeWorkstation(const Vec3i& from,
                               VillagerProfession wanted = VillagerProfession::Unemployed) const;
    int nearestFreeBed(const Vec3i& from) const;

    bool claim(int index, int villagerId);
    void release(int index);
    void releaseOwner(int villagerId);

    // Re-reads the world and invalidates entries whose block is gone, dropping
    // their owner. Returns how many entries were invalidated.
    int revalidate(const World& world, const PathConfig& config = {});

    void clear();

private:
    int countPois(bool beds, bool onlyFree) const;
    int nearestFree(const Vec3i& from, bool beds, VillagerProfession wanted) const;

    std::vector<WorkstationPoi> pois;
    int bell = -1;
};

// A villager: the physics body from Entity.h, a job, a day plan, the two points
// of interest it owns and its own memory of the player.
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

