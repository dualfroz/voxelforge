#include "testHarness.h"
#include "vf/WorldSim.h"
#include "vf/RandomTick.h"
#include "vf/ChunkSync.h"
#include "vf/Farming.h"
#include "vf/Weather.h"
#include "vf/TimeOfDay.h"
#include "vf/Mob.h"
#include "vf/Minecart.h"

#include <cmath>

using namespace vf;

namespace {

void stoneFloor(World& world, int x0, int x1, int z0, int z1, int y = 0) {
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z)
            world.setBlock(x, y, z, Block::Stone);
}

float horizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

void buildRedstoneSandScene(World& world) {
    world.setBlock(3, 0, 3, Block::Stone);
    world.setBlock(3, 8, 3, Block::Sand);
    world.setBlock(5, 2, 5, Block::Lever);
    world.setBlock(6, 2, 5, Block::RedstoneLamp);
}

WorldSimConfig daylightCropConfig() {
    WorldSimConfig config;
    config.worldSeed = 7u;
    config.randomTicksPerChunk = 64;
    // Freeze the clock at noon so crop sky light stays at the daytime maximum.
    config.time.ticksPerDay = 100000000;
    config.startTick = 50000000;
    return config;
}

} // namespace

VF_TEST(clockAdvancesOneTickPerSimTick) {
    World world;
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});
    sim.step(100);
    CHECK_EQ(sim.currentTick(), 100);
    CHECK_EQ(sim.time().absoluteTick(), static_cast<WorldTick>(100));
}

VF_TEST(weatherEntersRainAndIsDeterministic) {
    WorldSimConfig config;
    config.worldSeed = 44u;
    config.weather.clearMin = 1;
    config.weather.clearMax = 1;

    World worldA;
    World worldB;
    WorldSim a(worldA, {0, 0, 0}, {16, 16, 16}, config);
    WorldSim b(worldB, {0, 0, 0}, {16, 16, 16}, config);
    a.step(4);
    b.step(4);

    CHECK(a.weather().isRaining());
    CHECK_EQ(a.weather().currentTick(), static_cast<long long>(4));
    CHECK(a.weather().state() == b.weather().state());
    CHECK_EQ(a.weather().currentTick(), b.weather().currentTick());
    CHECK_NEAR(a.weather().rainGradient(), b.weather().rainGradient(), 1e-12);
}

VF_TEST(cropMaturesThroughDispatchedRandomTicks) {
    World world;
    world.setBlock(0, 63, 0, Block::Farmland);
    world.setBlock(2, 63, 0, Block::Water);

    WorldSim sim(world, {0, 64, 0}, {16, 16, 16}, daylightCropConfig());
    Vec3i crop{0, 64, 0};
    sim.farming().plant(world, crop, CropType::Wheat);
    CHECK_EQ(sim.farming().stateAt(crop)->stage, 0);
    CHECK_EQ(sim.time().skyLightLevel(), 15);

    int steps = 0;
    for (; steps < 40000 && !sim.farming().stateAt(crop)->mature(); ++steps) sim.step(1);

    CHECK(sim.farming().stateAt(crop)->mature());
    CHECK_EQ(sim.farming().stateAt(crop)->stage, 7);
    CHECK(world.blockAt(0, 64, 0) == Block::Wheat);
    CHECK(steps > 1);
}

VF_TEST(fallingSandIsDrivenByTheBlockTickScheduler) {
    World world;
    world.setBlock(3, 0, 3, Block::Stone);
    world.setBlock(3, 6, 3, Block::Sand);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    sim.step(6);
    CHECK(world.blockAt(3, 1, 3) == Block::Sand);
    CHECK(world.blockAt(3, 6, 3) == Block::Air);
}

VF_TEST(exposedDirtGrowsGrassThroughTheBlockTickHook) {
    World world;
    world.setBlock(5, 5, 5, Block::Dirt);
    WorldSimConfig config;
    config.blockRandomTicksPerChunk = 200;
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16}, config);

    bool grew = false;
    for (int i = 0; i < 300 && !grew; ++i) {
        sim.step(1);
        if (world.blockAt(5, 5, 5) == Block::Grass) grew = true;
    }
    CHECK(grew);
}

VF_TEST(redstoneLampFollowsTheLeverEachStep) {
    World world;
    world.setBlock(5, 2, 5, Block::Lever);
    world.setBlock(6, 2, 5, Block::RedstoneLamp);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    sim.circuit().setLever({5, 2, 5}, true);
    sim.step(2);
    CHECK(world.blockAt(6, 2, 5) == Block::RedstoneLampLit);

    sim.circuit().setLever({5, 2, 5}, false);
    sim.step(2);
    CHECK(world.blockAt(6, 2, 5) == Block::RedstoneLamp);
}

VF_TEST(hopperDrainsSourceIntoTargetOverManyTicks) {
    World world;
    Vec3i pos{4, 4, 4};
    world.setBlock(pos.x, pos.y, pos.z, Block::Hopper);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    sim.containers().ensure(pos + Vec3i{0, 1, 0}, 5).insert(ItemId::Stone, 5);
    sim.containers().ensure(pos + Vec3i{0, -1, 0}, 5);

    sim.step(120);
    CHECK_EQ(sim.containers().at(pos + Vec3i{0, 1, 0})->totalItems(), 0);
    CHECK_EQ(sim.containers().at(pos + Vec3i{0, -1, 0})->totalItems(), 5);
}

