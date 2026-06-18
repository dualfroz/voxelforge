#include "testHarness.h"
#include "vf/PlayerData.h"

#include <cmath>
#include <vector>

using namespace vf;

namespace {

bool slotsEqual(const ItemSlot& a, const ItemSlot& b) {
    return a.item == b.item && a.count == b.count && a.durability == b.durability;
}

PlayerData samplePlayer() {
    PlayerData player;
    player.position = {123.5f, 68.25f, -940.75f};
    player.yaw = 145.0f;
    player.pitch = -22.5f;
    player.survival.health = 14.5f;
    player.survival.hunger = 7.0f;
    player.survival.saturation = 2.5f;
    player.survival.exhaustion = 3.2f;
    player.survival.airRemaining = 9.0f;
    player.survival.alive = true;
    player.xpLevel = 27;
    player.xpProgress = 0.6f;
    player.selectedSlot = 3;
    player.inventory = {
        {ItemId::Stone, 64, 0},
        {ItemId::Planks, 32, 0},
        {ItemId::None, 0, 0},
        {ItemId::DiamondPickaxe, 1, 1200},
        {ItemId::CookedBeef, 12, 0},
        {ItemId::IronSword, 1, 47},
        {ItemId::RedstoneDust, 8, 0},
    };
    return player;
}

}  // namespace

VF_TEST(playerDataRoundTripsAllFields) {
    PlayerData original = samplePlayer();
    ByteBuffer bytes = savePlayer(original);
    PlayerData restored;
    CHECK(loadPlayer(bytes.data(), bytes.size(), restored));

    CHECK(restored.position.x == original.position.x);
    CHECK(restored.position.y == original.position.y);
    CHECK(restored.position.z == original.position.z);
    CHECK(restored.yaw == original.yaw);
    CHECK(restored.pitch == original.pitch);
    CHECK(restored.survival.health == original.survival.health);
    CHECK(restored.survival.hunger == original.survival.hunger);
    CHECK(restored.survival.saturation == original.survival.saturation);
    CHECK(restored.survival.exhaustion == original.survival.exhaustion);
    CHECK(restored.survival.airRemaining == original.survival.airRemaining);
    CHECK(restored.survival.alive == original.survival.alive);
    CHECK_EQ(restored.xpLevel, original.xpLevel);
    CHECK(restored.xpProgress == original.xpProgress);
    CHECK_EQ(restored.selectedSlot, original.selectedSlot);
}

VF_TEST(playerDataInventoryRoundTrips) {
    PlayerData original = samplePlayer();
    ByteBuffer bytes = savePlayer(original);
    PlayerData restored;
    CHECK(loadPlayer(bytes.data(), bytes.size(), restored));

    CHECK_EQ(restored.inventory.size(), original.inventory.size());
    bool identical = true;
    for (std::size_t i = 0; i < original.inventory.size(); ++i)
        if (!slotsEqual(restored.inventory[i], original.inventory[i])) identical = false;
    CHECK(identical);

    // Tool durability specifically must survive the trip.
    CHECK(restored.inventory[3].item == ItemId::DiamondPickaxe);
    CHECK_EQ(restored.inventory[3].durability, 1200);
    CHECK(restored.inventory[5].item == ItemId::IronSword);
    CHECK_EQ(restored.inventory[5].durability, 47);
}

VF_TEST(playerDataEmptyInventory) {
    PlayerData player;
    player.position = {0.0f, 0.0f, 0.0f};
    ByteBuffer bytes = savePlayer(player);
    PlayerData restored;
    CHECK(loadPlayer(bytes.data(), bytes.size(), restored));
    CHECK(restored.inventory.empty());
    CHECK(restored.survival.alive == player.survival.alive);
}

VF_TEST(playerDataNbtTreeIsInspectable) {
    PlayerData player = samplePlayer();
    NbtTag root = playerToNbt(player);
    const NbtTag* health = root.find("Health");
    CHECK(health != nullptr && health->kind() == NbtType::Float);
    CHECK(health->asFloat() == player.survival.health);
    const NbtTag* inventory = root.find("Inventory");
    CHECK(inventory != nullptr && inventory->kind() == NbtType::List);
    CHECK_EQ(inventory->asList().size(), player.inventory.size());
}

VF_TEST(playerDataRejectsTruncatedBytes) {
    PlayerData player = samplePlayer();
    ByteBuffer bytes = savePlayer(player);
    PlayerData restored;
    CHECK(!loadPlayer(bytes.data(), bytes.size() / 2, restored));
    CHECK(!loadPlayer(bytes.data(), 3, restored));
}

VF_TEST(playerDataRejectsMissingField) {
    NbtTag root = playerToNbt(samplePlayer());
    NbtTag broken = NbtTag::makeCompound();
    for (const auto& entry : root.asCompound())
        if (entry.first != "Health") broken.put(entry.first, entry.second);
    PlayerData out;
    CHECK(!playerFromNbt(broken, out));
    // The same tree with Health present must still parse, proving the guard is real.
    PlayerData ok;
    CHECK(playerFromNbt(root, ok));
}
