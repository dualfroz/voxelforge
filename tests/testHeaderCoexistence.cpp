#include "vf/Advancement.h"
#include "vf/Anvil.h"
#include "vf/Behavior.h"
#include "vf/BiomeGen.h"
#include "vf/Block.h"
#include "vf/BlockTick.h"
#include "vf/Boss.h"
#include "vf/Brewing.h"
#include "vf/Brush.h"
#include "vf/Buoyancy.h"
#include "vf/Chunk.h"
#include "vf/ChunkCache.h"
#include "vf/ChunkPipeline.h"
#include "vf/ChunkSync.h"
#include "vf/Circuit.h"
#include "vf/Climate.h"
#include "vf/Codec.h"
#include "vf/Collide.h"
#include "vf/Command.h"
#include "vf/Container.h"
#include "vf/ContainerMenu.h"
#include "vf/Crafting.h"
#include "vf/Damage.h"
#include "vf/DataPack.h"
#include "vf/Dimension.h"
#include "vf/Dispenser.h"
#include "vf/EditHistory.h"
#include "vf/Effect.h"
#include "vf/Enchant.h"
#include "vf/EndGen.h"
#include "vf/Entity.h"
#include "vf/Equipment.h"
#include "vf/Explosion.h"
#include "vf/Farming.h"
#include "vf/Fluid.h"
#include "vf/Frustum.h"
#include "vf/Gossip.h"
#include "vf/Hopper.h"
#include "vf/Inventory.h"
#include "vf/Item.h"
#include "vf/ItemStack.h"
#include "vf/JobSystem.h"
#include "vf/Json.h"
#include "vf/LightEngine.h"
#include "vf/Lighting.h"
#include "vf/LitMesh.h"
#include "vf/Loot.h"
#include "vf/LootTables.h"
#include "vf/Math.h"
#include "vf/Mesh.h"
#include "vf/Minecart.h"
#include "vf/Mob.h"
#include "vf/Nbt.h"
#include "vf/NetServer.h"
#include "vf/NetSession.h"
#include "vf/NetherGen.h"
#include "vf/Noise.h"
#include "vf/NoiseAdv.h"
#include "vf/Pathfind.h"
#include "vf/Physics.h"
#include "vf/Piston.h"
#include "vf/PlayerData.h"
#include "vf/Portal.h"
#include "vf/PotionCatalog.h"
#include "vf/Protocol.h"
#include "vf/Raid.h"
#include "vf/Rail.h"
#include "vf/RandomTick.h"
#include "vf/Raycast.h"
#include "vf/Region.h"
#include "vf/RegionCompact.h"
#include "vf/SaveIntegrity.h"
#include "vf/SaveMigration.h"
#include "vf/SaveV3.h"
#include "vf/Schematic.h"
#include "vf/Scoreboard.h"
#include "vf/Selection.h"
#include "vf/Serialize.h"
#include "vf/SlotAction.h"
#include "vf/Spawning.h"
#include "vf/Streaming.h"
#include "vf/StructureGen.h"
#include "vf/Structures.h"
#include "vf/Survival.h"
#include "vf/Terrain.h"
#include "vf/TilePalette.h"
#include "vf/TileTexture.h"
#include "vf/TimeOfDay.h"
#include "vf/Trade.h"
#include "vf/Transport.h"
#include "vf/Villager.h"
#include "vf/Weather.h"
#include "vf/World.h"
#include "vf/WorldBackup.h"
#include "vf/WorldGen.h"
#include "vf/WorldSim.h"

#include "testHarness.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

using namespace vf;

namespace {

std::filesystem::path findRepoRoot() {
    std::filesystem::path here = std::filesystem::current_path();
    for (int step = 0; step < 4; ++step) {
        if (std::filesystem::is_directory(here / "core" / "include" / "vf") &&
            std::filesystem::is_regular_file(here / "tests" / "testHeaderCoexistence.cpp")) {
            return here;
        }
        if (!here.has_parent_path() || here.parent_path() == here) break;
        here = here.parent_path();
    }
    return {};
}

}

