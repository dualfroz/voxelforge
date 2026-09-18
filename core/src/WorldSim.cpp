#include "vf/WorldSim.h"
#include "vf/Climate.h"

namespace vf {

namespace {

long long atLeastOne(long long value) { return value < 1 ? 1 : value; }

ClimateConfig toClimateConfig(const WorldClimateSettings& settings) {
    ClimateConfig config;
    config.seaLevel = settings.seaLevel;
    config.heightLapse = settings.heightLapse;
    config.latitudeLapse = settings.latitudeLapse;
    config.freezeTemperature = settings.freezeTemperature;
    config.meltTemperature = settings.meltTemperature;
    config.frostSpreadTemperature = settings.frostSpreadTemperature;
    config.maxSnowDepth = settings.maxSnowDepth;
    config.packThreshold = settings.packThreshold;
    return config;
}

uint64_t fnv1a(uint64_t hash, uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ull;
    return hash;
}

}

WorldSim::WorldSim(World& world, const Vec3i& regionOrigin, const Vec3i& regionDims,
                   const WorldSimConfig& config)
    : world(world),
      origin(regionOrigin),
      dims(regionDims),
      tickCounter(0),
      weatherInterval(atLeastOne(config.weatherIntervalTicks)),
      climateInterval(atLeastOne(config.climateIntervalTicks)),
      logisticsInterval(atLeastOne(config.logisticsIntervalTicks)),
      mobDt(config.mobDt),
      clock(config.startTick, config.time),
      sky(config.worldSeed, config.weather),
      biomes(config.biome),
      weatherClimate(std::make_unique<Climate>(biomes, toClimateConfig(config.climate))),
      crops(config.worldSeed, config.farming),
      randomTick(config.worldSeed, config.randomTicksPerChunk),
      scheduler(config.worldSeed, config.blockRandomTicksPerChunk),
      redstone(std::make_unique<RedstoneCircuit>(world, regionOrigin, regionDims)),
      cartConfig(config.cart),
      creatureConfig(config.creature),
      poweredRail([](const Vec3i&) { return false; }) {
    scheduler.setRandomTickHook([](World& w, const Vec3i& pos, Block block) {
        if (block == Block::Dirt) tryGrowGrass(w, pos);
    });

    std::size_t volume = static_cast<std::size_t>(dims.x) * static_cast<std::size_t>(dims.y) *
                         static_cast<std::size_t>(dims.z);
    baseline.resize(volume);
    clearDirty();
}

WorldSim::~WorldSim() = default;

int WorldSim::localIndexOf(int gx, int gy, int gz) const {
    int lx = gx - origin.x;
    int ly = gy - origin.y;
    int lz = gz - origin.z;
    return (ly * dims.z + lz) * dims.x + lx;
}

void WorldSim::step(long long ticks) {
    for (long long i = 0; i < ticks; ++i) {
        ++tickCounter;
        clock.advance(1);

        if (tickCounter % weatherInterval == 0) sky.advance(weatherInterval);

        runFarmingRandomTicks();
        scheduler.tick(world);

        if (tickCounter % climateInterval == 0) runClimate();

        redstone->tick();
        redstone->applyLamps();

        if (tickCounter % logisticsInterval == 0) runLogistics();

        runMinecarts();
        runCreatures();
    }
    recomputeDirty();
}

void WorldSim::runFarmingRandomTicks() {
    int skyLight = clock.skyLightLevel();
    Vec3i chunkMin{floorDiv(origin.x, chunkSize), floorDiv(origin.y, chunkSize),
                   floorDiv(origin.z, chunkSize)};
    Vec3i chunkMax{floorDiv(origin.x + dims.x - 1, chunkSize),
                   floorDiv(origin.y + dims.y - 1, chunkSize),
                   floorDiv(origin.z + dims.z - 1, chunkSize)};

    std::vector<Vec3i> cells;
    for (int cx = chunkMin.x; cx <= chunkMax.x; ++cx)
        for (int cy = chunkMin.y; cy <= chunkMax.y; ++cy)
            for (int cz = chunkMin.z; cz <= chunkMax.z; ++cz) {
                randomTick.sampleGlobal(Vec3i{cx, cy, cz}, tickCounter, cells);
                for (const Vec3i& cell : cells) crops.randomTick(world, cell, skyLight);
            }
}

void WorldSim::runClimate() {
    Precipitation precip = currentPrecipitation();
    std::vector<ClimateChange> changes = weatherClimate->tick(world, origin, dims, precip);
    Climate::apply(world, changes);
}

void WorldSim::runLogistics() {
    for (int gx = origin.x; gx < origin.x + dims.x; ++gx)
        for (int gy = origin.y; gy < origin.y + dims.y; ++gy)
            for (int gz = origin.z; gz < origin.z + dims.z; ++gz) {
                Vec3i pos{gx, gy, gz};
                switch (world.blockAt(gx, gy, gz)) {
                    case Block::Hopper:
                        tickHopper(world, pos, hopperStates[pos], store);
                        break;
                    case Block::Dispenser:
                        tickDispenser(world, pos, dispenserStates[pos], store, *redstone);
                        break;
                    case Block::Dropper:
                        tickDropper(world, pos, dispenserStates[pos], store, *redstone);
                        break;
                    default:
                        break;
                }
            }
}

void WorldSim::runMinecarts() {
    if (carts.empty()) return;
    stepMinecarts(world, carts, cartConfig, poweredRail, store);
}

void WorldSim::runCreatures() {
    for (Creature& creature : mobs)
        updateCreature(world, creature, player, hasPlayer, creatureConfig, mobDt);
}

void WorldSim::recomputeDirty() {
    dirtyList.clear();
    for (int gx = origin.x; gx < origin.x + dims.x; ++gx)
        for (int gy = origin.y; gy < origin.y + dims.y; ++gy)
            for (int gz = origin.z; gz < origin.z + dims.z; ++gz) {
                int idx = localIndexOf(gx, gy, gz);
                if (world.blockAt(gx, gy, gz) != baselineBlock(idx))
                    dirtyList.push_back(Vec3i{gx, gy, gz});
            }
}

void WorldSim::clearDirty() {
    for (int gx = origin.x; gx < origin.x + dims.x; ++gx)
        for (int gy = origin.y; gy < origin.y + dims.y; ++gy)
            for (int gz = origin.z; gz < origin.z + dims.z; ++gz)
                baseline[static_cast<std::size_t>(localIndexOf(gx, gy, gz))] =
                    world.blockAt(gx, gy, gz);
    dirtyList.clear();
}

uint64_t WorldSim::regionHash() const {
    uint64_t hash = 1469598103934665603ull;
    for (int gx = origin.x; gx < origin.x + dims.x; ++gx)
        for (int gy = origin.y; gy < origin.y + dims.y; ++gy)
            for (int gz = origin.z; gz < origin.z + dims.z; ++gz)
                hash = fnv1a(hash, static_cast<uint8_t>(world.blockAt(gx, gy, gz)));
    return hash;
}

int WorldSim::snowDepthAt(const Vec3i& pos) const { return weatherClimate->snowDepthAt(pos); }

int WorldSim::iceAgeAt(const Vec3i& pos) const { return weatherClimate->iceAgeAt(pos); }

float WorldSim::temperatureAt(int gx, int gy, int gz) const {
    return weatherClimate->temperatureAt(gx, gy, gz);
}

Precipitation WorldSim::currentPrecipitation() const {
    Vec3i center{origin.x + dims.x / 2, origin.y + dims.y / 2, origin.z + dims.z / 2};
    float temperature = weatherClimate->temperatureAt(center.x, center.y, center.z);
    return sky.precipitationAt(temperature);
}

void WorldSim::addMinecart(const Minecart& cart) { carts.push_back(cart); }

void WorldSim::setPoweredRailPredicate(std::function<bool(const Vec3i&)> predicate) {
    poweredRail = std::move(predicate);
}

void WorldSim::addCreature(const Creature& creature) { mobs.push_back(creature); }

void WorldSim::setPlayer(const Vec3& pos) {
    player = pos;
    hasPlayer = true;
}

void WorldSim::clearPlayer() { hasPlayer = false; }

}
