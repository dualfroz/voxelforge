#include "testHarness.h"
#include "vf/Scoreboard.h"

#include <string>

using namespace vf;

namespace {

ScoreObjective manualObjective(const std::string& name) {
    ScoreObjective objective;
    objective.name = name;
    objective.displayName = name;
    return objective;
}

ScoreObjective statObjective(const std::string& name, StatKind kind, int subject = -1) {
    ScoreObjective objective = manualObjective(name);
    objective.criterion = ScoreObjective::Criterion::Statistic;
    objective.statKind = kind;
    objective.statSubject = subject;
    return objective;
}

ScoreTeam makeTeam(const std::string& name, ScoreTeam::Color color) {
    ScoreTeam team;
    team.name = name;
    team.displayName = name;
    team.color = color;
    return team;
}

} // namespace

VF_TEST(statSheetsTallyEveryKindWithAndWithoutASubject) {
    StatSheet sheet;
    CHECK_EQ(sheet.revision(), 0);
    CHECK_EQ(sheet.total(StatKind::Deaths), 0);

    sheet.add(StatKind::Deaths, 1);
    sheet.add(StatKind::PlayTicks, 12000);
    sheet.add(StatKind::DistanceWalked, 450);
    sheet.add(StatKind::DistanceSprinted, 1200);
    sheet.addBlockBroken(Block::Stone, 40);
    sheet.addBlockBroken(Block::Dirt, 7);
    sheet.addItemUsed(ItemId::IronPickaxe, 3);
    sheet.addMobKilled(Species::Zombie, 2);
    sheet.addMobKilled(Species::Creeper);

    CHECK_EQ(sheet.total(StatKind::Deaths), 1);
    CHECK_EQ(sheet.total(StatKind::PlayTicks), 12000);
    CHECK_EQ(sheet.blocksBroken(Block::Stone), 40);
    CHECK_EQ(sheet.blocksBroken(Block::Sand), 0);
    CHECK_EQ(sheet.total(StatKind::BlocksBroken), 47);
    CHECK_EQ(sheet.itemsUsed(ItemId::IronPickaxe), 3);
    CHECK_EQ(sheet.mobsKilled(Species::Zombie), 2);
    CHECK_EQ(sheet.mobsKilled(Species::Creeper), 1);
    CHECK_EQ(sheet.total(StatKind::MobsKilled), 3);
    CHECK_EQ(sheet.distanceTravelled(), 1650);
    CHECK_EQ(sheet.distinctSubjects(StatKind::BlocksBroken), 2);
    CHECK_EQ(sheet.distinctSubjects(StatKind::Deaths), 1);

    long long before = sheet.revision();
    sheet.add(StatKind::Deaths, 0);
    CHECK_EQ(sheet.revision(), before);
    sheet.add(StatKind::Deaths, 1);
    CHECK_EQ(sheet.revision(), before + 1);
    CHECK_EQ(sheet.total(StatKind::Deaths), 2);

    std::vector<StatCounter> counters = sheet.counters();
    CHECK_EQ(counters.size(), static_cast<std::size_t>(9));
    for (std::size_t i = 1u; i < counters.size(); ++i) {
        bool ordered = counters[i - 1u].kind < counters[i].kind ||
                       (counters[i - 1u].kind == counters[i].kind &&
                        counters[i - 1u].subject < counters[i].subject);
        CHECK(ordered);
    }

    sheet.clear();
    CHECK_EQ(sheet.counters().size(), static_cast<std::size_t>(0));
    CHECK_EQ(sheet.total(StatKind::BlocksBroken), 0);
    CHECK(sheet.revision() > before);

    for (int i = 0; i < static_cast<int>(StatKind::Count); ++i) {
        StatKind kind = static_cast<StatKind>(i);
        StatKind reparsed = StatKind::Count;
        CHECK(parseStatKindName(statKindName(kind), reparsed));
        CHECK(reparsed == kind);
    }
    StatKind unknown = StatKind::Count;
    CHECK(!parseStatKindName("blocksPlaced", unknown));
    CHECK(statKindHasSubject(StatKind::BlocksBroken));
    CHECK(!statKindHasSubject(StatKind::Deaths));
    CHECK(statKindIsDistance(StatKind::DistanceFallen));
    CHECK(!statKindIsDistance(StatKind::PlayTicks));
}

