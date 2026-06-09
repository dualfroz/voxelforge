#include "vf/Command.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "vf/Structures.h"

namespace vf {

namespace {

struct NamedBlock {
    const char* name;
    Block block;
};

struct NamedItem {
    const char* name;
    ItemId id;
};

struct NamedSpecies {
    const char* name;
    Species species;
};

struct NamedStructure {
    const char* name;
    StructureGenKind kind;
};

const NamedBlock blockTable[] = {
    {"air", Block::Air}, {"grass", Block::Grass}, {"dirt", Block::Dirt},
    {"stone", Block::Stone}, {"sand", Block::Sand}, {"wood", Block::Wood},
    {"leaves", Block::Leaves}, {"water", Block::Water}, {"snow", Block::Snow},
    {"coalOre", Block::CoalOre}, {"ironOre", Block::IronOre}, {"goldOre", Block::GoldOre},
    {"gravel", Block::Gravel}, {"glowstone", Block::Glowstone},
    {"cobblestone", Block::Cobblestone}, {"planks", Block::Planks},
    {"sandstone", Block::Sandstone}, {"ice", Block::Ice}, {"bedrock", Block::Bedrock},
    {"obsidian", Block::Obsidian}, {"lava", Block::Lava}, {"diamondOre", Block::DiamondOre},
    {"redstoneOre", Block::RedstoneOre}, {"emeraldOre", Block::EmeraldOre},
    {"lapisOre", Block::LapisOre}, {"craftingTable", Block::CraftingTable},
    {"furnace", Block::Furnace}, {"furnaceLit", Block::FurnaceLit}, {"chest", Block::Chest},
    {"torch", Block::Torch}, {"redstoneWire", Block::RedstoneWire},
    {"redstoneTorch", Block::RedstoneTorch}, {"redstoneLamp", Block::RedstoneLamp},
    {"redstoneLampLit", Block::RedstoneLampLit}, {"lever", Block::Lever},
    {"button", Block::Button}, {"redstoneBlock", Block::RedstoneBlock},
    {"repeater", Block::Repeater}, {"comparator", Block::Comparator},
    {"piston", Block::Piston}, {"pistonSticky", Block::PistonSticky},
    {"pistonHead", Block::PistonHead}, {"tnt", Block::Tnt}, {"ladder", Block::Ladder},
    {"rail", Block::Rail}, {"poweredRail", Block::PoweredRail},
    {"detectorRail", Block::DetectorRail}, {"activatorRail", Block::ActivatorRail},
    {"hopper", Block::Hopper}, {"dropper", Block::Dropper}, {"dispenser", Block::Dispenser},
    {"observer", Block::Observer}, {"targetBlock", Block::TargetBlock},
    {"slimeBlock", Block::SlimeBlock}, {"noteBlock", Block::NoteBlock},
    {"farmland", Block::Farmland}, {"wheat", Block::Wheat}, {"carrot", Block::Carrot},
    {"potato", Block::Potato}, {"pumpkin", Block::Pumpkin}, {"melon", Block::Melon},
    {"pumpkinStem", Block::PumpkinStem}, {"melonStem", Block::MelonStem},
    {"sugarCane", Block::SugarCane}, {"cactus", Block::Cactus}, {"sapling", Block::Sapling},
    {"vine", Block::Vine}, {"snowLayer", Block::SnowLayer}, {"packedIce", Block::PackedIce},
};

const NamedItem itemTable[] = {
    {"grass", ItemId::Grass}, {"dirt", ItemId::Dirt}, {"stone", ItemId::Stone},
    {"sand", ItemId::Sand}, {"wood", ItemId::Wood}, {"leaves", ItemId::Leaves},
    {"snow", ItemId::Snow}, {"coalOre", ItemId::CoalOre}, {"ironOre", ItemId::IronOre},
    {"goldOre", ItemId::GoldOre}, {"gravel", ItemId::Gravel}, {"glowstone", ItemId::Glowstone},
    {"cobblestone", ItemId::Cobblestone}, {"planks", ItemId::Planks},
    {"sandstone", ItemId::Sandstone}, {"ice", ItemId::Ice}, {"obsidian", ItemId::Obsidian},
    {"diamondOre", ItemId::DiamondOre}, {"redstoneOre", ItemId::RedstoneOre},
    {"emeraldOre", ItemId::EmeraldOre}, {"lapisOre", ItemId::LapisOre},
    {"craftingTable", ItemId::CraftingTable}, {"furnace", ItemId::Furnace},
    {"chest", ItemId::Chest}, {"torch", ItemId::Torch}, {"redstoneWire", ItemId::RedstoneWire},
    {"redstoneTorch", ItemId::RedstoneTorch}, {"redstoneLamp", ItemId::RedstoneLamp},
    {"lever", ItemId::Lever}, {"button", ItemId::Button},
    {"redstoneBlock", ItemId::RedstoneBlock}, {"repeater", ItemId::Repeater},
    {"comparator", ItemId::Comparator}, {"piston", ItemId::Piston},
    {"pistonSticky", ItemId::PistonSticky}, {"tnt", ItemId::Tnt}, {"ladder", ItemId::Ladder},
    {"stick", ItemId::Stick}, {"gunpowder", ItemId::Gunpowder}, {"coalLump", ItemId::CoalLump},
    {"ironIngot", ItemId::IronIngot}, {"goldIngot", ItemId::GoldIngot},
    {"diamondGem", ItemId::DiamondGem}, {"emeraldGem", ItemId::EmeraldGem},
    {"redstoneDust", ItemId::RedstoneDust}, {"lapisLazuli", ItemId::LapisLazuli},
    {"woodPickaxe", ItemId::WoodPickaxe}, {"woodAxe", ItemId::WoodAxe},
    {"woodShovel", ItemId::WoodShovel}, {"woodSword", ItemId::WoodSword},
    {"woodHoe", ItemId::WoodHoe}, {"stonePickaxe", ItemId::StonePickaxe},
    {"stoneAxe", ItemId::StoneAxe}, {"stoneShovel", ItemId::StoneShovel},
    {"stoneSword", ItemId::StoneSword}, {"stoneHoe", ItemId::StoneHoe},
    {"ironPickaxe", ItemId::IronPickaxe}, {"ironAxe", ItemId::IronAxe},
    {"ironShovel", ItemId::IronShovel}, {"ironSword", ItemId::IronSword},
    {"ironHoe", ItemId::IronHoe}, {"goldPickaxe", ItemId::GoldPickaxe},
    {"goldAxe", ItemId::GoldAxe}, {"goldShovel", ItemId::GoldShovel},
    {"goldSword", ItemId::GoldSword}, {"goldHoe", ItemId::GoldHoe},
    {"diamondPickaxe", ItemId::DiamondPickaxe}, {"diamondAxe", ItemId::DiamondAxe},
    {"diamondShovel", ItemId::DiamondShovel}, {"diamondSword", ItemId::DiamondSword},
    {"diamondHoe", ItemId::DiamondHoe}, {"apple", ItemId::Apple}, {"bread", ItemId::Bread},
    {"rawPorkchop", ItemId::RawPorkchop}, {"cookedPorkchop", ItemId::CookedPorkchop},
    {"rawBeef", ItemId::RawBeef}, {"cookedBeef", ItemId::CookedBeef},
    {"rawMutton", ItemId::RawMutton}, {"cookedMutton", ItemId::CookedMutton},
    {"rottenFlesh", ItemId::RottenFlesh},
};

const NamedSpecies speciesTable[] = {
    {"sheep", Species::Sheep}, {"cow", Species::Cow}, {"pig", Species::Pig},
    {"zombie", Species::Zombie}, {"skeleton", Species::Skeleton}, {"creeper", Species::Creeper},
};

const NamedStructure structureTable[] = {
    {"village", StructureGenKind::Village}, {"dungeon", StructureGenKind::Dungeon},
    {"mineshaft", StructureGenKind::Mineshaft}, {"ravine", StructureGenKind::Ravine},
    {"desertTemple", StructureGenKind::DesertTemple},
};

std::string toLower(const std::string& in) {
    std::string out(in.size(), '\0');
    for (std::size_t i = 0; i < in.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(in[i])));
    return out;
}

bool namesMatch(const char* canonical, const std::string& loweredToken) {
    std::string lc = toLower(canonical);
    return lc == loweredToken;
}

int blockFloor(float value) { return static_cast<int>(std::floor(value)); }

long long absll(long long value) { return value < 0 ? -value : value; }

CommandResult fail(const std::string& message) {
    CommandResult out;
    out.ok = false;
    out.message = message;
    return out;
}

CommandResult okResult(const std::string& message, int affected) {
    CommandResult out;
    out.ok = true;
    out.message = message;
    out.affected = affected;
    return out;
}

std::string posText(const Vec3i& p) {
    return std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z);
}

