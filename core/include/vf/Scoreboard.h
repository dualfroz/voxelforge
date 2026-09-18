#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Block.h"
#include "Effect.h"
#include "Item.h"
#include "Mob.h"

namespace vf {

enum class StatKind : uint8_t {
    DistanceWalked,
    DistanceSprinted,
    DistanceSwum,
    DistanceFallen,
    DistanceClimbed,
    BlocksBroken,
    ItemsUsed,
    MobsKilled,
    Deaths,
    DamageDealt,
    DamageTaken,
    PlayTicks,
    Count
};

const char* statKindName(StatKind kind);
bool parseStatKindName(const std::string& text, StatKind& out);
bool statKindHasSubject(StatKind kind);
bool statKindIsDistance(StatKind kind);

struct StatCounter {
    StatKind kind = StatKind::PlayTicks;
    int subject = 0;
    long long value = 0;
};

class StatSheet {
public:
    void add(StatKind kind, long long amount);
    void addFor(StatKind kind, int subject, long long amount);
    void addBlockBroken(Block block, long long amount = 1);
    void addItemUsed(ItemId item, long long amount = 1);
    void addMobKilled(Species species, long long amount = 1);
    void clear();

    long long total(StatKind kind) const;
    long long value(StatKind kind, int subject) const;
    long long blocksBroken(Block block) const;
    long long itemsUsed(ItemId item) const;
    long long mobsKilled(Species species) const;
    long long distanceTravelled() const;

    std::vector<StatCounter> counters() const;
    int distinctSubjects(StatKind kind) const;
    long long revision() const { return changes; }

private:
    std::map<std::pair<StatKind, int>, long long> cells;
    long long changes = 0;
};

enum class DisplaySlot : uint8_t { PlayerList, Sidebar, BelowName, Count };

const char* displaySlotName(DisplaySlot slot);

struct ScoreObjective {
    enum class Criterion : uint8_t { Manual, Statistic };
    enum class Sort : uint8_t { Descending, Ascending };
    enum class Format : uint8_t { Integer, Distance, Duration };

    std::string name;
    std::string displayName;
    Criterion criterion = Criterion::Manual;
    StatKind statKind = StatKind::PlayTicks;
    int statSubject = -1;
    Sort sort = Sort::Descending;
    Format format = Format::Integer;

    bool automatic() const { return criterion == Criterion::Statistic; }
    std::string formatScore(int score) const;
};

struct ScoreEntry {
    std::string player;
    int score = 0;
    std::string formatted;
};

struct ScoreTeam {
    enum class Color : uint8_t {
        White, Gray, Black, Red, Green, Blue, Yellow, Aqua, Purple, Gold, Count
    };
    enum class Visibility : uint8_t { Always, Never, HideForOtherTeams, HideForOwnTeam };

    std::string name;
    std::string displayName;
    std::string prefix;
    std::string suffix;
    Color color = Color::White;
    bool friendlyFire = true;
    Visibility nameVisibility = Visibility::Always;
    std::vector<std::string> members;

    bool hasMember(const std::string& player) const;
};

const char* scoreTeamColorName(ScoreTeam::Color color);

class Scoreboard {
public:
    bool addObjective(const ScoreObjective& objective);
    bool removeObjective(const std::string& name);
    const ScoreObjective* objective(const std::string& name) const;
    std::vector<std::string> objectiveNames() const;

    StatSheet& stats(const std::string& player);
    const StatSheet* statsFor(const std::string& player) const;
    std::vector<std::string> players() const;

    bool setScore(const std::string& objectiveName, const std::string& player, int value);
    bool addScore(const std::string& objectiveName, const std::string& player, int delta);
    bool resetScore(const std::string& objectiveName, const std::string& player);
    void resetPlayer(const std::string& player);

    int score(const std::string& objectiveName, const std::string& player) const;
    bool tracked(const std::string& objectiveName, const std::string& player) const;

    std::vector<ScoreEntry> entries(const std::string& objectiveName) const;

    void setDisplay(DisplaySlot slot, const std::string& objectiveName);
    void clearDisplay(DisplaySlot slot);
    const std::string& displayedObjective(DisplaySlot slot) const;
    std::vector<ScoreEntry> displayLines(DisplaySlot slot, int limit = 15) const;

    bool addTeam(const ScoreTeam& team);
    bool removeTeam(const std::string& name);
    const ScoreTeam* team(const std::string& name) const;
    const ScoreTeam* teamOf(const std::string& player) const;
    bool joinTeam(const std::string& teamName, const std::string& player);
    bool leaveTeam(const std::string& player);
    std::vector<std::string> teamNames() const;

    bool friendlyFireAllowed(const std::string& attacker, const std::string& victim) const;
    bool nameVisibleTo(const std::string& viewer, const std::string& target) const;
    std::string displayNameOf(const std::string& player) const;

private:
    int statScore(const ScoreObjective& objective, const std::string& player) const;

    std::map<std::string, ScoreObjective> objectives;
    std::map<std::string, std::map<std::string, int>> manualScores;
    std::map<std::string, StatSheet> sheets;
    std::map<std::string, ScoreTeam> teams;
    std::map<std::string, std::string> teamByPlayer;
    std::array<std::string, static_cast<std::size_t>(DisplaySlot::Count)> slots;
};

}