VF_TEST(anAutomaticObjectiveFollowsTheStatSheetWithNoUpdateStep) {
    Scoreboard board;
    CHECK(board.addObjective(statObjective("mined", StatKind::BlocksBroken)));
    CHECK(board.addObjective(statObjective("stone", StatKind::BlocksBroken,
                                           static_cast<int>(Block::Stone))));
    CHECK(board.addObjective(statObjective("deaths", StatKind::Deaths)));

    CHECK_EQ(board.score("mined", "foksiu"), 0);
    CHECK(!board.tracked("mined", "foksiu"));

    board.stats("foksiu").addBlockBroken(Block::Stone, 12);
    CHECK(board.tracked("mined", "foksiu"));
    CHECK_EQ(board.score("mined", "foksiu"), 12);
    CHECK_EQ(board.score("stone", "foksiu"), 12);

    board.stats("foksiu").addBlockBroken(Block::Dirt, 5);
    CHECK_EQ(board.score("mined", "foksiu"), 17);
    CHECK_EQ(board.score("stone", "foksiu"), 12);

    board.stats("foksiu").add(StatKind::Deaths, 1);
    CHECK_EQ(board.score("deaths", "foksiu"), 1);
    CHECK_EQ(board.score("mined", "foksiu"), 17);

    // An automatic objective refuses to be written by hand.
    CHECK(!board.setScore("mined", "foksiu", 999));
    CHECK(!board.addScore("mined", "foksiu", 1));
    CHECK_EQ(board.score("mined", "foksiu"), 17);

    CHECK(!board.addObjective(statObjective("mined", StatKind::Deaths)));
    CHECK_EQ(board.objectiveNames().size(), static_cast<std::size_t>(3));
    CHECK(board.objective("mined")->automatic());
    CHECK(board.objective("nothing") == nullptr);
    CHECK_EQ(board.score("nothing", "foksiu"), 0);
}

VF_TEST(manualObjectivesOnlyMoveWhenSomethingSetsThem) {
    Scoreboard board;
    CHECK(board.addObjective(manualObjective("bounty")));
    CHECK(!board.addObjective(manualObjective("")));

    CHECK(!board.tracked("bounty", "foksiu"));
    CHECK(board.setScore("bounty", "foksiu", 10));
    CHECK(board.tracked("bounty", "foksiu"));
    CHECK_EQ(board.score("bounty", "foksiu"), 10);
    CHECK(board.addScore("bounty", "foksiu", -4));
    CHECK_EQ(board.score("bounty", "foksiu"), 6);
    CHECK(board.addScore("bounty", "newcomer", 3));
    CHECK_EQ(board.score("bounty", "newcomer"), 3);

    CHECK(!board.setScore("absent", "foksiu", 1));
    CHECK(board.resetScore("bounty", "foksiu"));
    CHECK(!board.resetScore("bounty", "foksiu"));
    CHECK(!board.tracked("bounty", "foksiu"));
    CHECK_EQ(board.score("bounty", "foksiu"), 0);

    board.stats("newcomer").add(StatKind::Deaths, 2);
    board.resetPlayer("newcomer");
    CHECK(!board.tracked("bounty", "newcomer"));
    CHECK(board.statsFor("newcomer") == nullptr);

    board.setScore("bounty", "a", 2147483647);
    CHECK(board.addScore("bounty", "a", 10));
    CHECK_EQ(board.score("bounty", "a"), 2147483647);
}

VF_TEST(entriesComeBackSortedAndFormattedForTheirObjective) {
    Scoreboard board;
    ScoreObjective bounty = manualObjective("bounty");
    board.addObjective(bounty);
    board.setScore("bounty", "cara", 5);
    board.setScore("bounty", "bea", 9);
    board.setScore("bounty", "abel", 5);

    std::vector<ScoreEntry> ranked = board.entries("bounty");
    CHECK_EQ(ranked.size(), static_cast<std::size_t>(3));
    CHECK_EQ(ranked[0].player, std::string("bea"));
    CHECK_EQ(ranked[1].player, std::string("abel"));
    CHECK_EQ(ranked[2].player, std::string("cara"));
    CHECK_EQ(ranked[0].formatted, std::string("9"));

    ScoreObjective ascending = manualObjective("race");
    ascending.sort = ScoreObjective::Sort::Ascending;
    ascending.format = ScoreObjective::Format::Duration;
    board.addObjective(ascending);
    board.setScore("race", "bea", 20 * 65);
    board.setScore("race", "abel", 20 * 3661);
    board.setScore("race", "cara", 20 * 9);
    std::vector<ScoreEntry> fastest = board.entries("race");
    CHECK_EQ(fastest[0].player, std::string("cara"));
    CHECK_EQ(fastest[0].formatted, std::string("9s"));
    CHECK_EQ(fastest[1].formatted, std::string("1m 05s"));
    CHECK_EQ(fastest[2].formatted, std::string("1h 01m 01s"));

    ScoreObjective walked = statObjective("walked", StatKind::DistanceWalked);
    walked.format = ScoreObjective::Format::Distance;
    board.addObjective(walked);
    board.stats("abel").add(StatKind::DistanceWalked, 123456);
    board.stats("bea").add(StatKind::DistanceWalked, 7);
    std::vector<ScoreEntry> distances = board.entries("walked");
    CHECK_EQ(distances.size(), static_cast<std::size_t>(2));
    CHECK_EQ(distances[0].player, std::string("abel"));
    CHECK_EQ(distances[0].formatted, std::string("1234.56 m"));
    CHECK_EQ(distances[1].formatted, std::string("0.07 m"));

    CHECK(board.entries("missing").empty());
    CHECK_EQ(walked.formatScore(-250), std::string("-2.50 m"));
    CHECK_EQ(ascending.formatScore(0), std::string("0s"));
}