VF_TEST(dispenserFiresOnRedstoneEdgeThroughLogistics) {
    World world;
    Vec3i disp{5, 2, 5};
    world.setBlock(disp.x, disp.y, disp.z, Block::Dispenser);
    world.setBlock(4, 2, 5, Block::Lever);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});
    sim.containers().ensure(disp, dispenserSlots).insert(ItemId::Stone, 1);

    sim.step(1);
    CHECK(world.blockAt(6, 2, 5) == Block::Air);

    sim.circuit().setLever({4, 2, 5}, true);
    sim.step(2);
    CHECK(world.blockAt(6, 2, 5) == Block::Stone);
    CHECK_EQ(sim.containers().at(disp)->totalItems(), 0);
}

VF_TEST(minecartRollsAlongTrackWhenStepped) {
    World world;
    for (int x = 0; x <= 15; ++x) world.setBlock(x, 0, 0, Block::Rail);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    Minecart cart = makeMinecart({2, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.3f;
    sim.addMinecart(cart);

    sim.step(20);
    CHECK(sim.minecarts()[0].railPos.x > 2);
    CHECK(!sim.minecarts()[0].derailed);
}

VF_TEST(hostileCreatureChasesThePlayerAndClosesDistance) {
    World world;
    stoneFloor(world, -2, 40, -2, 40);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    sim.addCreature(spawnCreature(Species::Zombie, {5.0f, 1.0f, 5.0f}, 5u));
    Vec3 playerPos{11.0f, 1.0f, 5.0f};
    sim.setPlayer(playerPos);

    float startDist = horizontalDistance(sim.creatures()[0].body.position, playerPos);
    bool sawChase = false;
    for (int i = 0; i < 150; ++i) {
        sim.step(1);
        if (sim.creatures()[0].state == CreatureState::Chase) sawChase = true;
    }
    float endDist = horizontalDistance(sim.creatures()[0].body.position, playerPos);

    CHECK(sawChase);
    CHECK(endDist < startDist);
}

VF_TEST(climateLaysSnowUnderSnowfall) {
    World world;
    world.setBlock(0, 30, 0, Block::Stone);

    WorldSimConfig config;
    config.climateIntervalTicks = 1;
    config.weather.clearMin = 1;
    config.weather.clearMax = 1;
    config.weather.rainMin = 1000000;
    config.weather.rainMax = 1000000;
    config.weather.snowTemperature = 1000.0f;
    config.climate.freezeTemperature = 1000.0f;
    config.climate.meltTemperature = 2000.0f;

    WorldSim sim(world, {-1, 30, -1}, {3, 5, 3}, config);
    sim.step(10);

    CHECK(world.blockAt(0, 31, 0) == Block::SnowLayer);
    CHECK(sim.snowDepthAt({0, 31, 0}) >= 1);
    CHECK(sim.currentPrecipitation() == Precipitation::Snow);
}

VF_TEST(dirtyFeedReplicatesChangedBlocksThroughChunkSync) {
    World server;
    server.setBlock(5, 2, 5, Block::Lever);
    server.setBlock(6, 2, 5, Block::RedstoneLamp);

    Vec3i coord{0, 0, 0};
    World client;
    client.ensureChunk(coord).raw() = server.chunkAt(coord)->raw();

    WorldSim sim(server, {0, 0, 0}, {16, 16, 16});
    sim.circuit().setLever({5, 2, 5}, true);
    sim.step(2);

    const std::vector<Vec3i>& dirty = sim.dirtyBlocks();
    CHECK(!dirty.empty());
    bool foundLamp = false;
    for (const Vec3i& cell : dirty)
        if (cell == Vec3i{6, 2, 5}) foundLamp = true;
    CHECK(foundLamp);

    ServerChunkSync sync;
    for (const Vec3i& cell : dirty) {
        Vec3i chunkCoord{floorDiv(cell.x, chunkSize), floorDiv(cell.y, chunkSize),
                         floorDiv(cell.z, chunkSize)};
        sync.markDirty(chunkCoord, floorMod(cell.x, chunkSize), floorMod(cell.y, chunkSize),
                       floorMod(cell.z, chunkSize));
    }

    MultiBlockChange delta;
    CHECK(sync.buildDelta(server, coord, delta));
    ClientChunkSync clientSync;
    clientSync.applyDelta(delta, client);

    CHECK(client.chunkAt(coord)->raw() == server.chunkAt(coord)->raw());
}

VF_TEST(clearDirtyRebasesTheDirtyFeed) {
    World world;
    world.setBlock(6, 2, 5, Block::RedstoneLamp);
    world.setBlock(5, 2, 5, Block::Lever);
    WorldSim sim(world, {0, 0, 0}, {16, 16, 16});

    sim.circuit().setLever({5, 2, 5}, true);
    sim.step(2);
    CHECK(!sim.dirtyBlocks().empty());

    sim.clearDirty();
    CHECK(sim.dirtyBlocks().empty());

    sim.step(2);
    CHECK(sim.dirtyBlocks().empty());
}

VF_TEST(sameSeedYieldsByteIdenticalWorldState) {
    WorldSimConfig config;
    config.worldSeed = 2718u;
    config.weather.clearMin = 1;
    config.weather.clearMax = 1;

    World worldA;
    World worldB;
    buildRedstoneSandScene(worldA);
    buildRedstoneSandScene(worldB);

    WorldSim a(worldA, {0, 0, 0}, {16, 16, 16}, config);
    WorldSim b(worldB, {0, 0, 0}, {16, 16, 16}, config);
    a.circuit().setLever({5, 2, 5}, true);
    b.circuit().setLever({5, 2, 5}, true);

    a.step(60);
    b.step(60);

    CHECK_EQ(a.regionHash(), b.regionHash());
    CHECK_EQ(a.currentTick(), b.currentTick());
    CHECK(a.weather().state() == b.weather().state());
    CHECK(worldA.chunkAt({0, 0, 0})->raw() == worldB.chunkAt({0, 0, 0})->raw());
}
