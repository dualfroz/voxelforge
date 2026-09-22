#include "testHarness.h"
#include "vf/Boss.h"

#include <string>
#include <vector>

using namespace vf;

namespace {

DragonPlayerInput clingingTo(const DragonFight& fight, float melee) {
    DragonPlayerInput input;
    input.present = true;
    input.position = fight.state().position;
    input.meleeDamage = melee;
    return input;
}

int shatterEveryCrystal(DragonFight& fight) {
    int destroyed = 0;
    for (int index = 0; index < static_cast<int>(fight.crystals().size()); ++index) {
        if (fight.crystals()[static_cast<std::size_t>(index)].cageIntact) {
            DragonPlayerInput breaking = clingingTo(fight, 0.0f);
            breaking.shootCrystal = index;
            breaking.breakCage = true;
            fight.tick(breaking);
        }
        DragonPlayerInput shot = clingingTo(fight, 0.0f);
        shot.shootCrystal = index;
        destroyed += fight.tick(shot).crystalsDestroyed;
    }
    return destroyed;
}

WitherFightConfig quickSummon(int summonTicks) {
    WitherFightConfig config;
    config.summonTicks = summonTicks;
    return config;
}

void runUpToRangedPhase(WitherFight& fight, std::vector<WitherTarget>& targets) {
    std::vector<WitherHit> nothing;
    for (int step = 0; step < 200 && fight.state().phase != BossPhase::Ranged; ++step) {
        fight.tick(nothing, targets);
    }
}

World witherAltar(const Vec3i& base, bool alongX) {
    World world;
    world.setBlock(base.x, base.y, base.z, witherBaseBlock());
    for (int along = -1; along <= 1; ++along) {
        int ax = alongX ? base.x + along : base.x;
        int az = alongX ? base.z : base.z + along;
        world.setBlock(ax, base.y + 1, az, witherBaseBlock());
        world.setBlock(ax, base.y + 2, az, witherHeadBlock());
    }
    return world;
}

}

VF_TEST(everyBossKindAndPhaseHasAName) {
    for (int index = 0; index < static_cast<int>(BossKind::Count); ++index) {
        CHECK(std::string(bossKindName(static_cast<BossKind>(index))) != std::string("unknown"));
    }
    for (int index = 0; index < static_cast<int>(BossPhase::Count); ++index) {
        CHECK(std::string(bossPhaseName(static_cast<BossPhase>(index))) != std::string("unknown"));
    }
    CHECK(std::string(bossPhaseName(BossPhase::Armored)) == std::string("armored"));
}

VF_TEST(theBossBarTranslatesHealthIntoNotchesAndPercent) {
    BossBar bar;
    bar.style = BossBarStyle::Notched10;
    bar.setFromHealth(200.0f, 200.0f);
    CHECK_NEAR(bar.progress, 1.0f, 1e-6f);
    CHECK_EQ(bar.notchCount(), 10);
    CHECK_EQ(bar.filledNotches(), 10);
    CHECK_EQ(bar.percent(), 100);

    bar.setFromHealth(95.0f, 200.0f);
    CHECK_EQ(bar.filledNotches(), 4);
    CHECK_EQ(bar.percent(), 48);

    bar.setFromHealth(0.0f, 200.0f);
    CHECK_EQ(bar.filledNotches(), 0);
    CHECK_EQ(bar.percent(), 0);

    bar.setFromHealth(500.0f, 200.0f);
    CHECK_NEAR(bar.progress, 1.0f, 1e-6f);
    bar.setFromHealth(-10.0f, 200.0f);
    CHECK_NEAR(bar.progress, 0.0f, 1e-6f);

    bar.style = BossBarStyle::Notched6;
    bar.setFromHealth(150.0f, 300.0f);
    CHECK_EQ(bar.notchCount(), 6);
    CHECK_EQ(bar.filledNotches(), 3);

    bar.style = BossBarStyle::Solid;
    CHECK_EQ(bar.notchCount(), 1);
    bar.style = BossBarStyle::Notched12;
    CHECK_EQ(bar.notchCount(), 12);
    bar.style = BossBarStyle::Notched20;
    CHECK_EQ(bar.notchCount(), 20);
}

