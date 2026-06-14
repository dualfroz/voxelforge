#include "testHarness.h"
#include "vf/Physics.h"

using namespace vf;

namespace {

World floorWorld() {
    World world;
    for (int x = 0; x < chunkSize; ++x)
        for (int z = 0; z < chunkSize; ++z)
            world.setBlock(x, 0, z, Block::Stone);
    return world;
}

Player standingPlayer() {
    Player player;
    player.position = {8.0f, 5.0f, 8.0f};
    return player;
}

} // namespace

VF_TEST(physicsPlayerFallsAndLandsOnFloor) {
    World world = floorWorld();
    Player player = standingPlayer();
    PhysicsConfig config;
    MoveInput input;

    for (int i = 0; i < 300; ++i) stepPlayer(world, player, input, config, 1.0f / 60.0f);

    CHECK(player.onGround);
    CHECK_NEAR(player.position.y, 1.0f, 0.02f);   // feet rest on top of the y=0 block
    CHECK(!boxOverlapsSolid(world, playerBox(player)));
}

VF_TEST(physicsWallBlocksHorizontalMovement) {
    World world = floorWorld();
    for (int y = 1; y <= 3; ++y)
        for (int z = 0; z < chunkSize; ++z)
            world.setBlock(10, y, z, Block::Stone);

    Player player = standingPlayer();
    PhysicsConfig config;
    MoveInput input;
    input.strafe = 1.0f;   // yaw 0 -> pushes toward +x

    for (int i = 0; i < 300; ++i) stepPlayer(world, player, input, config, 1.0f / 60.0f);

    CHECK(player.position.x + player.width * 0.5f <= 10.0f + 1e-3f);
    CHECK(!boxOverlapsSolid(world, playerBox(player)));
}

VF_TEST(physicsFlyModeIgnoresGravity) {
    World world;   // empty, nothing to stand on
    Player player;
    player.position = {0.0f, 20.0f, 0.0f};
    player.flying = true;
    PhysicsConfig config;
    MoveInput input;

    for (int i = 0; i < 120; ++i) stepPlayer(world, player, input, config, 1.0f / 60.0f);

    CHECK_NEAR(player.position.y, 20.0f, 1e-3f);   // hovers in place
}

VF_TEST(physicsFlyAscendsWithInput) {
    World world;
    Player player;
    player.position = {0.0f, 20.0f, 0.0f};
    player.flying = true;
    PhysicsConfig config;
    MoveInput input;
    input.up = true;

    stepPlayer(world, player, input, config, 1.0f / 60.0f);
    CHECK(player.position.y > 20.0f);
}

VF_TEST(physicsJumpLeavesGround) {
    World world = floorWorld();
    Player player = standingPlayer();
    PhysicsConfig config;
    MoveInput settle;
    for (int i = 0; i < 300; ++i) stepPlayer(world, player, settle, config, 1.0f / 60.0f);
    CHECK(player.onGround);

    float restY = player.position.y;
    MoveInput jump;
    jump.jump = true;
    stepPlayer(world, player, jump, config, 1.0f / 60.0f);
    CHECK(!player.onGround);
    CHECK(player.position.y > restY);
}

VF_TEST(physicsCannotSpawnOverlappingResolvesOnStep) {
    World world = floorWorld();
    Player player = standingPlayer();
    PhysicsConfig config;
    MoveInput input;
    for (int i = 0; i < 300; ++i) stepPlayer(world, player, input, config, 1.0f / 60.0f);
    CHECK(!world.solidAt(static_cast<int>(player.position.x),
                         static_cast<int>(player.position.y), 8));
}