Vec3i callerBlock(const CommandContext& ctx) {
    return {blockFloor(ctx.callerPos.x), blockFloor(ctx.callerPos.y), blockFloor(ctx.callerPos.z)};
}

bool inWorldBounds(const CommandContext& ctx, const Vec3i& p) {
    if (p.y < ctx.worldMinY || p.y >= ctx.worldMaxY) return false;
    if (absll(p.x) > ctx.horizontalLimit || absll(p.z) > ctx.horizontalLimit) return false;
    return true;
}

bool parseCorner(const std::vector<std::string>& args, std::size_t start, const Vec3i& base,
                 Vec3i& out) {
    return parseCoordArg(args[start], base.x, out.x) &&
           parseCoordArg(args[start + 1], base.y, out.y) &&
           parseCoordArg(args[start + 2], base.z, out.z);
}

void boxExtents(const Vec3i& a, const Vec3i& b, Vec3i& lo, Vec3i& hi) {
    lo = {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    hi = {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

long long boxVolume(const Vec3i& lo, const Vec3i& hi) {
    long long dx = static_cast<long long>(hi.x - lo.x) + 1;
    long long dy = static_cast<long long>(hi.y - lo.y) + 1;
    long long dz = static_cast<long long>(hi.z - lo.z) + 1;
    return dx * dy * dz;
}

enum class Selector : uint8_t { Self, Nearest, AllEntities, AllPlayers };

bool parseSelector(const std::string& token, Selector& out) {
    if (token == "@s") { out = Selector::Self; return true; }
    if (token == "@p") { out = Selector::Nearest; return true; }
    if (token == "@e") { out = Selector::AllEntities; return true; }
    if (token == "@a") { out = Selector::AllPlayers; return true; }
    return false;
}

int nearestCreature(const CommandContext& ctx) {
    int best = -1;
    float bestDistSq = 0.0f;
    for (std::size_t i = 0; i < ctx.creatures.size(); ++i) {
        const Creature& creature = ctx.creatures[i];
        if (!creature.alive) continue;
        Vec3 delta = creature.body.position - ctx.callerPos;
        float distSq = dot(delta, delta);
        if (best < 0 || distSq < bestDistSq) {
            best = static_cast<int>(i);
            bestDistSq = distSq;
        }
    }
    return best;
}

CommandResult cmdSetblock(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 4u) return fail("usage: setblock <x> <y> <z> <block>");
    Vec3i target;
    if (!parseCorner(args, 0, callerBlock(ctx), target))
        return fail("invalid coordinates");
    Block block;
    if (!parseBlockName(args[3], block)) return fail("unknown block: " + args[3]);
    if (!inWorldBounds(ctx, target)) return fail("position out of bounds");
    ctx.world.setBlock(target.x, target.y, target.z, block);
    CommandResult out = okResult(std::string("placed ") + blockDisplayName(block) + " at " +
                                     posText(target),
                                 1);
    out.hasPosition = true;
    out.position = target;
    return out;
}

CommandResult cmdFill(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 7u && args.size() != 8u)
        return fail("usage: fill <x1> <y1> <z1> <x2> <y2> <z2> <block> [replace|keep|destroy]");
    Vec3i base = callerBlock(ctx);
    Vec3i cornerA, cornerB;
    if (!parseCorner(args, 0, base, cornerA) || !parseCorner(args, 3, base, cornerB))
        return fail("invalid coordinates");
    Block block;
    if (!parseBlockName(args[6], block)) return fail("unknown block: " + args[6]);

    std::string mode = args.size() == 8u ? toLower(args[7]) : "replace";
    if (mode != "replace" && mode != "keep" && mode != "destroy")
        return fail("unknown fill mode: " + args[7]);

    Vec3i lo, hi;
    boxExtents(cornerA, cornerB, lo, hi);
    if (!inWorldBounds(ctx, lo) || !inWorldBounds(ctx, hi))
        return fail("region out of bounds");
    if (boxVolume(lo, hi) > ctx.maxVolume)
        return fail("region exceeds max volume " + std::to_string(ctx.maxVolume));

    int changed = 0;
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                if (mode == "keep" && ctx.world.blockAt(x, y, z) != Block::Air) continue;
                ctx.world.setBlock(x, y, z, block);
                ++changed;
            }
    return okResult("filled " + std::to_string(changed) + " blocks with " + blockDisplayName(block),
                    changed);
}

