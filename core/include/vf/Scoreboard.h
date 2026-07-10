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

// What a statistic counts. The three families differ in their unit: distances
// are whole centimetres, PlayTicks is world ticks, everything else is a plain
// tally.
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
// True for the kinds counted once per block, item or species.
bool statKindHasSubject(StatKind kind);
bool statKindIsDistance(StatKind kind);

// One counted cell. `subject` is the block, item or species the tally belongs
// to, cast to int; it stays 0 for the kinds that count a single total.
struct StatCounter {
    StatKind kind = StatKind::PlayTicks;
    int subject = 0;
    long long value = 0;
};

// Everything one player has done, as integers so a replay counts identically.
class StatSheet {
public:
    void add(StatKind kind, long long amount);
    void addFor(StatKind kind, int subject, long long amount);
    void addBlockBroken(Block block, long long amount = 1);
    void addItemUsed(ItemId item, long long amount = 1);
    void addMobKilled(Species species, long long amount = 1);
    void clear();

    // Summed over every subject of that kind.
    long long total(StatKind kind) const;
    long long value(StatKind kind, int subject) const;
    long long blocksBroken(Block block) const;
    long long itemsUsed(ItemId item) const;
    long long mobsKilled(Species species) const;
    long long distanceTravelled() const;

    // Sorted by kind then subject, so two equal sheets serialize identically.
    std::vector<StatCounter> counters() const;
    int distinctSubjects(StatKind kind) const;
    // Incremented on every change, so a cache can tell it went stale.
    long long revision() const { return changes; }

private:
    std::map<std::pair<StatKind, int>, long long> cells;
    long long changes = 0;
};

// Where a scoreboard objective is shown.
enum class DisplaySlot : uint8_t { PlayerList, Sidebar, BelowName, Count };

const char* displaySlotName(DisplaySlot slot);

// A tracked score column. A Manual objective only moves when something calls
// setScore or addScore; a Statistic objective is a live view of the players'
// stat sheets and needs no update step at all.
struct ScoreObjective {
    enum class Criterion : uint8_t { Manual, Statistic };
    enum class Sort : uint8_t { Descending, Ascending };
    enum class Format : uint8_t { Integer, Distance, Duration };

    std::string name;
    std::string displayName;
    Criterion criterion = Criterion::Manual;
    StatKind statKind = StatKind::PlayTicks;
    // -1 totals every subject of the kind, otherwise it is the one block, item
    // or species this objective follows.
    int statSubject = -1;
    Sort sort = Sort::Descending;
    Format format = Format::Integer;

    bool automatic() const { return criterion == Criterion::Statistic; }
    // The score rendered for display: a plain number, "12.34 m" or "1h 02m 03s".
    std::string formatScore(int score) const;
};

struct ScoreEntry {
    std::string player;
    int score = 0;
    std::string formatted;
};

// A named group of players with its own colour, friendly fire rule and name
// visibility.
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

// Objectives, scores, display slots, teams and the per player stat sheets the
// automatic objectives read. Every listing comes back sorted, so two runs of
// the same game produce the same board.
class Scoreboard {
public:
    bool addObjective(const ScoreObjective& objective);
    bool removeObjective(const std::string& name);
    const ScoreObjective* objective(const std::string& name) const;
    std::vector<std::string> objectiveNames() const;

    StatSheet& stats(const std::string& player);
    const StatSheet* statsFor(const std::string& player) const;
    std::vector<std::string> players() const;

    // Refused for automatic objectives: their value belongs to the stat sheet.
    bool setScore(const std::string& objectiveName, const std::string& player, int value);
    bool addScore(const std::string& objectiveName, const std::string& player, int delta);
    bool resetScore(const std::string& objectiveName, const std::string& player);
    void resetPlayer(const std::string& player);

    int score(const std::string& objectiveName, const std::string& player) const;
    bool tracked(const std::string& objectiveName, const std::string& player) const;

    // Sorted by the objective's sort order, ties broken by player name.
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

} // namespace vf
