#include "testHarness.h"
#include "vf/Command.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

struct Fixture {
    World world;
    TimeOfDay time;
    Weather weather{1337u};
    Container inventory{36};
    SurvivalState player;
    std::vector<Creature> creatures;
    GameRules rules;
    CommandDispatcher dispatcher;

    CommandContext context{world, time, weather, inventory, player, creatures, rules};

    Fixture() {
        context.seed = 1337u;
        context.callerPos = {0.0f, 64.0f, 0.0f};
    }

    CommandResult run(const std::string& line) { return dispatcher.execute(context, line); }
};

int countBlockInBox(const World& world, const Vec3i& lo, const Vec3i& hi, Block block) {
    int found = 0;
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x)
                if (world.blockAt(x, y, z) == block) ++found;
    return found;
}

}

VF_TEST(commandTokenizerSplitsOnWhitespace) {
    std::vector<std::string> tokens = tokenizeCommand("  setblock   10 64   -3  stone ");
    CHECK_EQ(tokens.size(), 5u);
    CHECK(tokens[0] == "setblock");
    CHECK(tokens[2] == "64");
    CHECK(tokens[4] == "stone");
    CHECK(tokenizeCommand("").empty());
    CHECK(tokenizeCommand("   ").empty());
}

VF_TEST(commandParseIntAcceptsSignRejectsGarbage) {
    int out = 0;
    CHECK(parseIntArg("42", out) && out == 42);
    CHECK(parseIntArg("-7", out) && out == -7);
    CHECK(parseIntArg("+5", out) && out == 5);
    CHECK(!parseIntArg("", out));
    CHECK(!parseIntArg("12a", out));
    CHECK(!parseIntArg("~", out));
    CHECK(!parseIntArg("-", out));
}

VF_TEST(commandParseCoordAbsoluteAndRelative) {
    int out = 0;
    CHECK(parseCoordArg("10", 100, out) && out == 10);
    CHECK(parseCoordArg("~", 100, out) && out == 100);
    CHECK(parseCoordArg("~5", 100, out) && out == 105);
    CHECK(parseCoordArg("~-3", 100, out) && out == 97);
    CHECK(!parseCoordArg("~x", 100, out));
    CHECK(!parseCoordArg("", 100, out));
}

VF_TEST(commandParseBlockItemSpeciesStructureNames) {
    Block block;
    CHECK(parseBlockName("stone", block) && block == Block::Stone);
    CHECK(parseBlockName("DiamondOre", block) && block == Block::DiamondOre);
    CHECK(parseBlockName("packedice", block) && block == Block::PackedIce);
    CHECK(!parseBlockName("notablock", block));

    ItemId item;
    CHECK(parseItemName("diamondSword", item) && item == ItemId::DiamondSword);
    CHECK(!parseItemName("nope", item));

    Species species;
    CHECK(parseSpeciesName("ZOMBIE", species) && species == Species::Zombie);
    CHECK(!parseSpeciesName("dragon", species));

    StructureGenKind kind;
    CHECK(parseStructureName("village", kind) && kind == StructureGenKind::Village);
    CHECK(parseStructureName("desertTemple", kind) && kind == StructureGenKind::DesertTemple);
    CHECK(!parseStructureName("castle", kind));
}

VF_TEST(commandSetblockPlacesRealBlock) {
    Fixture fix;
    CHECK(fix.world.blockAt(10, 64, -3) == Block::Air);
    CommandResult result = fix.run("setblock 10 64 -3 stone");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 1);
    CHECK(fix.world.blockAt(10, 64, -3) == Block::Stone);
    CHECK(result.hasPosition);
    CHECK(result.position == Vec3i(10, 64, -3));
}

VF_TEST(commandSetblockRelativeCoordsResolveAgainstCaller) {
    Fixture fix;
    fix.context.callerPos = {20.0f, 70.0f, -5.0f};
    CommandResult result = fix.run("setblock ~ ~1 ~-2 glowstone");
    CHECK(result.ok);
    CHECK(fix.world.blockAt(20, 71, -7) == Block::Glowstone);
}

VF_TEST(commandSetblockRejectsBadArgs) {
    Fixture fix;
    CHECK(!fix.run("setblock 1 2 stone").ok);
    CHECK(!fix.run("setblock 1 2 3 notablock").ok);
    CHECK(!fix.run("setblock a b c stone").ok);
    CHECK(!fix.run("setblock 1 9000 3 stone").ok);
}