VF_TEST(displaySlotsShowOneObjectiveEachAndCanBeCleared) {
    Scoreboard board;
    board.addObjective(manualObjective("bounty"));
    board.addObjective(statObjective("deaths", StatKind::Deaths));
    board.setScore("bounty", "abel", 3);
    board.setScore("bounty", "bea", 8);
    board.setScore("bounty", "cara", 1);

    CHECK(board.displayedObjective(DisplaySlot::Sidebar).empty());
    board.setDisplay(DisplaySlot::Sidebar, "bounty");
    board.setDisplay(DisplaySlot::PlayerList, "deaths");
    board.setDisplay(DisplaySlot::BelowName, "nothing at all");
    CHECK_EQ(board.displayedObjective(DisplaySlot::Sidebar), std::string("bounty"));
    CHECK_EQ(board.displayedObjective(DisplaySlot::PlayerList), std::string("deaths"));
    CHECK(board.displayedObjective(DisplaySlot::BelowName).empty());

    std::vector<ScoreEntry> sidebar = board.displayLines(DisplaySlot::Sidebar, 2);
    CHECK_EQ(sidebar.size(), static_cast<std::size_t>(2));
    CHECK_EQ(sidebar[0].player, std::string("bea"));
    CHECK_EQ(sidebar[1].player, std::string("abel"));
    CHECK_EQ(board.displayLines(DisplaySlot::Sidebar, 15).size(), static_cast<std::size_t>(3));
    CHECK(board.displayLines(DisplaySlot::BelowName, 15).empty());

    board.clearDisplay(DisplaySlot::Sidebar);
    CHECK(board.displayedObjective(DisplaySlot::Sidebar).empty());
    CHECK(board.displayLines(DisplaySlot::Sidebar, 15).empty());

    board.setDisplay(DisplaySlot::Sidebar, "bounty");
    CHECK(board.removeObjective("bounty"));
    CHECK(!board.removeObjective("bounty"));
    CHECK(board.displayedObjective(DisplaySlot::Sidebar).empty());
    CHECK_EQ(board.score("bounty", "bea"), 0);

    CHECK_EQ(std::string(displaySlotName(DisplaySlot::Sidebar)), std::string("sidebar"));
    CHECK_EQ(std::string(displaySlotName(DisplaySlot::BelowName)), std::string("belowName"));
}

VF_TEST(teamsDecideFriendlyFireNameVisibilityAndDisplayName) {
    Scoreboard board;
    ScoreTeam red = makeTeam("red", ScoreTeam::Color::Red);
    red.friendlyFire = false;
    red.prefix = "[R] ";
    red.suffix = " *";
    red.members.push_back("abel");
    red.members.push_back("bea");
    CHECK(board.addTeam(red));
    CHECK(!board.addTeam(red));
    CHECK(board.addTeam(makeTeam("blue", ScoreTeam::Color::Blue)));
    CHECK(board.joinTeam("blue", "cara"));
    CHECK(!board.joinTeam("green", "cara"));

    CHECK_EQ(board.teamNames().size(), static_cast<std::size_t>(2));
    CHECK(board.teamOf("abel") != nullptr);
    CHECK_EQ(board.teamOf("abel")->name, std::string("red"));
    CHECK_EQ(board.team("red")->members.size(), static_cast<std::size_t>(2));
    CHECK(board.teamOf("nobody") == nullptr);

    CHECK(!board.friendlyFireAllowed("abel", "bea"));
    CHECK(board.friendlyFireAllowed("abel", "cara"));
    CHECK(board.friendlyFireAllowed("cara", "abel"));
    CHECK(board.friendlyFireAllowed("nobody", "abel"));

    CHECK_EQ(board.displayNameOf("abel"), std::string("[R] abel *"));
    CHECK_EQ(board.displayNameOf("cara"), std::string("cara"));

    // Moving a player leaves the old roster behind.
    CHECK(board.joinTeam("blue", "bea"));
    CHECK_EQ(board.team("red")->members.size(), static_cast<std::size_t>(1));
    CHECK_EQ(board.teamOf("bea")->name, std::string("blue"));
    CHECK(board.friendlyFireAllowed("abel", "bea"));

    CHECK(board.leaveTeam("bea"));
    CHECK(!board.leaveTeam("bea"));
    CHECK(board.teamOf("bea") == nullptr);
    CHECK_EQ(board.team("blue")->members.size(), static_cast<std::size_t>(1));

    CHECK(board.removeTeam("blue"));
    CHECK(!board.removeTeam("blue"));
    CHECK(board.teamOf("cara") == nullptr);
    CHECK_EQ(std::string(scoreTeamColorName(ScoreTeam::Color::Red)), std::string("red"));
    CHECK_EQ(std::string(scoreTeamColorName(ScoreTeam::Color::Aqua)), std::string("aqua"));
}