VF_TEST(crystalsAreBuiltOnTopOfTheGeneratedObsidianPillars) {
    EndGenerator end;
    std::vector<ObsidianPillar> pillars = end.pillars();
    std::vector<CrystalNode> crystals = crystalsFromPillars(pillars);

    CHECK_EQ(crystals.size(), pillars.size());
    CHECK(crystals.size() >= 8u);

    int caged = 0;
    for (std::size_t i = 0; i < crystals.size(); ++i) {
        CHECK(crystals[i].alive);
        CHECK_EQ(crystals[i].position.x, pillars[i].base.x);
        CHECK_EQ(crystals[i].position.z, pillars[i].base.z);
        CHECK_EQ(crystals[i].position.y, pillars[i].base.y + pillars[i].height + 1);
        CHECK(crystals[i].caged == pillars[i].crowned);
        CHECK(crystals[i].cageIntact == pillars[i].crowned);
        if (crystals[i].caged) ++caged;
    }
    CHECK(caged > 0);
}

VF_TEST(theDragonOrbitsTheGeneratedExitPortalColumn) {
    EndGenerator end;
    DragonFight fight(end, 1234u);

    CHECK(fight.state().kind == BossKind::EnderDragon);
    CHECK(fight.state().bar.visible);
    CHECK(fight.state().bar.color == BossBarColor::Pink);
    CHECK_NEAR(fight.portalCenter().y, static_cast<float>(end.surfaceHeight(0, 0)), 1e-4f);
    CHECK_EQ(fight.eggPosition().y, end.surfaceHeight(0, 0) + 1);
    CHECK(fight.brain().structureValid());

    DragonPlayerInput idle;
    idle.present = false;
    float startAngle = fight.state().orbitAngle;
    for (int step = 0; step < 20; ++step) fight.tick(idle);

    CHECK(fight.state().phase == BossPhase::Circling);
    CHECK(fight.state().orbitAngle > startAngle);
    float radius = behaviorHorizontalDistance(fight.state().position, fight.portalCenter());
    CHECK_NEAR(radius, fight.settings().orbitRadius, 0.05f);
    CHECK_EQ(fight.aliveCrystalCount(), static_cast<int>(fight.crystals().size()));
}

VF_TEST(livingCrystalsHealTheDragonFasterThanTheSwordDrainsIt) {
    EndGenerator end;
    DragonFight fight(end, 99u);
    float fullHealth = fight.state().health;

    float totalHealed = 0.0f;
    for (int step = 0; step < 200; ++step) {
        BossTickReport report = fight.tick(clingingTo(fight, 5.0f));
        totalHealed += report.healedThisTick;
    }

    CHECK(totalHealed > 0.0f);
    CHECK(fight.state().damageTaken > 0.0f);
    CHECK_NEAR(fight.state().health, fullHealth, 1e-3f);
    CHECK(!fight.exitPortalOpen());
    CHECK(fight.aliveCrystalCount() == static_cast<int>(fight.crystals().size()));
}

VF_TEST(aCagedCrystalIgnoresShotsUntilTheCageComesDown) {
    EndGenerator end;
    DragonFight fight(end, 7u);

    int cagedIndex = -1;
    for (int index = 0; index < static_cast<int>(fight.crystals().size()); ++index) {
        if (fight.crystals()[static_cast<std::size_t>(index)].cageIntact) {
            cagedIndex = index;
            break;
        }
    }
    CHECK(cagedIndex >= 0);
    if (cagedIndex < 0) return;

    DragonPlayerInput blocked = clingingTo(fight, 0.0f);
    blocked.shootCrystal = cagedIndex;
    CHECK_EQ(fight.tick(blocked).crystalsDestroyed, 0);
    CHECK(fight.crystals()[static_cast<std::size_t>(cagedIndex)].alive);

    DragonPlayerInput breaking = blocked;
    breaking.breakCage = true;
    CHECK_EQ(fight.tick(breaking).crystalsDestroyed, 0);
    CHECK(!fight.crystals()[static_cast<std::size_t>(cagedIndex)].cageIntact);

    CHECK_EQ(fight.tick(blocked).crystalsDestroyed, 1);
    CHECK(!fight.crystals()[static_cast<std::size_t>(cagedIndex)].alive);

    CHECK_EQ(fight.tick(blocked).crystalsDestroyed, 0);
    DragonPlayerInput bogus = clingingTo(fight, 0.0f);
    bogus.shootCrystal = 9999;
    CHECK_EQ(fight.tick(bogus).crystalsDestroyed, 0);
}