CommandResult cmdReplace(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 8u)
        return fail("usage: replace <x1> <y1> <z1> <x2> <y2> <z2> <from> <to>");
    Vec3i base = callerBlock(ctx);
    Vec3i cornerA, cornerB;
    if (!parseCorner(args, 0, base, cornerA) || !parseCorner(args, 3, base, cornerB))
        return fail("invalid coordinates");
    Block fromBlock, toBlock;
    if (!parseBlockName(args[6], fromBlock)) return fail("unknown block: " + args[6]);
    if (!parseBlockName(args[7], toBlock)) return fail("unknown block: " + args[7]);

    Vec3i lo, hi;
    boxExtents(cornerA, cornerB, lo, hi);
    if (!inWorldBounds(ctx, lo) || !inWorldBounds(ctx, hi))
        return fail("region out of bounds");
    if (boxVolume(lo, hi) > ctx.maxVolume)
        return fail("region exceeds max volume " + std::to_string(ctx.maxVolume));

    int changed = 0;
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x) {
                if (ctx.world.blockAt(x, y, z) != fromBlock) continue;
                ctx.world.setBlock(x, y, z, toBlock);
                ++changed;
            }
    return okResult("replaced " + std::to_string(changed) + " blocks", changed);
}

CommandResult cmdClone(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 9u)
        return fail("usage: clone <x1> <y1> <z1> <x2> <y2> <z2> <dx> <dy> <dz>");
    Vec3i base = callerBlock(ctx);
    Vec3i cornerA, cornerB, dest;
    if (!parseCorner(args, 0, base, cornerA) || !parseCorner(args, 3, base, cornerB) ||
        !parseCorner(args, 6, base, dest))
        return fail("invalid coordinates");

    Vec3i lo, hi;
    boxExtents(cornerA, cornerB, lo, hi);
    if (!inWorldBounds(ctx, lo) || !inWorldBounds(ctx, hi))
        return fail("source region out of bounds");
    if (boxVolume(lo, hi) > ctx.maxVolume)
        return fail("region exceeds max volume " + std::to_string(ctx.maxVolume));

    Vec3i extent{hi.x - lo.x, hi.y - lo.y, hi.z - lo.z};
    Vec3i destHi{dest.x + extent.x, dest.y + extent.y, dest.z + extent.z};
    if (!inWorldBounds(ctx, dest) || !inWorldBounds(ctx, destHi))
        return fail("destination region out of bounds");

    std::vector<Block> buffer;
    buffer.reserve(static_cast<std::size_t>(boxVolume(lo, hi)));
    for (int y = lo.y; y <= hi.y; ++y)
        for (int z = lo.z; z <= hi.z; ++z)
            for (int x = lo.x; x <= hi.x; ++x)
                buffer.push_back(ctx.world.blockAt(x, y, z));

    std::size_t index = 0;
    int copied = 0;
    for (int y = 0; y <= extent.y; ++y)
        for (int z = 0; z <= extent.z; ++z)
            for (int x = 0; x <= extent.x; ++x) {
                ctx.world.setBlock(dest.x + x, dest.y + y, dest.z + z, buffer[index]);
                ++index;
                ++copied;
            }
    return okResult("cloned " + std::to_string(copied) + " blocks", copied);
}

