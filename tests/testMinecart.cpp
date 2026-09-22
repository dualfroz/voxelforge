#include "testHarness.h"
#include "vf/Minecart.h"

using namespace vf;

namespace {

auto neverPowered = [](const Vec3i&) { return false; };
auto alwaysPowered = [](const Vec3i&) { return true; };

void railLine(World& world, int x0, int x1, int y, int z, Block kind) {
    for (int x = x0; x <= x1; ++x) world.setBlock(x, y, z, kind);
}

}

VF_TEST(cartRollsAlongFlatTrackAndFrictionBringsItToRest) {
    World world;
    railLine(world, 0, 60, 0, 0, Block::Rail);
    ContainerStore store;

    Minecart cart = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.3f;
    CartConfig config;

    tickMinecart(world, cart, config, neverPowered, store);
    CHECK(cart.speed < 0.3f);
    CHECK(cart.offset > 0.0f);

    for (int i = 0; i < 400; ++i) tickMinecart(world, cart, config, neverPowered, store);
    CHECK(!cart.derailed);
    CHECK(cart.railPos.x > 5);
    CHECK_NEAR(cart.speed, 0.0f, 1e-6f);
}

VF_TEST(poweredRailAcceleratesCartToMaxSpeed) {
    World world;
    railLine(world, 0, 300, 0, 0, Block::PoweredRail);
    ContainerStore store;

    Minecart cart = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.05f;
    CartConfig config;

    tickMinecart(world, cart, config, alwaysPowered, store);
    CHECK(cart.speed > 0.05f);

    for (int i = 0; i < 30; ++i) tickMinecart(world, cart, config, alwaysPowered, store);
    CHECK_NEAR(cart.speed, config.maxSpeed, 1e-4f);
    CHECK(!cart.derailed);
}

VF_TEST(inactivePoweredRailBrakesTheCart) {
    World world;
    railLine(world, 0, 60, 0, 0, Block::PoweredRail);
    ContainerStore store;

    Minecart cart = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.3f;
    CartConfig config;

    tickMinecart(world, cart, config, neverPowered, store);
    CHECK(cart.speed < 0.2f);
}

VF_TEST(gravityAcceleratesCartDownASlope) {
    World world;
    world.setBlock(0, 0, 0, Block::Rail);
    world.setBlock(-1, 0, 0, Block::Rail);
    world.setBlock(1, 1, 0, Block::Rail);
    ContainerStore store;

    Minecart cart = makeMinecart({0, 0, 0}, RailDir::West, CartKind::Empty);
    cart.speed = 0.0f;
    CartConfig config;

    tickMinecart(world, cart, config, neverPowered, store);
    CHECK(cart.speed > 0.04f);
}