VF_TEST(commandFillCoversExactVolume) {
    Fixture fix;
    CommandResult result = fix.run("fill 0 64 0 3 66 1 stone");
    CHECK(result.ok);
    int expected = 4 * 3 * 2;
    CHECK_EQ(result.affected, expected);
    CHECK_EQ(countBlockInBox(fix.world, {0, 64, 0}, {3, 66, 1}, Block::Stone), expected);
}

VF_TEST(commandFillKeepModeOnlyFillsAir) {
    Fixture fix;
    fix.world.setBlock(1, 64, 0, Block::Dirt);
    CommandResult result = fix.run("fill 0 64 0 2 64 0 stone keep");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 2);
    CHECK(fix.world.blockAt(0, 64, 0) == Block::Stone);
    CHECK(fix.world.blockAt(1, 64, 0) == Block::Dirt);
    CHECK(fix.world.blockAt(2, 64, 0) == Block::Stone);
}

VF_TEST(commandFillRejectsOversizeVolume) {
    Fixture fix;
    fix.context.maxVolume = 8;
    CommandResult result = fix.run("fill 0 64 0 3 66 1 stone");
    CHECK(!result.ok);
    CHECK(fix.world.blockAt(0, 64, 0) == Block::Air);
}

VF_TEST(commandReplaceOnlyMatchingBlocks) {
    Fixture fix;
    fix.run("fill 0 64 0 4 64 0 stone");
    fix.world.setBlock(2, 64, 0, Block::Dirt);
    CommandResult result = fix.run("replace 0 64 0 4 64 0 stone cobblestone");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 4);
    CHECK(fix.world.blockAt(2, 64, 0) == Block::Dirt);
    CHECK_EQ(countBlockInBox(fix.world, {0, 64, 0}, {4, 64, 0}, Block::Cobblestone), 4);
}

VF_TEST(commandCloneCopiesVolumeOneToOne) {
    Fixture fix;
    fix.world.setBlock(0, 64, 0, Block::Stone);
    fix.world.setBlock(1, 64, 0, Block::Dirt);
    fix.world.setBlock(0, 65, 0, Block::Glowstone);
    CommandResult result = fix.run("clone 0 64 0 1 65 0 10 64 0");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 4);
    CHECK(fix.world.blockAt(10, 64, 0) == Block::Stone);
    CHECK(fix.world.blockAt(11, 64, 0) == Block::Dirt);
    CHECK(fix.world.blockAt(10, 65, 0) == Block::Glowstone);
    CHECK(fix.world.blockAt(11, 65, 0) == Block::Air);
}

VF_TEST(commandCloneHandlesOverlappingRegions) {
    Fixture fix;
    fix.world.setBlock(0, 64, 0, Block::Stone);
    fix.world.setBlock(1, 64, 0, Block::Dirt);
    fix.world.setBlock(2, 64, 0, Block::Sand);
    CommandResult result = fix.run("clone 0 64 0 2 64 0 1 64 0");
    CHECK(result.ok);
    CHECK(fix.world.blockAt(1, 64, 0) == Block::Stone);
    CHECK(fix.world.blockAt(2, 64, 0) == Block::Dirt);
    CHECK(fix.world.blockAt(3, 64, 0) == Block::Sand);
}

VF_TEST(commandGiveAddsRealItem) {
    Fixture fix;
    CHECK_EQ(fix.inventory.totalItems(), 0);
    CommandResult result = fix.run("give diamondSword");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 1);
    CHECK_EQ(fix.inventory.totalItems(), 1);

    CommandResult stack = fix.run("give apple 40");
    CHECK(stack.ok);
    CHECK_EQ(stack.affected, 40);
    CHECK_EQ(fix.inventory.totalItems(), 41);
}

VF_TEST(commandGiveRejectsBadCountAndUnknownItem) {
    Fixture fix;
    CHECK(!fix.run("give apple 0").ok);
    CHECK(!fix.run("give apple -5").ok);
    CHECK(!fix.run("give banana").ok);
}

VF_TEST(commandTimeSetAndAdd) {
    Fixture fix;
    CommandResult setResult = fix.run("time set 6000");
    CHECK(setResult.ok);
    CHECK_EQ(fix.time.timeOfDayTicks(), 6000);

    fix.run("time set day");
    CHECK_EQ(fix.time.timeOfDayTicks(), 1000);

    fix.run("time add 500");
    CHECK_EQ(fix.time.timeOfDayTicks(), 1500);

    CHECK(!fix.run("time add -100").ok);
    CHECK(!fix.run("time warp 5").ok);
}

