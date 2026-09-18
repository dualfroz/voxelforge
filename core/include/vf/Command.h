#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Container.h"
#include "Item.h"
#include "Math.h"
#include "Mob.h"
#include "StructureGen.h"
#include "Survival.h"
#include "TimeOfDay.h"
#include "Weather.h"
#include "World.h"

namespace vf {

enum class GameRuleType : uint8_t { Bool, Int };

struct GameRuleValue {
    GameRuleType type = GameRuleType::Bool;
    bool boolValue = false;
    int intValue = 0;
};

class GameRules {
public:
    GameRules();

    bool has(const std::string& name) const;
    const GameRuleValue* find(const std::string& name) const;

    bool boolValue(const std::string& name) const;
    int intValue(const std::string& name) const;

    bool setBool(const std::string& name, bool value);
    bool setInt(const std::string& name, int value);

    const std::vector<std::string>& names() const { return order; }

private:
    void define(const std::string& name, const GameRuleValue& value);

    std::unordered_map<std::string, GameRuleValue> rules;
    std::vector<std::string> order;
};

struct CommandContext {
    World& world;
    TimeOfDay& time;
    Weather& weather;
    Container& inventory;
    SurvivalState& player;
    std::vector<Creature>& creatures;
    GameRules& rules;

    Vec3 callerPos{0.0f, 0.0f, 0.0f};
    uint32_t seed = 0u;

    int worldMinY = 0;
    int worldMaxY = 256;
    int horizontalLimit = 30000000;
    long long maxVolume = 32768;
    int locateChunkRadius = 128;

    bool weatherForced = false;
    WeatherState forcedWeather = WeatherState::Clear;
    long long forcedWeatherDuration = 0;
};

struct CommandResult {
    bool ok = false;
    std::string message;
    int affected = 0;
    bool hasPosition = false;
    Vec3i position{0, 0, 0};
};

using CommandHandler = std::function<CommandResult(CommandContext&, const std::vector<std::string>&)>;

class CommandDispatcher {
public:
    CommandDispatcher();

    void registerCommand(const std::string& name, CommandHandler handler);
    bool hasCommand(const std::string& name) const;
    std::vector<std::string> commandNames() const;

    CommandResult execute(CommandContext& context, const std::string& line) const;

private:
    std::unordered_map<std::string, CommandHandler> handlers;
};

std::vector<std::string> tokenizeCommand(const std::string& line);
bool parseIntArg(const std::string& token, int& out);
bool parseCoordArg(const std::string& token, int base, int& out);
bool parseBlockName(const std::string& token, Block& out);
bool parseItemName(const std::string& token, ItemId& out);
bool parseSpeciesName(const std::string& token, Species& out);
bool parseStructureName(const std::string& token, StructureGenKind& out);

const char* blockDisplayName(Block block);
const char* itemDisplayName(ItemId item);
const char* speciesDisplayName(Species species);
const char* structureDisplayName(StructureGenKind kind);

}
