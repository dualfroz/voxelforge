#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "World.h"
#include "Math.h"
#include "Block.h"
#include "TimeOfDay.h"
#include "Weather.h"
#include "Farming.h"
#include "BiomeGen.h"
#include "RandomTick.h"
#include "BlockTick.h"
#include "Container.h"
#include "Circuit.h"
#include "Hopper.h"
#include "Dispenser.h"
#include "Minecart.h"
#include "Rail.h"
#include "Mob.h"

namespace vf {

// Climate carries its own vf::BlockChange that collides with the one in
// Protocol.h. Kept out of this header (forward declared, held by pointer) so a
// translation unit can pull WorldSim together with ChunkSync/Protocol.
class Climate;

// Mirror of ClimateConfig's tunables, so callers can shape the climate without
// pulling Climate.h (and its clashing BlockChange) into their translation unit.
struct WorldClimateSettings {
    int seaLevel = 26;
    float heightLapse = 0.006f;
    float latitudeLapse = 0.0009f;
    float freezeTemperature = 0.15f;
    float meltTemperature = 0.5f;
    float frostSpreadTemperature = 0.30f;
    int maxSnowDepth = 8;
    int packThreshold = 4;
};

struct WorldSimConfig {
    uint32_t worldSeed = 1u;

    // Farming random ticks dispatched by the WorldSim's own selector.
    int randomTicksPerChunk = 3;
    // Vanilla block random ticks handled by the reused BlockTickScheduler.
    int blockRandomTicksPerChunk = 3;

    long long weatherIntervalTicks = 1;
    long long climateIntervalTicks = 20;
    long long logisticsIntervalTicks = 1;

    float mobDt = 1.0f / 20.0f;

    WorldTick startTick = 0;

    TimeOfDayConfig time;
    WeatherConfig weather;
    WorldClimateSettings climate;
    FarmingConfig farming;
    BiomeGenParams biome;
    CartConfig cart;
    CreatureConfig creature;
};

// Deterministic orchestrator that folds the standalone simulation modules into
// one step. Every tick advances the clock, periodically the weather and the
// regional climate, dispatches random ticks to farming and vanilla block
// behaviour, runs redstone, logistics (hoppers/dispensers), minecarts and
// creatures, then collects the block cells that changed inside the active
// region as a dirty feed for chunk replication. Given the same seed and the
// same inputs, two WorldSims produce byte-identical world state.
class WorldSim {
public:
    WorldSim(World& world, const Vec3i& regionOrigin, const Vec3i& regionDims,
             const WorldSimConfig& config = {});
    ~WorldSim();

    WorldSim(const WorldSim&) = delete;
    WorldSim& operator=(const WorldSim&) = delete;
    WorldSim(WorldSim&&) = delete;
    WorldSim& operator=(WorldSim&&) = delete;

    void step(long long ticks = 1);

    long long currentTick() const { return tickCounter; }
    const Vec3i& regionOrigin() const { return origin; }
    const Vec3i& regionDims() const { return dims; }

    const TimeOfDay& time() const { return clock; }
    const Weather& weather() const { return sky; }
    Farming& farming() { return crops; }
    const Farming& farming() const { return crops; }
    RedstoneCircuit& circuit() { return *redstone; }
    ContainerStore& containers() { return store; }
    const ContainerStore& containers() const { return store; }

    // Climate readouts exposed without leaking Climate.h into callers.
    int snowDepthAt(const Vec3i& pos) const;
    int iceAgeAt(const Vec3i& pos) const;
    float temperatureAt(int gx, int gy, int gz) const;
    Precipitation currentPrecipitation() const;

    void addMinecart(const Minecart& cart);
    std::vector<Minecart>& minecarts() { return carts; }
    const std::vector<Minecart>& minecarts() const { return carts; }
    void setPoweredRailPredicate(std::function<bool(const Vec3i&)> predicate);

    void addCreature(const Creature& creature);
    std::vector<Creature>& creatures() { return mobs; }
    const std::vector<Creature>& creatures() const { return mobs; }
    void setPlayer(const Vec3& pos);
    void clearPlayer();

    // Global cells whose block changed versus the last clearDirty (or since
    // construction), in a deterministic sorted order.
    const std::vector<Vec3i>& dirtyBlocks() const { return dirtyList; }
    void clearDirty();

    uint64_t regionHash() const;

private:
    void runFarmingRandomTicks();
    void runClimate();
    void runLogistics();
    void runMinecarts();
    void runCreatures();
    void recomputeDirty();

    int localIndexOf(int gx, int gy, int gz) const;
    Block baselineBlock(int localIdx) const { return baseline[static_cast<std::size_t>(localIdx)]; }

    World& world;
    Vec3i origin;
    Vec3i dims;

    long long tickCounter;
    long long weatherInterval;
    long long climateInterval;
    long long logisticsInterval;
    float mobDt;

    TimeOfDay clock;
    Weather sky;
    BiomeGen biomes;
    std::unique_ptr<Climate> weatherClimate;
    Farming crops;
    RandomTick randomTick;
    BlockTickScheduler scheduler;
    ContainerStore store;
    std::unique_ptr<RedstoneCircuit> redstone;
    std::unordered_map<Vec3i, HopperState, Vec3iHash> hopperStates;
    std::unordered_map<Vec3i, DispenserState, Vec3iHash> dispenserStates;

    CartConfig cartConfig;
    CreatureConfig creatureConfig;
    std::vector<Minecart> carts;
    std::function<bool(const Vec3i&)> poweredRail;

    std::vector<Creature> mobs;
    Vec3 player;
    bool hasPlayer = false;

    std::vector<Block> baseline;
    std::vector<Vec3i> dirtyList;
};

} // namespace vf