VF_TEST(commandWeatherForcesTargetState) {
    Fixture fix;
    CommandResult rain = fix.run("weather rain 400");
    CHECK(rain.ok);
    CHECK(fix.context.weatherForced);
    CHECK(fix.context.forcedWeather == WeatherState::Rain);
    CHECK_EQ(fix.context.forcedWeatherDuration, 400);

    fix.run("weather thunder");
    CHECK(fix.context.forcedWeather == WeatherState::Thunder);

    CommandResult clear = fix.run("weather clear");
    CHECK(clear.ok);
    CHECK(fix.context.forcedWeather == WeatherState::Clear);
    CHECK(fix.weather.state() == WeatherState::Clear);
    CHECK_NEAR(fix.weather.rainGradient(), 0.0, 1e-9);

    CHECK(!fix.run("weather snow").ok);
}

VF_TEST(commandSummonSpawnsMob) {
    Fixture fix;
    CHECK(fix.creatures.empty());
    CommandResult result = fix.run("summon zombie 5 64 5");
    CHECK(result.ok);
    CHECK_EQ(fix.creatures.size(), 1u);
    CHECK(fix.creatures[0].species == Species::Zombie);
    CHECK(fix.creatures[0].alive);
    CHECK_NEAR(fix.creatures[0].body.position.y, 64.0f, 1e-4f);
    CHECK(!fix.run("summon dragon 1 2 3").ok);
}

VF_TEST(commandSummonIsDeterministicForSamePosition) {
    Fixture a;
    Fixture b;
    a.run("summon sheep 3 70 8");
    b.run("summon sheep 3 70 8");
    CHECK_EQ(a.creatures[0].rng, b.creatures[0].rng);
    CHECK_NEAR(a.creatures[0].body.wanderDir.x, b.creatures[0].body.wanderDir.x, 1e-6f);
}

VF_TEST(commandTpMovesCaller) {
    Fixture fix;
    fix.run("tp 100 72 -40");
    CHECK_NEAR(fix.context.callerPos.x, 100.0f, 1e-4f);
    CHECK_NEAR(fix.context.callerPos.y, 72.0f, 1e-4f);
    CHECK_NEAR(fix.context.callerPos.z, -40.0f, 1e-4f);

    fix.run("tp ~ ~5 ~");
    CHECK_NEAR(fix.context.callerPos.y, 77.0f, 1e-4f);
}

VF_TEST(commandTpRejectsPositionOutsideWorldHeight) {
    Fixture fix;
    fix.context.worldMinY = -64;
    fix.context.worldMaxY = 320;
    CHECK(fix.run("tp 0 -64 0").ok);
    CHECK_NEAR(fix.context.callerPos.y, -64.0f, 1e-4f);

    CommandResult below = fix.run("tp 0 -65 0");
    CHECK(!below.ok);
    CHECK(below.message == "position out of bounds");
    CHECK(!fix.run("tp ~ ~-1 ~").ok);
    CHECK(!fix.run("tp 0 320 0").ok);
    CHECK_NEAR(fix.context.callerPos.y, -64.0f, 1e-4f);
}

VF_TEST(commandSummonRejectsPositionOutsideWorldHeight) {
    Fixture fix;
    CommandResult below = fix.run("summon zombie 5 -1 5");
    CHECK(!below.ok);
    CHECK(below.message == "position out of bounds");
    CHECK(!fix.run("summon cow 5 256 5").ok);
    CHECK(!fix.run("summon cow 30000001 64 0").ok);
    CHECK(fix.creatures.empty());

    CHECK(fix.run("summon cow 5 0 5").ok);
    CHECK(fix.run("summon cow 5 255 5").ok);
    CHECK_EQ(fix.creatures.size(), 2u);
}

VF_TEST(commandKillSelfEndsPlayer) {
    Fixture fix;
    CHECK(fix.player.alive);
    CommandResult result = fix.run("kill @s");
    CHECK(result.ok);
    CHECK(!fix.player.alive);
    CHECK_NEAR(fix.player.health, 0.0f, 1e-4f);
}

VF_TEST(commandKillAllEntitiesClearsCreatures) {
    Fixture fix;
    fix.run("summon cow 1 64 1");
    fix.run("summon pig 2 64 2");
    fix.run("summon sheep 3 64 3");
    CommandResult result = fix.run("kill @e");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 3);
    for (const Creature& creature : fix.creatures) CHECK(!creature.alive);
    CHECK(fix.player.alive);
}