CommandResult cmdGive(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 1u && args.size() != 2u) return fail("usage: give <item> [count]");
    ItemId item;
    if (!parseItemName(args[0], item)) return fail("unknown item: " + args[0]);
    int count = 1;
    if (args.size() == 2u) {
        if (!parseIntArg(args[1], count) || count <= 0) return fail("invalid count: " + args[1]);
    }
    int before = ctx.inventory.totalItems();
    ctx.inventory.insert(item, count);
    int given = ctx.inventory.totalItems() - before;
    if (given <= 0) return fail("no room in inventory");
    return okResult("gave " + std::to_string(given) + " " + itemDisplayName(item), given);
}

long long timeKeyword(const std::string& word, bool& ok) {
    ok = true;
    if (word == "day") return 1000;
    if (word == "noon") return 6000;
    if (word == "night") return 13000;
    if (word == "midnight") return 18000;
    ok = false;
    return 0;
}

CommandResult cmdTime(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 2u) return fail("usage: time <set|add> <ticks|day|noon|night|midnight>");
    std::string sub = toLower(args[0]);
    if (sub == "set") {
        bool keywordOk = false;
        long long value = timeKeyword(toLower(args[1]), keywordOk);
        if (!keywordOk) {
            int parsed = 0;
            if (!parseIntArg(args[1], parsed) || parsed < 0) return fail("invalid time: " + args[1]);
            value = parsed;
        }
        ctx.time.setTick(value);
        return okResult("time set to " + std::to_string(ctx.time.timeOfDayTicks()), 0);
    }
    if (sub == "add") {
        int amount = 0;
        if (!parseIntArg(args[1], amount) || amount <= 0) return fail("invalid amount: " + args[1]);
        ctx.time.advance(amount);
        return okResult("time is now " + std::to_string(ctx.time.timeOfDayTicks()), 0);
    }
    return fail("unknown time subcommand: " + args[0]);
}