VF_TEST(theWholeDragonFightRunsFromTheCrystalsToTheEgg) {
    EndGenerator end;
    DragonFight fight(end, 4242u);
    int crystalCount = static_cast<int>(fight.crystals().size());

    CHECK_EQ(shatterEveryCrystal(fight), crystalCount);
    CHECK_EQ(fight.aliveCrystalCount(), 0);

    int descentTicks = 0;
    while (fight.state().phase != BossPhase::Perching && descentTicks < 200) {
        fight.tick(clingingTo(fight, 0.0f));
        ++descentTicks;
    }
    CHECK(fight.state().phase == BossPhase::Perching);

    int settleTicks = 0;
    while (fight.state().position.y > fight.portalCenter().y + fight.settings().perchHeight + 0.01f &&
           settleTicks < 300) {
        fight.tick(clingingTo(fight, 0.0f));
        ++settleTicks;
    }
    CHECK(settleTicks < 300);
    CHECK_NEAR(fight.state().position.y,
               fight.portalCenter().y + fight.settings().perchHeight, 1e-3f);

    BossTickReport report;
    int killTicks = 0;
    while (!report.defeated && killTicks < 600) {
        report = fight.tick(clingingTo(fight, 8.0f));
        ++killTicks;
    }

    CHECK(report.defeated);
    CHECK(fight.state().phase == BossPhase::Defeated);
    CHECK(!fight.state().alive);
    CHECK(!fight.state().bar.visible);
    CHECK_NEAR(fight.state().health, 0.0f, 1e-4f);
    CHECK(fight.state().attacksLaunched > 0);
    CHECK(fight.exitPortalOpen());
    CHECK(fight.dragonEggDropped());
    CHECK(fight.eggPosition() == Vec3i(0, end.surfaceHeight(0, 0) + 1, 0));

    BossTickReport after = fight.tick(clingingTo(fight, 8.0f));
    CHECK(after.defeated);
    CHECK(!after.phaseChanged);

    World world;
    CHECK(fight.placeVictoryStructures(world, end));
    CHECK(world.blockAt(0, end.surfaceHeight(0, 0), 0) == Block::EndPortal);
    CHECK(world.blockAt(2, end.surfaceHeight(0, 0), 2) == Block::Bedrock);
}

VF_TEST(aLivingDragonRefusesToHandOverItsRewards) {
    EndGenerator end;
    DragonFight fight(end, 5u);
    World world;
    CHECK(!fight.placeVictoryStructures(world, end));
    CHECK_EQ(world.chunkCount(), 0u);
    CHECK(!fight.dragonEggDropped());
}

VF_TEST(twoDragonFightsOnTheSameSeedAndScriptRunIdentically) {
    EndGenerator end;

    auto playOut = [&end](std::vector<float>& healthTrack, Vec3& endPosition, int& attacks) {
        DragonFight fight(end, 31337u);
        shatterEveryCrystal(fight);
        for (int step = 0; step < 300; ++step) {
            BossTickReport report = fight.tick(clingingTo(fight, 3.0f));
            healthTrack.push_back(report.bossHealth);
        }
        endPosition = fight.state().position;
        attacks = fight.state().attacksLaunched;
    };

    std::vector<float> firstTrack;
    std::vector<float> secondTrack;
    Vec3 firstEnd;
    Vec3 secondEnd;
    int firstAttacks = 0;
    int secondAttacks = 0;
    playOut(firstTrack, firstEnd, firstAttacks);
    playOut(secondTrack, secondEnd, secondAttacks);

    CHECK_EQ(firstTrack.size(), secondTrack.size());
    for (std::size_t i = 0; i < firstTrack.size() && i < secondTrack.size(); ++i) {
        CHECK_NEAR(firstTrack[i], secondTrack[i], 1e-6f);
    }
    CHECK_NEAR(firstEnd.x, secondEnd.x, 1e-6f);
    CHECK_NEAR(firstEnd.y, secondEnd.y, 1e-6f);
    CHECK_EQ(firstAttacks, secondAttacks);
    CHECK(firstAttacks > 0);
}