VF_TEST(commandKillNearestPicksClosest) {
    Fixture fix;
    fix.context.callerPos = {0.0f, 64.0f, 0.0f};
    fix.run("summon cow 20 64 0");
    fix.run("summon pig 2 64 0");
    CommandResult result = fix.run("kill @p");
    CHECK(result.ok);
    CHECK_EQ(result.affected, 1);
    CHECK(fix.creatures[0].alive);
    CHECK(!fix.creatures[1].alive);
}

VF_TEST(commandHealRestoresPlayer) {
    Fixture fix;
    applyDamage(fix.player, DamageSource::Generic, 15.0f);
    CHECK(fix.player.health < 20.0f);
    CommandResult result = fix.run("heal");
    CHECK(result.ok);
    CHECK_NEAR(fix.player.health, 20.0f, 1e-4f);
}

VF_TEST(commandLocateFindsStructureForSeed) {
    Fixture fix;
    CommandResult result = fix.run("locate village");
    CHECK(result.ok);
    CHECK(result.hasPosition);

    StructureGenParams params;
    params.seed = fix.context.seed;
    StructureGen gen(params);
    int cx = floorDiv(result.position.x, chunkSize);
    int cz = floorDiv(result.position.z, chunkSize);
    StructureGenKind kind;
    CHECK(gen.structureForChunk(cx, cz, kind));
    CHECK(kind == StructureGenKind::Village);
}

VF_TEST(commandLocateNearestIsNotFartherThanAnotherHit) {
    Fixture fix;
    CommandResult result = fix.run("locate dungeon");
    CHECK(result.ok);

    StructureGenParams params;
    params.seed = fix.context.seed;
    StructureGen gen(params);
    int foundCx = floorDiv(result.position.x, chunkSize);
    int foundCz = floorDiv(result.position.z, chunkSize);
    long long foundDist = static_cast<long long>(foundCx) * foundCx +
                          static_cast<long long>(foundCz) * foundCz;

    long long closest = -1;
    for (int cz = -20; cz <= 20; ++cz)
        for (int cx = -20; cx <= 20; ++cx) {
            StructureGenKind kind;
            if (!gen.structureForChunk(cx, cz, kind) || kind != StructureGenKind::Dungeon) continue;
            long long dist = static_cast<long long>(cx) * cx + static_cast<long long>(cz) * cz;
            if (closest < 0 || dist < closest) closest = dist;
        }
    CHECK(closest >= 0);
    CHECK_EQ(foundDist, closest);
}

VF_TEST(commandGameruleQueryAndSet) {
    Fixture fix;
    CommandResult query = fix.run("gamerule keepInventory");
    CHECK(query.ok);
    CHECK(query.message.find("false") != std::string::npos);

    CommandResult setBoolResult = fix.run("gamerule keepInventory true");
    CHECK(setBoolResult.ok);
    CHECK(fix.rules.boolValue("keepInventory"));

    CommandResult setIntResult = fix.run("gamerule randomTickSpeed 7");
    CHECK(setIntResult.ok);
    CHECK_EQ(fix.rules.intValue("randomTickSpeed"), 7);

    CHECK(!fix.run("gamerule keepInventory maybe").ok);
    CHECK(!fix.run("gamerule randomTickSpeed fast").ok);
    CHECK(!fix.run("gamerule nonexistentRule true").ok);
}

VF_TEST(commandSeedReportsContextSeed) {
    Fixture fix;
    fix.context.seed = 424242u;
    CommandResult result = fix.run("seed");
    CHECK(result.ok);
    CHECK(result.message.find("424242") != std::string::npos);
}

VF_TEST(commandDispatcherRejectsUnknownAndEmpty) {
    Fixture fix;
    CHECK(!fix.run("").ok);
    CHECK(!fix.run("frobnicate 1 2 3").ok);
    CHECK(fix.dispatcher.hasCommand("setblock"));
    CHECK(!fix.dispatcher.hasCommand("frobnicate"));
    CHECK_EQ(fix.dispatcher.commandNames().size(), 14u);
}

VF_TEST(commandDispatcherStripsLeadingSlash) {
    Fixture fix;
    CommandResult result = fix.run("/setblock 0 64 0 stone");
    CHECK(result.ok);
    CHECK(fix.world.blockAt(0, 64, 0) == Block::Stone);
}