CommandResult cmdWeather(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 1u && args.size() != 2u)
        return fail("usage: weather <clear|rain|thunder> [duration]");
    std::string kind = toLower(args[0]);
    WeatherState state;
    if (kind == "clear") state = WeatherState::Clear;
    else if (kind == "rain") state = WeatherState::Rain;
    else if (kind == "thunder") state = WeatherState::Thunder;
    else return fail("unknown weather: " + args[0]);

    long long duration = 6000;
    if (args.size() == 2u) {
        int parsed = 0;
        if (!parseIntArg(args[1], parsed) || parsed <= 0) return fail("invalid duration: " + args[1]);
        duration = parsed;
    }

    ctx.weatherForced = true;
    ctx.forcedWeather = state;
    ctx.forcedWeatherDuration = duration;
    if (state == WeatherState::Clear)
        ctx.weather = Weather(ctx.seed, ctx.weather.settings());

    return okResult(std::string("weather set to ") + weatherStateName(state), 0);
}

CommandResult cmdSummon(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 4u) return fail("usage: summon <mob> <x> <y> <z>");
    Species species;
    if (!parseSpeciesName(args[0], species)) return fail("unknown mob: " + args[0]);
    Vec3i cell;
    if (!parseCorner(args, 1, callerBlock(ctx), cell)) return fail("invalid coordinates");

    Vec3 position{static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y),
                  static_cast<float>(cell.z) + 0.5f};
    uint32_t creatureSeed = structureHash(ctx.seed, cell.x, cell.y, cell.z);
    ctx.creatures.push_back(spawnCreature(species, position, creatureSeed));

    CommandResult out = okResult(std::string("summoned ") + speciesDisplayName(species) + " at " +
                                     posText(cell),
                                 1);
    out.hasPosition = true;
    out.position = cell;
    return out;
}