VF_TEST(theWitherIsCalledOutOfASoulSandPatternInEitherOrientation) {
    Vec3i base{4, 10, -3};

    World alongX = witherAltar(base, true);
    bool orientation = false;
    CHECK(witherPatternAt(alongX, base, true));
    CHECK(!witherPatternAt(alongX, base, false));
    CHECK(findWitherPattern(alongX, base, orientation));
    CHECK(orientation);

    World alongZ = witherAltar(base, false);
    CHECK(findWitherPattern(alongZ, base, orientation));
    CHECK(!orientation);

    WitherFight fight(11u);
    CHECK(fight.state().phase == BossPhase::Dormant);
    CHECK(!fight.state().bar.visible);
    CHECK(fight.summonFromWorld(alongX, base));
    CHECK(fight.state().phase == BossPhase::Summoning);
    CHECK(fight.state().bar.visible);
    CHECK(fight.state().bar.style == BossBarStyle::Notched6);
    CHECK_NEAR(fight.state().position.y, static_cast<float>(base.y + 1), 1e-6f);

    CHECK(alongX.blockAt(base.x, base.y, base.z) == Block::Air);
    CHECK(alongX.blockAt(base.x, base.y + 2, base.z) == Block::Air);
    WitherFight second(12u);
    CHECK(!second.summonFromWorld(alongX, base));
    CHECK(second.state().phase == BossPhase::Dormant);

    World broken = witherAltar(base, true);
    broken.setBlock(base.x, base.y + 2, base.z, Block::Stone);
    CHECK(!findWitherPattern(broken, base, orientation));
}

VF_TEST(theWitherIsUntouchableWhileItChargesAndBlastsOnArrival) {
    WitherFight fight(5u, quickSummon(6));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{4.0f, 64.0f, 0.0f}, 20.0f, true});
    targets.push_back(WitherTarget{Vec3{40.0f, 64.0f, 0.0f}, 20.0f, true});

    std::vector<WitherHit> heavy;
    heavy.push_back(WitherHit{100.0f, false});

    for (int step = 0; step < 5; ++step) {
        BossTickReport report = fight.tick(heavy, targets);
        CHECK(report.phase == BossPhase::Summoning);
        CHECK_NEAR(report.bossHealth, fight.settings().maxHealth, 1e-4f);
    }

    BossTickReport blast = fight.tick(heavy, targets);
    CHECK(blast.phase == BossPhase::Explosive);
    CHECK(blast.phaseChanged);
    CHECK_NEAR(blast.damageToTargets, fight.settings().summonBlastDamage, 1e-4f);
    CHECK_NEAR(targets[0].health, 20.0f - fight.settings().summonBlastDamage, 1e-4f);
    CHECK_NEAR(targets[1].health, 20.0f, 1e-4f);

    BossTickReport settle = fight.tick(std::vector<WitherHit>{}, targets);
    CHECK(settle.phase == BossPhase::Ranged);
}

VF_TEST(theWitherArmoursUpExactlyAtHalfHealth) {
    WitherFight fight(21u, quickSummon(3));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{10.0f, 64.0f, 0.0f}, 200.0f, true});
    runUpToRangedPhase(fight, targets);
    CHECK(fight.state().phase == BossPhase::Ranged);
    CHECK(!fight.armored());

    float maxHealth = fight.settings().maxHealth;
    std::vector<WitherHit> nearlyHalf;
    nearlyHalf.push_back(WitherHit{maxHealth * 0.5f - 1.0f, false});
    BossTickReport stillBare = fight.tick(nearlyHalf, targets);
    CHECK(stillBare.phase == BossPhase::Ranged);
    CHECK_NEAR(stillBare.bossHealth, maxHealth * 0.5f + 1.0f, 1e-4f);
    CHECK(!fight.armored());

    std::vector<WitherHit> theLastPoint;
    theLastPoint.push_back(WitherHit{1.0f, false});
    BossTickReport onTheLine = fight.tick(theLastPoint, targets);
    CHECK_NEAR(onTheLine.bossHealth, maxHealth * 0.5f, 1e-4f);
    CHECK(onTheLine.phase == BossPhase::Armored);
    CHECK(onTheLine.phaseChanged);
    CHECK(fight.armored());
}

