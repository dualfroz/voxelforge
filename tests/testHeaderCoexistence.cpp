// Every public header pulled into ONE translation unit.
//
// This exists because it already failed for real: two modules independently
// declared vf::DamageSource (an enum in Survival.h, a struct in Damage.h) and
// nothing caught it, because no single translation unit ever included both.
// The combined binary linked and then died with SIGSEGV instead of a compiler
// error. A duplicate type, a duplicate free function or a conflicting
// declaration between any two modules now fails this file at compile time.
//
// Do not split this file and do not include headers selectively - the whole
// point is that all of them meet here.

#include "vf/Advancement.h"
#include "vf/Anvil.h"
#include "vf/Behavior.h"
#include "vf/BiomeGen.h"
#include "vf/Block.h"
#include "vf/BlockTick.h"
#include "vf/Boss.h"
#include "vf/Brewing.h"
#include "vf/Brush.h"
#include "vf/Chunk.h"
#include "vf/ChunkCache.h"
#include "vf/ChunkPipeline.h"
#include "vf/ChunkSync.h"
#include "vf/Circuit.h"
#include "vf/Climate.h"
#include "vf/Codec.h"
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

// Walk up from the working directory until the source tree is found, so the
// binary can be launched from the repository root (what `make test` does) or
// from one level below it.
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

} // namespace

// The guard above is only worth as much as its include list. A module added
// without a line here would quietly stop being checked, which is exactly how
// the first collision slipped through. This test reads the directory and this
// file's own source and fails when the two disagree.
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
        // A header on disk but missing from the include list above.
        CHECK(included.count(header) == 1u);
        if (included.count(header) != 1u) {
            std::printf("  unguarded header: core/include/vf/%s\n", header.c_str());
        }
    }
    for (const std::string& header : included) {
        // An include line left behind after a header was deleted or renamed.
        CHECK(onDisk.count(header) == 1u);
    }
    CHECK_EQ(included.size(), onDisk.size());
}

// The two names that actually collided, now proven to coexist: an enum in
// Survival.h and a struct in Damage.h, both reachable from one scope.
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

    // Distinct types: the sizes below are unrelated, and the point is only
    // that both names resolve in the same translation unit.
    CHECK(sizeof(DamageSource) >= 1u);
    CHECK(sizeof(HurtSource) > sizeof(DamageSource));
}

// Every block in the palette must map to a tile inside the atlas, on every
// face. A block added without a faceTile case silently falls through to the
// stone tile, which is the kind of thing nobody notices until the atlas is
// resized. Both facts are checked here because faceTile and tileCount live in
// the one header every module shares.
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
    // The atlas must not be padded with layers nothing references.
    CHECK_EQ(highestTile, tileCount - 1);
}

// Opacity and solidity are independent flags, but light emission is only
// meaningful within the 0..15 range the light engine packs into four bits.
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

// A transparent block must never be opaque, or the mesher would both skip the
// neighbouring face and draw the block in the transparent pass.
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

// faceVisible is the single rule the mesher trusts. It has to stay coherent
// with isAir and isOpaque for the whole palette, not just for the handful of
// blocks that existed when it was written.
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