CommandResult cmdTp(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 3u) return fail("usage: tp <x> <y> <z>");
    Vec3i target;
    if (!parseCorner(args, 0, callerBlock(ctx), target)) return fail("invalid coordinates");
    ctx.callerPos = {static_cast<float>(target.x), static_cast<float>(target.y),
                     static_cast<float>(target.z)};
    CommandResult out = okResult("teleported to " + posText(target), 1);
    out.hasPosition = true;
    out.position = target;
    return out;
}

CommandResult cmdKill(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 1u) return fail("usage: kill <@s|@p|@e|@a>");
    Selector selector;
    if (!parseSelector(args[0], selector)) return fail("invalid selector: " + args[0]);

    if (selector == Selector::Self || selector == Selector::AllPlayers) {
        applyDamage(ctx.player, DamageSource::Generic, ctx.player.health + 1.0f);
        ctx.player.alive = false;
        return okResult("killed player", 1);
    }
    if (selector == Selector::Nearest) {
        int index = nearestCreature(ctx);
        if (index < 0) return okResult("no entity to kill", 0);
        Creature& creature = ctx.creatures[static_cast<std::size_t>(index)];
        damageCreature(creature, creature.health + 1.0f);
        creature.alive = false;
        return okResult("killed nearest entity", 1);
    }
    int killed = 0;
    for (Creature& creature : ctx.creatures) {
        if (!creature.alive) continue;
        damageCreature(creature, creature.health + 1.0f);
        creature.alive = false;
        ++killed;
    }
    return okResult("killed " + std::to_string(killed) + " entities", killed);
}

CommandResult cmdHeal(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() > 1u) return fail("usage: heal [@s|@p|@e|@a]");
    Selector selector = Selector::Self;
    if (args.size() == 1u && !parseSelector(args[0], selector))
        return fail("invalid selector: " + args[0]);

    if (selector == Selector::Self || selector == Selector::AllPlayers) {
        SurvivalConfig config;
        ctx.player.health = config.maxHealth;
        ctx.player.alive = true;
        return okResult("healed player", 1);
    }
    if (selector == Selector::Nearest) {
        int index = nearestCreature(ctx);
        if (index < 0) return okResult("no entity to heal", 0);
        Creature& creature = ctx.creatures[static_cast<std::size_t>(index)];
        creature.health = speciesStats(creature.species).maxHealth;
        return okResult("healed nearest entity", 1);
    }
    int healed = 0;
    for (Creature& creature : ctx.creatures) {
        if (!creature.alive) continue;
        creature.health = speciesStats(creature.species).maxHealth;
        ++healed;
    }
    return okResult("healed " + std::to_string(healed) + " entities", healed);
}

CommandResult cmdLocate(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 1u) return fail("usage: locate <structure>");
    StructureGenKind wanted;
    if (!parseStructureName(args[0], wanted)) return fail("unknown structure: " + args[0]);

    StructureGenParams params;
    params.seed = ctx.seed;
    StructureGen gen(params);

    Vec3i origin = callerBlock(ctx);
    int originChunkX = floorDiv(origin.x, chunkSize);
    int originChunkZ = floorDiv(origin.z, chunkSize);

    int bestChunkX = 0;
    int bestChunkZ = 0;
    long long bestDistSq = 0;
    bool found = false;

    for (int ring = 0; ring <= ctx.locateChunkRadius && !found; ++ring) {
        for (int dz = -ring; dz <= ring; ++dz)
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                int cx = originChunkX + dx;
                int cz = originChunkZ + dz;
                StructureGenKind kind;
                if (!gen.structureForChunk(cx, cz, kind) || kind != wanted) continue;
                long long ddx = dx;
                long long ddz = dz;
                long long distSq = ddx * ddx + ddz * ddz;
                if (!found || distSq < bestDistSq) {
                    found = true;
                    bestDistSq = distSq;
                    bestChunkX = cx;
                    bestChunkZ = cz;
                }
            }
    }

    if (!found) return fail("could not find " + std::string(structureDisplayName(wanted)));

    Vec3i located{bestChunkX * chunkSize + chunkSize / 2, ctx.worldMinY + 64,
                  bestChunkZ * chunkSize + chunkSize / 2};
    CommandResult out = okResult(std::string("nearest ") + structureDisplayName(wanted) + " at " +
                                     posText(located),
                                 0);
    out.hasPosition = true;
    out.position = located;
    return out;
}

