#include "vf/Scoreboard.h"

#include <algorithm>

namespace vf {

namespace {

int clampToScore(long long value) {
    const long long high = 2147483647ll;
    const long long low = -2147483648ll;
    if (value > high) return static_cast<int>(high);
    if (value < low) return static_cast<int>(low);
    return static_cast<int>(value);
}

std::string twoDigits(long long value) {
    std::string text = std::to_string(value);
    return text.size() < 2u ? "0" + text : text;
}

}

const char* statKindName(StatKind kind) {
    switch (kind) {
        case StatKind::DistanceWalked: return "distanceWalked";
        case StatKind::DistanceSprinted: return "distanceSprinted";
        case StatKind::DistanceSwum: return "distanceSwum";
        case StatKind::DistanceFallen: return "distanceFallen";
        case StatKind::DistanceClimbed: return "distanceClimbed";
        case StatKind::BlocksBroken: return "blocksBroken";
        case StatKind::ItemsUsed: return "itemsUsed";
        case StatKind::MobsKilled: return "mobsKilled";
        case StatKind::Deaths: return "deaths";
        case StatKind::DamageDealt: return "damageDealt";
        case StatKind::DamageTaken: return "damageTaken";
        case StatKind::PlayTicks: return "playTicks";
        case StatKind::Count: break;
    }
    return "unknown";
}

bool parseStatKindName(const std::string& text, StatKind& out) {
    for (int i = 0; i < static_cast<int>(StatKind::Count); ++i) {
        StatKind kind = static_cast<StatKind>(i);
        if (text == statKindName(kind)) {
            out = kind;
            return true;
        }
    }
    return false;
}

bool statKindHasSubject(StatKind kind) {
    return kind == StatKind::BlocksBroken || kind == StatKind::ItemsUsed ||
           kind == StatKind::MobsKilled;
}

bool statKindIsDistance(StatKind kind) {
    return kind == StatKind::DistanceWalked || kind == StatKind::DistanceSprinted ||
           kind == StatKind::DistanceSwum || kind == StatKind::DistanceFallen ||
           kind == StatKind::DistanceClimbed;
}

void StatSheet::add(StatKind kind, long long amount) { addFor(kind, 0, amount); }

void StatSheet::addFor(StatKind kind, int subject, long long amount) {
    if (amount == 0) return;
    cells[std::make_pair(kind, subject)] += amount;
    ++changes;
}

void StatSheet::addBlockBroken(Block block, long long amount) {
    addFor(StatKind::BlocksBroken, static_cast<int>(block), amount);
}

void StatSheet::addItemUsed(ItemId item, long long amount) {
    addFor(StatKind::ItemsUsed, static_cast<int>(item), amount);
}

void StatSheet::addMobKilled(Species species, long long amount) {
    addFor(StatKind::MobsKilled, static_cast<int>(species), amount);
}

void StatSheet::clear() {
    if (cells.empty()) return;
    cells.clear();
    ++changes;
}

long long StatSheet::total(StatKind kind) const {
    long long sum = 0;
    for (const auto& cell : cells)
        if (cell.first.first == kind) sum += cell.second;
    return sum;
}

long long StatSheet::value(StatKind kind, int subject) const {
    auto found = cells.find(std::make_pair(kind, subject));
    return found == cells.end() ? 0 : found->second;
}

long long StatSheet::blocksBroken(Block block) const {
    return value(StatKind::BlocksBroken, static_cast<int>(block));
}

long long StatSheet::itemsUsed(ItemId item) const {
    return value(StatKind::ItemsUsed, static_cast<int>(item));
}

long long StatSheet::mobsKilled(Species species) const {
    return value(StatKind::MobsKilled, static_cast<int>(species));
}

long long StatSheet::distanceTravelled() const {
    long long sum = 0;
    for (int i = 0; i < static_cast<int>(StatKind::Count); ++i) {
        StatKind kind = static_cast<StatKind>(i);
        if (statKindIsDistance(kind)) sum += total(kind);
    }
    return sum;
}

std::vector<StatCounter> StatSheet::counters() const {
    std::vector<StatCounter> out;
    out.reserve(cells.size());
    for (const auto& cell : cells) {
        StatCounter counter;
        counter.kind = cell.first.first;
        counter.subject = cell.first.second;
        counter.value = cell.second;
        out.push_back(counter);
    }
    return out;
}

int StatSheet::distinctSubjects(StatKind kind) const {
    int count = 0;
    for (const auto& cell : cells)
        if (cell.first.first == kind) ++count;
    return count;
}

const char* displaySlotName(DisplaySlot slot) {
    switch (slot) {
        case DisplaySlot::PlayerList: return "playerList";
        case DisplaySlot::Sidebar: return "sidebar";
        case DisplaySlot::BelowName: return "belowName";
        case DisplaySlot::Count: break;
    }
    return "unknown";
}

std::string ScoreObjective::formatScore(int score) const {
    if (format == Format::Distance) {
        long long centimetres = score;
        std::string sign = centimetres < 0 ? "-" : "";
        long long magnitude = centimetres < 0 ? -centimetres : centimetres;
        return sign + std::to_string(magnitude / 100) + "." + twoDigits(magnitude % 100) + " m";
    }
    if (format == Format::Duration) {
        long long ticks = score < 0 ? 0 : score;
        long long seconds = ticks / ticksPerSecond;
        long long hours = seconds / 3600;
        long long minutes = (seconds % 3600) / 60;
        long long rest = seconds % 60;
        if (hours > 0)
            return std::to_string(hours) + "h " + twoDigits(minutes) + "m " + twoDigits(rest) + "s";
        if (minutes > 0) return std::to_string(minutes) + "m " + twoDigits(rest) + "s";
        return std::to_string(rest) + "s";
    }
    return std::to_string(score);
}

bool ScoreTeam::hasMember(const std::string& player) const {
    return std::find(members.begin(), members.end(), player) != members.end();
}

const char* scoreTeamColorName(ScoreTeam::Color color) {
    switch (color) {
        case ScoreTeam::Color::White: return "white";
        case ScoreTeam::Color::Gray: return "gray";
        case ScoreTeam::Color::Black: return "black";
        case ScoreTeam::Color::Red: return "red";
        case ScoreTeam::Color::Green: return "green";
        case ScoreTeam::Color::Blue: return "blue";
        case ScoreTeam::Color::Yellow: return "yellow";
        case ScoreTeam::Color::Aqua: return "aqua";
        case ScoreTeam::Color::Purple: return "purple";
        case ScoreTeam::Color::Gold: return "gold";
        case ScoreTeam::Color::Count: break;
    }
    return "unknown";
}

bool Scoreboard::addObjective(const ScoreObjective& objective) {
    if (objective.name.empty()) return false;
    if (objectives.count(objective.name) > 0u) return false;
    objectives[objective.name] = objective;
    return true;
}

bool Scoreboard::removeObjective(const std::string& name) {
    if (objectives.erase(name) == 0u) return false;
    manualScores.erase(name);
    for (std::string& slot : slots)
        if (slot == name) slot.clear();
    return true;
}

const ScoreObjective* Scoreboard::objective(const std::string& name) const {
    auto found = objectives.find(name);
    return found == objectives.end() ? nullptr : &found->second;
}

std::vector<std::string> Scoreboard::objectiveNames() const {
    std::vector<std::string> out;
    out.reserve(objectives.size());
    for (const auto& entry : objectives) out.push_back(entry.first);
    return out;
}

StatSheet& Scoreboard::stats(const std::string& player) { return sheets[player]; }

const StatSheet* Scoreboard::statsFor(const std::string& player) const {
    auto found = sheets.find(player);
    return found == sheets.end() ? nullptr : &found->second;
}

std::vector<std::string> Scoreboard::players() const {
    std::vector<std::string> out;
    for (const auto& sheet : sheets) out.push_back(sheet.first);
    for (const auto& objectiveScores : manualScores)
        for (const auto& entry : objectiveScores.second)
            if (std::find(out.begin(), out.end(), entry.first) == out.end())
                out.push_back(entry.first);
    for (const auto& membership : teamByPlayer)
        if (std::find(out.begin(), out.end(), membership.first) == out.end())
            out.push_back(membership.first);
    std::sort(out.begin(), out.end());
    return out;
}

bool Scoreboard::setScore(const std::string& objectiveName, const std::string& player, int value) {
    const ScoreObjective* target = objective(objectiveName);
    if (target == nullptr || target->automatic() || player.empty()) return false;
    manualScores[objectiveName][player] = value;
    return true;
}

bool Scoreboard::addScore(const std::string& objectiveName, const std::string& player, int delta) {
    const ScoreObjective* target = objective(objectiveName);
    if (target == nullptr || target->automatic() || player.empty()) return false;
    long long updated = static_cast<long long>(score(objectiveName, player)) + delta;
    manualScores[objectiveName][player] = clampToScore(updated);
    return true;
}

bool Scoreboard::resetScore(const std::string& objectiveName, const std::string& player) {
    auto found = manualScores.find(objectiveName);
    if (found == manualScores.end()) return false;
    return found->second.erase(player) > 0u;
}

void Scoreboard::resetPlayer(const std::string& player) {
    for (auto& objectiveScores : manualScores) objectiveScores.second.erase(player);
    sheets.erase(player);
}

int Scoreboard::statScore(const ScoreObjective& target, const std::string& player) const {
    const StatSheet* sheet = statsFor(player);
    if (sheet == nullptr) return 0;
    if (target.statSubject < 0) return clampToScore(sheet->total(target.statKind));
    return clampToScore(sheet->value(target.statKind, target.statSubject));
}

int Scoreboard::score(const std::string& objectiveName, const std::string& player) const {
    const ScoreObjective* target = objective(objectiveName);
    if (target == nullptr) return 0;
    if (target->automatic()) return statScore(*target, player);
    auto objectiveScores = manualScores.find(objectiveName);
    if (objectiveScores == manualScores.end()) return 0;
    auto entry = objectiveScores->second.find(player);
    return entry == objectiveScores->second.end() ? 0 : entry->second;
}

bool Scoreboard::tracked(const std::string& objectiveName, const std::string& player) const {
    const ScoreObjective* target = objective(objectiveName);
    if (target == nullptr) return false;
    if (target->automatic()) return sheets.count(player) > 0u;
    auto objectiveScores = manualScores.find(objectiveName);
    if (objectiveScores == manualScores.end()) return false;
    return objectiveScores->second.count(player) > 0u;
}

std::vector<ScoreEntry> Scoreboard::entries(const std::string& objectiveName) const {
    std::vector<ScoreEntry> out;
    const ScoreObjective* target = objective(objectiveName);
    if (target == nullptr) return out;
    if (target->automatic()) {
        for (const auto& sheet : sheets) {
            ScoreEntry entry;
            entry.player = sheet.first;
            entry.score = statScore(*target, sheet.first);
            out.push_back(entry);
        }
    } else {
        auto objectiveScores = manualScores.find(objectiveName);
        if (objectiveScores != manualScores.end()) {
            for (const auto& scored : objectiveScores->second) {
                ScoreEntry entry;
                entry.player = scored.first;
                entry.score = scored.second;
                out.push_back(entry);
            }
        }
    }
    const bool ascending = target->sort == ScoreObjective::Sort::Ascending;
    std::sort(out.begin(), out.end(), [ascending](const ScoreEntry& a, const ScoreEntry& b) {
        if (a.score != b.score) return ascending ? a.score < b.score : a.score > b.score;
        return a.player < b.player;
    });
    for (ScoreEntry& entry : out) entry.formatted = target->formatScore(entry.score);
    return out;
}

void Scoreboard::setDisplay(DisplaySlot slot, const std::string& objectiveName) {
    if (slot == DisplaySlot::Count) return;
    if (objectives.count(objectiveName) == 0u) return;
    slots[static_cast<std::size_t>(slot)] = objectiveName;
}

void Scoreboard::clearDisplay(DisplaySlot slot) {
    if (slot == DisplaySlot::Count) return;
    slots[static_cast<std::size_t>(slot)].clear();
}

const std::string& Scoreboard::displayedObjective(DisplaySlot slot) const {
    static const std::string none;
    if (slot == DisplaySlot::Count) return none;
    return slots[static_cast<std::size_t>(slot)];
}

std::vector<ScoreEntry> Scoreboard::displayLines(DisplaySlot slot, int limit) const {
    std::vector<ScoreEntry> lines = entries(displayedObjective(slot));
    if (limit >= 0 && static_cast<int>(lines.size()) > limit)
        lines.resize(static_cast<std::size_t>(limit));
    return lines;
}

bool Scoreboard::addTeam(const ScoreTeam& team) {
    if (team.name.empty()) return false;
    if (teams.count(team.name) > 0u) return false;
    ScoreTeam stored = team;
    stored.members.clear();
    teams[team.name] = stored;
    for (const std::string& member : team.members) joinTeam(team.name, member);
    return true;
}

bool Scoreboard::removeTeam(const std::string& name) {
    auto found = teams.find(name);
    if (found == teams.end()) return false;
    for (const std::string& member : found->second.members) teamByPlayer.erase(member);
    teams.erase(found);
    return true;
}

const ScoreTeam* Scoreboard::team(const std::string& name) const {
    auto found = teams.find(name);
    return found == teams.end() ? nullptr : &found->second;
}

const ScoreTeam* Scoreboard::teamOf(const std::string& player) const {
    auto membership = teamByPlayer.find(player);
    if (membership == teamByPlayer.end()) return nullptr;
    return team(membership->second);
}

bool Scoreboard::joinTeam(const std::string& teamName, const std::string& player) {
    auto found = teams.find(teamName);
    if (found == teams.end() || player.empty()) return false;
    leaveTeam(player);
    found->second.members.push_back(player);
    std::sort(found->second.members.begin(), found->second.members.end());
    teamByPlayer[player] = teamName;
    return true;
}

bool Scoreboard::leaveTeam(const std::string& player) {
    auto membership = teamByPlayer.find(player);
    if (membership == teamByPlayer.end()) return false;
    auto found = teams.find(membership->second);
    if (found != teams.end()) {
        std::vector<std::string>& members = found->second.members;
        members.erase(std::remove(members.begin(), members.end(), player), members.end());
    }
    teamByPlayer.erase(membership);
    return true;
}

std::vector<std::string> Scoreboard::teamNames() const {
    std::vector<std::string> out;
    out.reserve(teams.size());
    for (const auto& entry : teams) out.push_back(entry.first);
    return out;
}

bool Scoreboard::friendlyFireAllowed(const std::string& attacker, const std::string& victim) const {
    const ScoreTeam* attackerTeam = teamOf(attacker);
    if (attackerTeam == nullptr) return true;
    if (!attackerTeam->hasMember(victim)) return true;
    return attackerTeam->friendlyFire;
}

bool Scoreboard::nameVisibleTo(const std::string& viewer, const std::string& target) const {
    const ScoreTeam* targetTeam = teamOf(target);
    if (targetTeam == nullptr) return true;
    const bool sameTeam = targetTeam->hasMember(viewer);
    switch (targetTeam->nameVisibility) {
        case ScoreTeam::Visibility::Always: return true;
        case ScoreTeam::Visibility::Never: return false;
        case ScoreTeam::Visibility::HideForOtherTeams: return sameTeam;
        case ScoreTeam::Visibility::HideForOwnTeam: return !sameTeam;
    }
    return true;
}

std::string Scoreboard::displayNameOf(const std::string& player) const {
    const ScoreTeam* membership = teamOf(player);
    if (membership == nullptr) return player;
    return membership->prefix + player + membership->suffix;
}

}
