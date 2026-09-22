#include "testHarness.h"
#include "vf/Entity.h"

using namespace vf;

namespace {

World floorWorld(int x0 = -4, int z0 = -4, int x1 = 24, int z1 = 24) {
    World world;
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z)
            world.setBlock(x, 0, z, Block::Stone);
    return world;
}

}

VF_TEST(mobFallsAndLandsOnFloor) {
    World world = floorWorld();
    Mob mob;
    mob.position = {5.0f, 6.0f, 5.0f};
    MobConfig config;

    for (int i = 0; i < 240; ++i) stepMob(world, mob, {0, 0, 0}, config, 1.0f / 60.0f);

    CHECK(mob.onGround);
    CHECK_NEAR(mob.position.y, 1.0f, 0.02f);
    CHECK(!boxOverlapsSolid(world, mobBox(mob)));
}

VF_TEST(mobStopsAtWall) {
    World world = floorWorld();
    for (int y = 1; y <= 3; ++y)
        for (int z = -4; z <= 24; ++z)
            world.setBlock(10, y, z, Block::Stone);

    Mob mob;
    mob.position = {5.0f, 1.0f, 5.0f};
    MobConfig config;

    for (int i = 0; i < 300; ++i) stepMob(world, mob, {1.0f, 0.0f, 0.0f}, config, 1.0f / 60.0f);

    CHECK(mob.position.x + mob.width * 0.5f <= 10.0f + 1e-3f);
    CHECK(!boxOverlapsSolid(world, mobBox(mob)));
}

VF_TEST(mobBrainWandersWhenTargetFar) {
    World world = floorWorld();
    Mob mob;
    mob.position = {5.0f, 1.0f, 5.0f};
    MobConfig config;
    Vec3 target{80.0f, 1.0f, 80.0f};

    Vec3 wish = updateMobBrain(world, mob, target, config);
    CHECK(mob.state == MobState::Wander);
    CHECK(length(wish) > 0.5f);
}

VF_TEST(mobBrainChasesReachableTarget) {
    World world = floorWorld();
    Mob mob;
    mob.position = {5.0f, 1.0f, 5.0f};
    MobConfig config;
    Vec3 target{9.0f, 1.0f, 5.0f};

    Vec3 wish = updateMobBrain(world, mob, target, config);
    CHECK(mob.state == MobState::Chase);
    CHECK(wish.x > 0.5f);
    CHECK(wish.x > std::fabs(wish.z));
}

VF_TEST(mobBrainHysteresisKeepsChaseInBand) {
    World world = floorWorld();
    Mob mob;
    mob.position = {5.0f, 1.0f, 5.0f};
    mob.state = MobState::Chase;
    MobConfig config;
    Vec3 inBand{5.0f + 14.0f, 1.0f, 5.0f};

    updateMobBrain(world, mob, inBand, config);
    CHECK(mob.state == MobState::Chase);

    Vec3 farOut{5.0f + 20.0f, 1.0f, 5.0f};
    updateMobBrain(world, mob, farOut, config);
    CHECK(mob.state == MobState::Wander);
}

VF_TEST(mobBrainWandersWhenTargetUnreachable) {
    World world = floorWorld();
    for (int y = 1; y <= 2; ++y) {
        world.setBlock(8, y, 5, Block::Stone);
        world.setBlock(10, y, 5, Block::Stone);
        world.setBlock(9, y, 4, Block::Stone);
        world.setBlock(9, y, 6, Block::Stone);
    }
    Mob mob;
    mob.position = {5.0f, 1.0f, 5.0f};
    MobConfig config;
    Vec3 target{9.0f, 1.0f, 5.0f};

    updateMobBrain(world, mob, target, config);
    CHECK(mob.state == MobState::Wander);
}

VF_TEST(mobChaseClosesDistanceOverTime) {
    World world = floorWorld(-4, -4, 30, 30);
    Mob mob;
    mob.position = {2.5f, 1.0f, 2.5f};
    MobConfig config;
    config.chaseRadius = 40.0f;
    config.loseRadius = 50.0f;
    Vec3 target{18.5f, 1.0f, 2.5f};

    float startDist = mob.position.x;
    for (int i = 0; i < 500; ++i) {
        Vec3 wish = updateMobBrain(world, mob, target, config);
        stepMob(world, mob, wish, config, 1.0f / 60.0f);
    }
    CHECK(mob.state == MobState::Chase);
    CHECK(mob.position.x > startDist + 10.0f);
    CHECK(std::fabs(mob.position.x - target.x) < 1.5f);
}