VF_TEST(armourStopsProjectilesButNotABladeAndTheWitherRegenerates) {
    WitherFight fight(33u, quickSummon(3));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{10.0f, 64.0f, 0.0f}, 400.0f, true});
    runUpToRangedPhase(fight, targets);

    std::vector<WitherHit> half;
    half.push_back(WitherHit{fight.settings().maxHealth * 0.5f, false});
    fight.tick(half, targets);
    CHECK(fight.armored());

    float before = fight.state().health;
    std::vector<WitherHit> arrows;
    arrows.push_back(WitherHit{60.0f, true});
    BossTickReport shrugged = fight.tick(arrows, targets);
    CHECK(shrugged.bossHealth >= before);
    CHECK(shrugged.healedThisTick > 0.0f);

    before = fight.state().health;
    std::vector<WitherHit> blade;
    blade.push_back(WitherHit{60.0f, false});
    BossTickReport cut = fight.tick(blade, targets);
    CHECK(cut.bossHealth < before - 50.0f);
    CHECK(fight.armored());
}

VF_TEST(theArmouredWitherCallsMinionsUpToItsLimitAndKeepsShooting) {
    WitherFight fight(44u, quickSummon(3));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{12.0f, 64.0f, 0.0f}, 100000.0f, true});
    runUpToRangedPhase(fight, targets);

    std::vector<WitherHit> half;
    half.push_back(WitherHit{fight.settings().maxHealth * 0.5f, false});
    fight.tick(half, targets);
    CHECK(fight.armored());

    std::vector<WitherHit> nothing;
    float targetHealthBefore = targets[0].health;
    for (int step = 0; step < 600; ++step) fight.tick(nothing, targets);

    CHECK_EQ(fight.aliveMinionCount(), fight.settings().maxMinions);
    CHECK_EQ(fight.state().minionsSummoned, fight.settings().maxMinions);
    CHECK(targets[0].health < targetHealthBefore);
    CHECK(fight.state().attacksLaunched > 10);
    for (const BossMinion& minion : fight.minions()) {
        CHECK(minion.alive);
        CHECK(behaviorHorizontalDistance(minion.position, fight.state().position) > 0.0f);
    }
}

VF_TEST(theWitherAlwaysSwitchesToTheNearestLivingTarget) {
    WitherFight fight(55u, quickSummon(3));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{60.0f, 64.0f, 0.0f}, 500.0f, true});
    targets.push_back(WitherTarget{Vec3{12.0f, 64.0f, 0.0f}, 500.0f, true});
    targets.push_back(WitherTarget{Vec3{90.0f, 64.0f, 0.0f}, 500.0f, true});
    runUpToRangedPhase(fight, targets);

    std::vector<WitherHit> nothing;
    fight.tick(nothing, targets);
    CHECK_EQ(fight.state().targetIndex, 1);

    targets[1].alive = false;
    fight.tick(nothing, targets);
    CHECK_EQ(fight.state().targetIndex, 0);

    targets[0].alive = false;
    targets[2].alive = false;
    BossTickReport alone = fight.tick(nothing, targets);
    CHECK_EQ(fight.state().targetIndex, -1);
    CHECK_NEAR(alone.damageToTargets, 0.0f, 1e-6f);
}