CommandResult cmdGamerule(CommandContext& ctx, const std::vector<std::string>& args) {
    if (args.size() != 1u && args.size() != 2u) return fail("usage: gamerule <name> [value]");
    const std::string& name = args[0];
    const GameRuleValue* rule = ctx.rules.find(name);
    if (!rule) return fail("unknown gamerule: " + name);

    if (args.size() == 1u) {
        std::string value = rule->type == GameRuleType::Bool
                                ? (rule->boolValue ? "true" : "false")
                                : std::to_string(rule->intValue);
        return okResult(name + " = " + value, 0);
    }

    if (rule->type == GameRuleType::Bool) {
        std::string value = toLower(args[1]);
        if (value != "true" && value != "false") return fail("expected true or false");
        ctx.rules.setBool(name, value == "true");
        return okResult(name + " set to " + value, 0);
    }

    int parsed = 0;
    if (!parseIntArg(args[1], parsed)) return fail("expected integer");
    ctx.rules.setInt(name, parsed);
    return okResult(name + " set to " + std::to_string(parsed), 0);
}

CommandResult cmdSeed(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!args.empty()) return fail("usage: seed");
    return okResult("seed: " + std::to_string(ctx.seed), 0);
}

} // namespace

std::vector<std::string> tokenizeCommand(const std::string& line) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i >= line.size()) break;
        std::size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        tokens.push_back(line.substr(start, i - start));
    }
    return tokens;
}

bool parseIntArg(const std::string& token, int& out) {
    if (token.empty()) return false;
    std::size_t start = 0;
    bool negative = false;
    if (token[0] == '+' || token[0] == '-') {
        negative = token[0] == '-';
        start = 1;
    }
    if (start >= token.size()) return false;
    long long value = 0;
    for (std::size_t i = start; i < token.size(); ++i) {
        if (token[i] < '0' || token[i] > '9') return false;
        value = value * 10 + (token[i] - '0');
        if (value > 4000000000LL) return false;
    }
    out = static_cast<int>(negative ? -value : value);
    return true;
}

bool parseCoordArg(const std::string& token, int base, int& out) {
    if (token.empty()) return false;
    if (token[0] == '~') {
        if (token.size() == 1u) {
            out = base;
            return true;
        }
        int offset = 0;
        if (!parseIntArg(token.substr(1), offset)) return false;
        out = base + offset;
        return true;
    }
    return parseIntArg(token, out);
}

bool parseBlockName(const std::string& token, Block& out) {
    std::string lowered = toLower(token);
    for (const NamedBlock& entry : blockTable)
        if (namesMatch(entry.name, lowered)) {
            out = entry.block;
            return true;
        }
    return false;
}

bool parseItemName(const std::string& token, ItemId& out) {
    std::string lowered = toLower(token);
    for (const NamedItem& entry : itemTable)
        if (namesMatch(entry.name, lowered)) {
            out = entry.id;
            return true;
        }
    return false;
}

bool parseSpeciesName(const std::string& token, Species& out) {
    std::string lowered = toLower(token);
    for (const NamedSpecies& entry : speciesTable)
        if (namesMatch(entry.name, lowered)) {
            out = entry.species;
            return true;
        }
    return false;
}

bool parseStructureName(const std::string& token, StructureGenKind& out) {
    std::string lowered = toLower(token);
    for (const NamedStructure& entry : structureTable)
        if (namesMatch(entry.name, lowered)) {
            out = entry.kind;
            return true;
        }
    return false;
}

const char* blockDisplayName(Block block) {
    for (const NamedBlock& entry : blockTable)
        if (entry.block == block) return entry.name;
    return "unknown";
}

const char* itemDisplayName(ItemId item) {
    for (const NamedItem& entry : itemTable)
        if (entry.id == item) return entry.name;
    return "unknown";
}