VF_TEST(everyPublicHeaderIsListedInTheCoexistenceGuard) {
    std::filesystem::path root = findRepoRoot();
    CHECK(!root.empty());
    if (root.empty()) return;

    std::set<std::string> onDisk;
    for (const auto& entry : std::filesystem::directory_iterator(root / "core" / "include" / "vf")) {
        if (entry.is_regular_file() && entry.path().extension() == ".h") {
            onDisk.insert(entry.path().filename().string());
        }
    }
    CHECK(onDisk.size() > 60u);

    std::set<std::string> included;
    std::ifstream source(root / "tests" / "testHeaderCoexistence.cpp");
    CHECK(source.good());
    const std::string prefix = "#include \"vf/";
    std::string line;
    while (std::getline(source, line)) {
        if (line.rfind(prefix, 0) != 0) continue;
        std::size_t end = line.find('"', prefix.size());
        if (end == std::string::npos) continue;
        included.insert(line.substr(prefix.size(), end - prefix.size()));
    }

    for (const std::string& header : onDisk) {
        CHECK(included.count(header) == 1u);
        if (included.count(header) != 1u) {
            std::printf("  unguarded header: core/include/vf/%s\n", header.c_str());
        }
    }
    for (const std::string& header : included) {
        CHECK(onDisk.count(header) == 1u);
    }
    CHECK_EQ(included.size(), onDisk.size());
}

VF_TEST(survivalAndDamageHeadersDeclareDistinctSourceTypes) {
    DamageSource legacy = DamageSource::Fall;
    CHECK(legacy == DamageSource::Fall);
    CHECK(legacy != DamageSource::Lava);

    HurtSource blow;
    blow.type = DamageType::Fall;
    blow.byPlayer = true;
    blow.attackerId = 7;
    CHECK(blow.type == DamageType::Fall);
    CHECK(blow.byPlayer);
    CHECK_EQ(blow.attackerId, 7);
    CHECK_EQ(blow.ownerId, -1);

    CHECK(sizeof(DamageSource) >= 1u);
    CHECK(sizeof(HurtSource) > sizeof(DamageSource));
}

VF_TEST(everyPaletteBlockMapsToATileInsideTheAtlas) {
    const Face faces[6] = {Face::PosX, Face::NegX, Face::PosY,
                           Face::NegY, Face::PosZ, Face::NegZ};
    int highestTile = -1;
    for (int i = 1; i < static_cast<int>(Block::Count); ++i) {
        Block block = static_cast<Block>(i);
        for (Face face : faces) {
            int tile = faceTile(block, face);
            CHECK(tile >= 0);
            CHECK(tile < tileCount);
            if (tile > highestTile) highestTile = tile;
        }
    }
    CHECK_EQ(highestTile, tileCount - 1);
}

VF_TEST(blockLightEmissionStaysInsideTheFourBitLightRange) {
    for (int i = 0; i < static_cast<int>(Block::Count); ++i) {
        Block block = static_cast<Block>(i);
        int emission = blockLightEmission(block);
        CHECK(emission >= 0);
        CHECK(emission <= 15);
        if (isAir(block)) CHECK_EQ(emission, 0);
    }
    CHECK_EQ(blockLightEmission(Block::Glowstone), 15);
    CHECK_EQ(blockLightEmission(Block::EndPortal), 15);
    CHECK_EQ(blockLightEmission(Block::NetherPortal), 11);
    CHECK_EQ(blockLightEmission(Block::Stone), 0);
}

VF_TEST(transparentBlocksAreNeverAlsoOpaque) {
    for (int i = 0; i < static_cast<int>(Block::Count); ++i) {
        Block block = static_cast<Block>(i);
        if (isTransparent(block)) CHECK(!isOpaque(block));
        if (isOpaque(block)) CHECK(occludesLight(block));
    }
    CHECK(isTransparent(Block::Glass));
    CHECK(!isOpaque(Block::Glass));
    CHECK(isOpaque(Block::Stone));
    CHECK(occludesLight(Block::Stone));
}

VF_TEST(faceVisibilityAgreesWithAirAndOpacityAcrossThePalette) {
    for (int i = 0; i < static_cast<int>(Block::Count); ++i) {
        Block self = static_cast<Block>(i);
        CHECK(!faceVisible(self, Block::Stone));
        if (isAir(self)) {
            CHECK(!faceVisible(self, Block::Air));
            continue;
        }
        CHECK(faceVisible(self, Block::Air));
        CHECK(!faceVisible(self, self));
    }
}