VF_TEST(theWitherDiesWhenItsHealthRunsOutAndStopsTicking) {
    WitherFight fight(66u, quickSummon(3));
    fight.begin(Vec3{0.0f, 64.0f, 0.0f});

    std::vector<WitherTarget> targets;
    targets.push_back(WitherTarget{Vec3{10.0f, 64.0f, 0.0f}, 500.0f, true});
    runUpToRangedPhase(fight, targets);

    std::vector<WitherHit> execution;
    execution.push_back(WitherHit{fight.settings().maxHealth, false});
    BossTickReport killed = fight.tick(execution, targets);

    CHECK(killed.defeated);
    CHECK(killed.phase == BossPhase::Defeated);
    CHECK(!fight.state().alive);
    CHECK(!fight.state().bar.visible);
    CHECK_NEAR(fight.state().health, 0.0f, 1e-6f);

    int attacksAtDeath = fight.state().attacksLaunched;
    float targetHealth = targets[0].health;
    for (int step = 0; step < 50; ++step) fight.tick(execution, targets);
    CHECK_EQ(fight.state().attacksLaunched, attacksAtDeath);
    CHECK_NEAR(targets[0].health, targetHealth, 1e-6f);
}

VF_TEST(twoWitherFightsOnTheSameSeedAndScriptRunIdentically) {
    auto playOut = [](std::vector<float>& healthTrack, std::vector<float>& targetTrack,
                      std::vector<Vec3>& minionSpots) {
        WitherFight fight(0xbeefu, quickSummon(4));
        fight.begin(Vec3{0.0f, 64.0f, 0.0f});

        std::vector<WitherTarget> targets;
        targets.push_back(WitherTarget{Vec3{14.0f, 64.0f, 0.0f}, 100000.0f, true});
        targets.push_back(WitherTarget{Vec3{-30.0f, 64.0f, 8.0f}, 100000.0f, true});

        for (int step = 0; step < 500; ++step) {
            std::vector<WitherHit> incoming;
            if (step % 7 == 0) incoming.push_back(WitherHit{2.5f, step % 14 == 0});
            BossTickReport report = fight.tick(incoming, targets);
            healthTrack.push_back(report.bossHealth);
            targetTrack.push_back(targets[0].health);
        }
        for (const BossMinion& minion : fight.minions()) minionSpots.push_back(minion.position);
    };

    std::vector<float> firstHealth;
    std::vector<float> firstTargets;
    std::vector<Vec3> firstMinions;
    std::vector<float> secondHealth;
    std::vector<float> secondTargets;
    std::vector<Vec3> secondMinions;
    playOut(firstHealth, firstTargets, firstMinions);
    playOut(secondHealth, secondTargets, secondMinions);

    CHECK_EQ(firstHealth.size(), secondHealth.size());
    for (std::size_t i = 0; i < firstHealth.size() && i < secondHealth.size(); ++i) {
        CHECK_NEAR(firstHealth[i], secondHealth[i], 1e-6f);
        CHECK_NEAR(firstTargets[i], secondTargets[i], 1e-6f);
    }
    CHECK(!firstMinions.empty());
    CHECK_EQ(firstMinions.size(), secondMinions.size());
    for (std::size_t i = 0; i < firstMinions.size() && i < secondMinions.size(); ++i) {
        CHECK_NEAR(firstMinions[i].x, secondMinions[i].x, 1e-6f);
        CHECK_NEAR(firstMinions[i].z, secondMinions[i].z, 1e-6f);
    }
}

VF_TEST(bothBossesAreDrivenByAValidBehaviourTree) {
    EndGenerator end;
    DragonFight dragon(end, 1u);
    CHECK(dragon.brain().structureValid());
    CHECK(dragon.brain().stats().nodeCount > 10);
    CHECK(dragon.brain().stats().depth <= maxBehaviorDepth);
    CHECK_EQ(dragon.brain().root()->name(), std::string("dragonRoot"));

    WitherFight wither(2u);
    CHECK(wither.brain().structureValid());
    CHECK(wither.brain().stats().nodeCount > 10);
    CHECK(wither.brain().stats().depth <= maxBehaviorDepth);
    CHECK_EQ(wither.brain().root()->name(), std::string("witherRoot"));

    dragon.tick(clingingTo(dragon, 0.0f));
    CHECK_EQ(dragon.context().tick, 1);
    CHECK_EQ(dragon.brain().tickCount(), 1);
}