const char* speciesDisplayName(Species species) {
    for (const NamedSpecies& entry : speciesTable)
        if (entry.species == species) return entry.name;
    return "unknown";
}

const char* structureDisplayName(StructureGenKind kind) {
    for (const NamedStructure& entry : structureTable)
        if (entry.kind == kind) return entry.name;
    return "unknown";
}

GameRules::GameRules() {
    define("doDaylightCycle", GameRuleValue{GameRuleType::Bool, true, 0});
    define("doWeatherCycle", GameRuleValue{GameRuleType::Bool, true, 0});
    define("doMobSpawning", GameRuleValue{GameRuleType::Bool, true, 0});
    define("mobGriefing", GameRuleValue{GameRuleType::Bool, true, 0});
    define("keepInventory", GameRuleValue{GameRuleType::Bool, false, 0});
    define("naturalRegeneration", GameRuleValue{GameRuleType::Bool, true, 0});
    define("doFireTick", GameRuleValue{GameRuleType::Bool, true, 0});
    define("randomTickSpeed", GameRuleValue{GameRuleType::Int, false, 3});
    define("spawnRadius", GameRuleValue{GameRuleType::Int, false, 10});
    define("maxEntityCramming", GameRuleValue{GameRuleType::Int, false, 24});
}

void GameRules::define(const std::string& name, const GameRuleValue& value) {
    rules[name] = value;
    order.push_back(name);
}

bool GameRules::has(const std::string& name) const { return rules.find(name) != rules.end(); }

const GameRuleValue* GameRules::find(const std::string& name) const {
    auto it = rules.find(name);
    return it == rules.end() ? nullptr : &it->second;
}

bool GameRules::boolValue(const std::string& name) const {
    const GameRuleValue* rule = find(name);
    return rule && rule->boolValue;
}

int GameRules::intValue(const std::string& name) const {
    const GameRuleValue* rule = find(name);
    return rule ? rule->intValue : 0;
}

bool GameRules::setBool(const std::string& name, bool value) {
    auto it = rules.find(name);
    if (it == rules.end() || it->second.type != GameRuleType::Bool) return false;
    it->second.boolValue = value;
    return true;
}

bool GameRules::setInt(const std::string& name, int value) {
    auto it = rules.find(name);
    if (it == rules.end() || it->second.type != GameRuleType::Int) return false;
    it->second.intValue = value;
    return true;
}

CommandDispatcher::CommandDispatcher() {
    registerCommand("setblock", cmdSetblock);
    registerCommand("fill", cmdFill);
    registerCommand("replace", cmdReplace);
    registerCommand("clone", cmdClone);
    registerCommand("give", cmdGive);
    registerCommand("time", cmdTime);
    registerCommand("weather", cmdWeather);
    registerCommand("summon", cmdSummon);
    registerCommand("tp", cmdTp);
    registerCommand("kill", cmdKill);
    registerCommand("heal", cmdHeal);
    registerCommand("locate", cmdLocate);
    registerCommand("gamerule", cmdGamerule);
    registerCommand("seed", cmdSeed);
}

void CommandDispatcher::registerCommand(const std::string& name, CommandHandler handler) {
    handlers[name] = std::move(handler);
}

bool CommandDispatcher::hasCommand(const std::string& name) const {
    return handlers.find(name) != handlers.end();
}

std::vector<std::string> CommandDispatcher::commandNames() const {
    std::vector<std::string> names;
    names.reserve(handlers.size());
    for (const auto& entry : handlers) names.push_back(entry.first);
    std::sort(names.begin(), names.end());
    return names;
}

CommandResult CommandDispatcher::execute(CommandContext& context, const std::string& line) const {
    std::vector<std::string> tokens = tokenizeCommand(line);
    if (tokens.empty()) return fail("empty command");

    std::string name = tokens[0];
    if (!name.empty() && name[0] == '/') name = name.substr(1);

    auto handler = handlers.find(name);
    if (handler == handlers.end()) return fail("unknown command: " + name);

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    return handler->second(context, args);
}

} // namespace vf