VF_TEST(nameVisibilityFollowsTheTeamRuleForTheWatcher) {
    Scoreboard board;
    ScoreTeam hidden = makeTeam("hidden", ScoreTeam::Color::Gray);
    hidden.nameVisibility = ScoreTeam::Visibility::Never;
    hidden.members.push_back("ghost");
    board.addTeam(hidden);

    ScoreTeam clan = makeTeam("clan", ScoreTeam::Color::Green);
    clan.nameVisibility = ScoreTeam::Visibility::HideForOtherTeams;
    clan.members.push_back("abel");
    clan.members.push_back("bea");
    board.addTeam(clan);

    ScoreTeam masked = makeTeam("masked", ScoreTeam::Color::Purple);
    masked.nameVisibility = ScoreTeam::Visibility::HideForOwnTeam;
    masked.members.push_back("cara");
    masked.members.push_back("dana");
    board.addTeam(masked);

    CHECK(!board.nameVisibleTo("abel", "ghost"));
    CHECK(!board.nameVisibleTo("ghost", "ghost"));

    CHECK(board.nameVisibleTo("abel", "bea"));
    CHECK(!board.nameVisibleTo("cara", "bea"));

    CHECK(!board.nameVisibleTo("dana", "cara"));
    CHECK(board.nameVisibleTo("abel", "cara"));

    CHECK(board.nameVisibleTo("abel", "loner"));
    CHECK(board.nameVisibleTo("loner", "loner"));
}

VF_TEST(everyPlayerTouchedByTheBoardIsListedOnceInOrder) {
    Scoreboard board;
    board.addObjective(manualObjective("bounty"));
    board.addObjective(statObjective("mined", StatKind::BlocksBroken));
    board.addTeam(makeTeam("red", ScoreTeam::Color::Red));

    board.setScore("bounty", "cara", 1);
    board.stats("abel").addBlockBroken(Block::Stone);
    board.joinTeam("red", "bea");
    board.stats("cara").addBlockBroken(Block::Dirt, 4);

    std::vector<std::string> everyone = board.players();
    CHECK_EQ(everyone.size(), static_cast<std::size_t>(3));
    CHECK_EQ(everyone[0], std::string("abel"));
    CHECK_EQ(everyone[1], std::string("bea"));
    CHECK_EQ(everyone[2], std::string("cara"));

    // The automatic column only lists players who actually have a sheet.
    std::vector<ScoreEntry> mined = board.entries("mined");
    CHECK_EQ(mined.size(), static_cast<std::size_t>(2));
    CHECK_EQ(mined[0].player, std::string("cara"));
    CHECK_EQ(mined[0].score, 4);
    CHECK_EQ(mined[1].player, std::string("abel"));
}

VF_TEST(aStatisticObjectiveOnOneSubjectIgnoresTheOthers) {
    Scoreboard board;
    board.addObjective(statObjective("zombieHunter", StatKind::MobsKilled,
                                     static_cast<int>(Species::Zombie)));
    board.addObjective(statObjective("anyKill", StatKind::MobsKilled));
    board.addObjective(statObjective("pickaxeUse", StatKind::ItemsUsed,
                                     static_cast<int>(ItemId::IronPickaxe)));

    StatSheet& sheet = board.stats("foksiu");
    sheet.addMobKilled(Species::Zombie, 3);
    sheet.addMobKilled(Species::Sheep, 9);
    sheet.addItemUsed(ItemId::IronPickaxe, 2);
    sheet.addItemUsed(ItemId::WoodAxe, 40);

    CHECK_EQ(board.score("zombieHunter", "foksiu"), 3);
    CHECK_EQ(board.score("anyKill", "foksiu"), 12);
    CHECK_EQ(board.score("pickaxeUse", "foksiu"), 2);

    sheet.addMobKilled(Species::Zombie);
    CHECK_EQ(board.score("zombieHunter", "foksiu"), 4);
    CHECK_EQ(board.score("anyKill", "foksiu"), 13);

    // A stat far past the score range is clamped, not wrapped.
    sheet.add(StatKind::PlayTicks, 4000000000ll);
    board.addObjective(statObjective("played", StatKind::PlayTicks));
    CHECK_EQ(board.score("played", "foksiu"), 2147483647);
}
