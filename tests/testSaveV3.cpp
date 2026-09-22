#include "testHarness.h"
#include "vf/SaveV3.h"
#include "vf/Serialize.h"
#include "vf/Fluid.h"
#include "vf/LightEngine.h"

using namespace vf;

namespace {

GridSnapshot snapshotFluid(const FluidSim& sim) {
    return {sim.regionOrigin(), sim.regionDims(), sim.levels(), sim.sourceFlags()};
}

GridSnapshot snapshotLight(const LightEngine& engine) {
    return {engine.regionOrigin(), engine.regionDims(), engine.sunField(), engine.blockField()};
}

}

VF_TEST(saveV3RoundTripsWorldMobsFluidLight) {
    SaveV3 save;
    for (int x = 0; x < 8; ++x)
        for (int z = 0; z < 8; ++z)
            save.world.setBlock(x, 0, z, Block::Stone);
    save.world.setBlock(3, 4, 5, Block::Glowstone);

    FluidSim sim(save.world, {0, 0, 0}, {8, 4, 8});
    sim.addSource(4, 1, 4);
    sim.settle(40);
    save.hasFluid = true;
    save.fluid = snapshotFluid(sim);

    LightEngine engine(save.world, {0, 0, 0}, {8, 8, 8});
    engine.rebuild();
    save.hasLight = true;
    save.light = snapshotLight(engine);

    Mob a;
    a.position = {1.5f, 1.0f, 2.5f};
    a.velocity = {0.1f, -0.2f, 0.3f};
    a.state = MobState::Chase;
    a.rng = 0xdeadbeefu;
    a.onGround = true;
    Mob b;
    b.position = {6.5f, 3.0f, 6.5f};
    b.state = MobState::Wander;
    save.mobs = {a, b};

    std::vector<uint8_t> bytes = serializeSaveV3(save);
    SaveV3 loaded;
    CHECK(deserializeSaveV3(bytes.data(), bytes.size(), loaded));

    CHECK(loaded.world.blockAt(3, 4, 5) == Block::Glowstone);
    CHECK(loaded.world.blockAt(0, 0, 0) == Block::Stone);
    CHECK_EQ(loaded.mobs.size(), static_cast<std::size_t>(2));
    CHECK_NEAR(loaded.mobs[0].position.x, 1.5f, 1e-5f);
    CHECK_NEAR(loaded.mobs[0].velocity.z, 0.3f, 1e-5f);
    CHECK(loaded.mobs[0].state == MobState::Chase);
    CHECK(loaded.mobs[0].onGround);
    CHECK_EQ(loaded.mobs[0].rng, 0xdeadbeefu);
    CHECK(loaded.mobs[1].state == MobState::Wander);

    CHECK(loaded.hasFluid);
    CHECK(loaded.fluid.channelA == save.fluid.channelA);
    CHECK(loaded.fluid.channelB == save.fluid.channelB);
    CHECK(loaded.hasLight);
    CHECK(loaded.light.channelA == save.light.channelA);

    FluidSim restoredFluid(loaded.world, loaded.fluid.origin, loaded.fluid.dims);
    restoredFluid.loadRaw(loaded.fluid.channelA, loaded.fluid.channelB);
    CHECK_EQ(static_cast<int>(restoredFluid.level(4, 1, 4)), static_cast<int>(sim.level(4, 1, 4)));

    LightEngine restoredLight(loaded.world, loaded.light.origin, loaded.light.dims);
    restoredLight.loadRaw(loaded.light.channelA, loaded.light.channelB);
    CHECK_EQ(static_cast<int>(restoredLight.blockAt(3, 4, 5)),
             static_cast<int>(engine.blockAt(3, 4, 5)));
}

VF_TEST(saveV3LoadsLegacyV1File) {
    World world;
    world.setBlock(0, 0, 0, Block::Stone);
    world.setBlock(-5, 40, 17, Block::Water);
    std::vector<uint8_t> legacy = serializeWorld(world);

    SaveV3 loaded;
    CHECK(deserializeSaveV3(legacy.data(), legacy.size(), loaded));
    CHECK(loaded.world.blockAt(0, 0, 0) == Block::Stone);
    CHECK(loaded.world.blockAt(-5, 40, 17) == Block::Water);
    CHECK(loaded.mobs.empty());
    CHECK(!loaded.hasFluid);
    CHECK(!loaded.hasLight);
}

VF_TEST(saveV3RejectsBadMagic) {
    std::vector<uint8_t> junk = {0x00, 0x11, 0x22, 0x33, 0x44};
    SaveV3 out;
    CHECK(!deserializeSaveV3(junk.data(), junk.size(), out));
}

VF_TEST(saveV3RejectsTruncatedStream) {
    SaveV3 save;
    save.world.setBlock(0, 0, 0, Block::Stone);
    Mob m;
    save.mobs = {m};
    std::vector<uint8_t> bytes = serializeSaveV3(save);
    bytes.resize(bytes.size() - 6);
    SaveV3 out;
    CHECK(!deserializeSaveV3(bytes.data(), bytes.size(), out));
}

VF_TEST(saveV3EmptyRoundTrips) {
    SaveV3 save;
    std::vector<uint8_t> bytes = serializeSaveV3(save);
    SaveV3 loaded;
    CHECK(deserializeSaveV3(bytes.data(), bytes.size(), loaded));
    CHECK_EQ(loaded.world.chunkCount(), static_cast<std::size_t>(0));
    CHECK(loaded.mobs.empty());
    CHECK(!loaded.hasFluid);
}