VF_TEST(uphillDecelerationRollsTheCartBack) {
    World world;
    world.setBlock(0, 0, 0, Block::Rail);
    world.setBlock(-1, 0, 0, Block::Rail);
    world.setBlock(1, 1, 0, Block::Rail);
    ContainerStore store;

    Minecart cart = makeMinecart({0, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.03f;
    CartConfig config;

    tickMinecart(world, cart, config, neverPowered, store);
    CHECK(cart.moveDir == RailDir::West);
    CHECK(cart.speed > 0.0f);
}

VF_TEST(cartDerailsWhenTrackEnds) {
    World world;
    world.setBlock(0, 0, 0, Block::Rail);
    world.setBlock(1, 0, 0, Block::Rail);
    ContainerStore store;

    Minecart cart = makeMinecart({1, 0, 0}, RailDir::East, CartKind::Empty);
    cart.speed = 0.4f;
    CartConfig config;

    for (int i = 0; i < 12 && !cart.derailed; ++i)
        tickMinecart(world, cart, config, neverPowered, store);
    CHECK(cart.derailed);
    CHECK_NEAR(cart.speed, 0.0f, 1e-6f);
}

VF_TEST(hopperCartPullsFromContainerAboveOnCooldown) {
    World world;
    world.setBlock(5, 0, 0, Block::Rail);
    ContainerStore store;
    store.ensure({5, 1, 0}, 5).insert(ItemId::Stone, 3);

    Minecart cart = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Hopper);
    cart.speed = 0.0f;
    CartConfig config;

    tickMinecart(world, cart, config, neverPowered, store);
    CHECK_EQ(cart.inventory.totalItems(), 1);

    for (int i = 0; i < 60; ++i) tickMinecart(world, cart, config, neverPowered, store);
    CHECK_EQ(cart.inventory.totalItems(), 3);
    CHECK_EQ(store.at({5, 1, 0})->totalItems(), 0);
}

VF_TEST(chestCartCarriesAnInventory) {
    Minecart cart = makeMinecart({0, 0, 0}, RailDir::East, CartKind::Chest);
    CHECK_EQ(cart.inventory.size(), chestCartSlots);
    cart.inventory.insert(ItemId::DiamondGem, 12);
    CHECK_EQ(cart.inventory.totalItems(), 12);
}

VF_TEST(cartWorldPositionTracksSubCellOffset) {
    Minecart cart = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    cart.offset = 0.5f;
    Vec3 pos = cartWorldPos(cart);
    CHECK_NEAR(pos.x, 6.0f, 1e-5f);
    CHECK_NEAR(pos.z, 0.5f, 1e-5f);
}

VF_TEST(headOnCartsBounceApart) {
    World world;
    railLine(world, -40, 40, 0, 0, Block::Rail);
    ContainerStore store;
    CartConfig config;

    std::vector<Minecart> carts = {
        makeMinecart({3, 0, 0}, RailDir::East, CartKind::Empty),
        makeMinecart({7, 0, 0}, RailDir::West, CartKind::Empty),
    };
    carts[0].speed = 0.3f;
    carts[1].speed = 0.3f;

    for (int i = 0; i < 40; ++i)
        stepMinecarts(world, carts, config, neverPowered, store);

    CHECK(carts[0].moveDir == RailDir::West);
    CHECK(carts[1].moveDir == RailDir::East);
    CHECK(carts[0].railPos.x < carts[1].railPos.x);
    CHECK(!carts[0].derailed);
    CHECK(!carts[1].derailed);
}

VF_TEST(movingCartHandsMomentumToStationaryCart) {
    World world;
    railLine(world, -40, 40, 0, 0, Block::Rail);
    ContainerStore store;
    CartConfig config;

    std::vector<Minecart> carts = {
        makeMinecart({0, 0, 0}, RailDir::East, CartKind::Empty),
        makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty),
    };
    carts[0].speed = 0.3f;
    carts[1].speed = 0.0f;

    for (int i = 0; i < 40; ++i)
        stepMinecarts(world, carts, config, neverPowered, store);

    CHECK(carts[1].speed > carts[0].speed);
    CHECK(carts[1].railPos.x > carts[0].railPos.x);
    CHECK(carts[0].speed < 0.05f);
}

VF_TEST(minecartSimulationIsDeterministic) {
    World world;
    railLine(world, 0, 300, 0, 0, Block::PoweredRail);
    ContainerStore storeA, storeB;

    Minecart a = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    Minecart b = makeMinecart({5, 0, 0}, RailDir::East, CartKind::Empty);
    a.speed = 0.1f;
    b.speed = 0.1f;
    CartConfig config;

    for (int i = 0; i < 50; ++i) {
        tickMinecart(world, a, config, alwaysPowered, storeA);
        tickMinecart(world, b, config, alwaysPowered, storeB);
        CHECK(a.railPos == b.railPos);
        CHECK(a.moveDir == b.moveDir);
        CHECK_NEAR(a.speed, b.speed, 1e-6f);
        CHECK_NEAR(a.offset, b.offset, 1e-6f);
    }
}
