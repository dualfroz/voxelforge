#include "testHarness.h"
#include "vf/Selection.h"

#include <algorithm>
#include <vector>

using namespace vf;

namespace {

World slabWorld(int spanX, int spanZ, int surfaceY, Block block) {
    World world;
    for (int z = 0; z < spanZ; ++z)
        for (int x = 0; x < spanX; ++x) world.setBlock(x, surfaceY, z, block);
    return world;
}

std::size_t cellsInsideBox(const Selection& selection, const Vec3i& lo, const Vec3i& hi) {
    std::size_t inside = 0;
    selection.forEach([&](const Vec3i& cell) {
        if (cell.x >= lo.x && cell.x <= hi.x && cell.y >= lo.y && cell.y <= hi.y &&
            cell.z >= lo.z && cell.z <= hi.z)
            ++inside;
    });
    return inside;
}

} // namespace

VF_TEST(boxSelectionCoversEveryCellBetweenTheTwoCorners) {
    Selection box = Selection::box({4, 1, -2}, {1, 3, 0});
    CHECK(box.shape() == SelectionShape::Box);
    CHECK(box.min() == Vec3i(1, 1, -2));
    CHECK(box.max() == Vec3i(4, 3, 0));
    CHECK(box.dims() == Vec3i(4, 3, 3));
    CHECK_EQ(box.count(), 36u);
    CHECK(box.contains({1, 1, -2}));
    CHECK(box.contains({4, 3, 0}));
    CHECK(!box.contains({5, 3, 0}));
    CHECK(!box.contains({1, 0, 0}));
}

VF_TEST(sphereSelectionIsSymmetricAroundItsCentreAndFitsItsBoundingBox) {
    Selection sphere = Selection::sphere({10, 10, 10}, 4);
    CHECK(sphere.shape() == SelectionShape::Sphere);
    CHECK(sphere.dims() == Vec3i(9, 9, 9));
    CHECK(sphere.contains({10, 10, 10}));
    CHECK(sphere.contains({14, 10, 10}));
    CHECK(!sphere.contains({14, 14, 10}));
    CHECK(sphere.count() < 9u * 9u * 9u);
    CHECK(sphere.count() > 200u);

    sphere.forEach([&](const Vec3i& cell) {
        Vec3i mirrored{20 - cell.x, 20 - cell.y, 20 - cell.z};
        CHECK(sphere.contains(mirrored));
    });
}

VF_TEST(ellipsoidSelectionRespectsAPerAxisRadius) {
    Selection blob = Selection::ellipsoid({0, 0, 0}, {6, 1, 2});
    CHECK(blob.dims() == Vec3i(13, 3, 5));
    CHECK(blob.contains({6, 0, 0}));
    CHECK(blob.contains({0, 1, 0}));
    CHECK(blob.contains({0, 0, 2}));
    CHECK(!blob.contains({6, 1, 2}));
    CHECK(!blob.contains({0, 2, 0}));
}

VF_TEST(cylinderSelectionRepeatsTheSameDiscOnEveryLayer) {
    Selection tube = Selection::cylinder({0, 5, 0}, 3, 4);
    CHECK(tube.min() == Vec3i(-3, 5, -3));
    CHECK(tube.max() == Vec3i(3, 8, 3));

    std::size_t perLayer = 0;
    for (int z = -3; z <= 3; ++z)
        for (int x = -3; x <= 3; ++x)
            if (tube.contains({x, 5, z})) ++perLayer;
    CHECK(perLayer > 0u);
    CHECK_EQ(tube.count(), perLayer * 4u);
    for (int y = 5; y <= 8; ++y) {
        std::size_t layer = 0;
        for (int z = -3; z <= 3; ++z)
            for (int x = -3; x <= 3; ++x)
                if (tube.contains({x, y, z})) ++layer;
        CHECK_EQ(layer, perLayer);
    }
    CHECK(!tube.contains({0, 9, 0}));
}

VF_TEST(hollowBoxKeepsOnlyTheWallsOfTheRequestedThickness) {
    Selection walls = Selection::hollowBox({0, 0, 0}, {4, 4, 4}, 1);
    CHECK_EQ(walls.count(), 125u - 27u);
    CHECK(walls.contains({0, 0, 0}));
    CHECK(walls.contains({4, 2, 2}));
    CHECK(!walls.contains({2, 2, 2}));

    Selection thick = Selection::hollowBox({0, 0, 0}, {6, 6, 6}, 2);
    CHECK_EQ(thick.count(), 343u - 27u);
    CHECK(thick.contains({1, 1, 1}));
    CHECK(!thick.contains({3, 3, 3}));
}

VF_TEST(maskUnionCollectsCellsFromBothOperandsWithoutDoubleCounting) {
    Selection left = Selection::box({0, 0, 0}, {3, 0, 0});
    Selection right = Selection::box({2, 0, 0}, {5, 0, 0});
    Selection merged = left.unionWith(right);
    CHECK_EQ(merged.count(), 6u);
    CHECK(merged.min() == Vec3i(0, 0, 0));
    CHECK(merged.max() == Vec3i(5, 0, 0));
    for (int x = 0; x <= 5; ++x) CHECK(merged.contains({x, 0, 0}));
    CHECK(merged.shape() == SelectionShape::Mask);
}

VF_TEST(maskIntersectionKeepsOnlyTheOverlapAndIsEmptyWhenBoxesMiss) {
    Selection left = Selection::box({0, 0, 0}, {3, 3, 3});
    Selection right = Selection::box({2, 2, 2}, {5, 5, 5});
    Selection shared = left.intersectWith(right);
    CHECK_EQ(shared.count(), 8u);
    CHECK(shared.min() == Vec3i(2, 2, 2));
    CHECK(shared.max() == Vec3i(3, 3, 3));

    Selection apart = Selection::box({20, 20, 20}, {21, 21, 21});
    CHECK(left.intersectWith(apart).empty());
    CHECK_EQ(left.intersectWith(apart).count(), 0u);
}

VF_TEST(maskDifferenceRemovesEveryCellOfTheSecondOperand) {
    Selection block = Selection::box({0, 0, 0}, {3, 3, 3});
    Selection bite = Selection::box({0, 0, 0}, {1, 1, 1});
    Selection carved = block.differenceWith(bite);
    CHECK_EQ(carved.count(), 64u - 8u);
    CHECK(!carved.contains({0, 0, 0}));
    CHECK(carved.contains({2, 0, 0}));
    CHECK(carved.min() == Vec3i(0, 0, 0));
}

VF_TEST(maskNegationFlipsEveryCellInsideTheBoundingBox) {
    Selection sphere = Selection::sphere({0, 0, 0}, 3);
    Selection hole = sphere.inverted();
    CHECK_EQ(sphere.count() + hole.count(), 7u * 7u * 7u);
    CHECK(hole.contains({3, 3, 3}));
    CHECK(!hole.contains({0, 0, 0}));
    CHECK(sphere.intersectWith(hole).empty());
    CHECK_EQ(sphere.unionWith(hole).count(), 7u * 7u * 7u);
}

VF_TEST(dilationGrowsOneCellIntoASixNeighbourCrossAndKeepsGoing) {
    Selection seed = Selection::box({0, 0, 0}, {0, 0, 0});
    CHECK_EQ(seed.count(), 1u);

    Selection once = seed.expanded(1);
    CHECK_EQ(once.count(), 7u);
    CHECK(once.contains({1, 0, 0}));
    CHECK(once.contains({0, -1, 0}));
    CHECK(!once.contains({1, 1, 0}));
    CHECK(once.min() == Vec3i(-1, -1, -1));

    // Manhattan ball of radius two: 1 + 6 + 18 cells.
    Selection twice = seed.expanded(2);
    CHECK_EQ(twice.count(), 25u);
    CHECK(twice.contains({1, 1, 0}));
    CHECK(twice.contains({2, 0, 0}));
    CHECK(!twice.contains({1, 1, 1}));
}

VF_TEST(erosionPeelsExactlyOneShellOffASolidBoxPerStep) {
    Selection box = Selection::box({0, 0, 0}, {4, 4, 4});
    CHECK_EQ(box.count(), 125u);

    Selection once = box.contracted(1);
    CHECK_EQ(once.count(), 27u);
    CHECK(once.contains({2, 2, 2}));
    CHECK(!once.contains({0, 2, 2}));

    Selection twice = box.contracted(2);
    CHECK_EQ(twice.count(), 1u);
    CHECK(twice.contains({2, 2, 2}));

    Selection gone = box.contracted(3);
    CHECK(gone.empty());
}

VF_TEST(expandingThenContractingASolidBoxReturnsTheSameCells) {
    Selection box = Selection::box({-2, 7, 3}, {5, 9, 8});
    Selection roundTrip = box.expanded(2).contracted(2);
    CHECK_EQ(roundTrip.count(), box.count());
    CHECK(roundTrip.mask().sameCells(box.mask()));
}

VF_TEST(outlineKeepsOnlyCellsWithANeighbourOutsideTheSelection) {
    Selection box = Selection::box({0, 0, 0}, {4, 4, 4});
    Selection shell = box.outline();
    CHECK_EQ(shell.count(), 125u - 27u);
    CHECK(shell.contains({0, 0, 0}));
    CHECK(shell.contains({2, 4, 2}));
    CHECK(!shell.contains({2, 2, 2}));

    Selection single = Selection::box({9, 9, 9}, {9, 9, 9});
    CHECK_EQ(single.outline().count(), 1u);
}

VF_TEST(shiftingASelectionMovesEveryCellAndKeepsItsShape) {
    Selection sphere = Selection::sphere({0, 0, 0}, 3);
    Selection moved = sphere.shifted({100, -50, 7});
    CHECK_EQ(moved.count(), sphere.count());
    CHECK(moved.min() == Vec3i(97, -53, 4));
    sphere.forEach([&](const Vec3i& cell) {
        CHECK(moved.contains({cell.x + 100, cell.y - 50, cell.z + 7}));
    });
}

VF_TEST(magicWandFollowsConnectedCellsOfTheSameBlockOnly) {
    World world;
    for (int z = 0; z < 3; ++z)
        for (int x = 0; x < 3; ++x) world.setBlock(x, 0, z, Block::Stone);
    world.setBlock(1, 0, 1, Block::Dirt);
    world.setBlock(10, 0, 10, Block::Stone);

    Selection wand = Selection::magicWand(world, {0, 0, 0}, 1000u);
    CHECK(wand.shape() == SelectionShape::Wand);
    CHECK_EQ(wand.count(), 8u);
    CHECK(!wand.truncated());
    CHECK(!wand.contains({1, 0, 1}));
    CHECK(!wand.contains({10, 0, 10}));
    CHECK(wand.contains({2, 0, 2}));

    Selection islandWand = Selection::magicWand(world, {10, 0, 10}, 1000u);
    CHECK_EQ(islandWand.count(), 1u);
}

VF_TEST(magicWandStopsAtTheCellLimitAndSaysSo) {
    World world = slabWorld(40, 40, 0, Block::Stone);
    Selection capped = Selection::magicWand(world, {0, 0, 0}, 50u);
    CHECK_EQ(capped.count(), 50u);
    CHECK(capped.truncated());

    Selection whole = Selection::magicWand(world, {0, 0, 0}, 40u * 40u);
    CHECK_EQ(whole.count(), 1600u);
    CHECK(!whole.truncated());
}

VF_TEST(magicWandStaysInsideTheBoundingLimitItWasGiven) {
    World world = slabWorld(20, 20, 0, Block::Stone);
    Selection boxed =
        Selection::magicWandWithin(world, {5, 0, 5}, 10000u, {2, 0, 2}, {6, 0, 6});
    CHECK_EQ(boxed.count(), 25u);
    CHECK(boxed.min() == Vec3i(2, 0, 2));
    CHECK(boxed.max() == Vec3i(6, 0, 6));
    CHECK(!boxed.contains({7, 0, 5}));
    CHECK_EQ(cellsInsideBox(boxed, {2, 0, 2}, {6, 0, 6}), boxed.count());
}

VF_TEST(selectionStatsReportCountsDimensionsAndABlockHistogram) {
    World world;
    for (int z = 0; z < 4; ++z)
        for (int x = 0; x < 4; ++x) {
            world.setBlock(x, 0, z, Block::Stone);
            world.setBlock(x, 1, z, Block::Dirt);
        }
    world.setBlock(0, 1, 0, Block::Grass);

    Selection region = Selection::box({0, 0, 0}, {3, 2, 3});
    SelectionStats summary = region.stats(world);
    CHECK_EQ(summary.blockCount, 48u);
    CHECK(summary.dims == Vec3i(4, 3, 4));
    CHECK_EQ(summary.boundingVolume, 48);
    CHECK_EQ(summary.countOf(Block::Stone), 16u);
    CHECK_EQ(summary.countOf(Block::Dirt), 15u);
    CHECK_EQ(summary.countOf(Block::Grass), 1u);
    CHECK_EQ(summary.countOf(Block::Air), 16u);
    CHECK_EQ(summary.solidCount, 32u);
    CHECK_EQ(summary.airCount, 16u);
    CHECK(summary.dominantBlock() == Block::Stone);
    CHECK_EQ(summary.distinctBlocks(), 4u);
}

VF_TEST(anEmptySelectionHasNoCellsAndNoStatistics) {
    Selection nothing;
    CHECK(nothing.empty());
    CHECK_EQ(nothing.count(), 0u);
    CHECK_EQ(nothing.blocks().size(), 0u);

    World world;
    SelectionStats summary = nothing.stats(world);
    CHECK_EQ(summary.blockCount, 0u);
    CHECK_EQ(summary.boundingVolume, 0);
    CHECK_EQ(summary.distinctBlocks(), 0u);
    CHECK(summary.dominantBlock() == Block::Air);

    CHECK_EQ(nothing.expanded(3).count(), 0u);
    CHECK_EQ(nothing.outline().count(), 0u);
    CHECK(nothing.unionWith(Selection::box({0, 0, 0}, {1, 1, 1})).count() == 8u);
}

VF_TEST(setOperationsComposeIntoARingShapedSelection) {
    Selection outer = Selection::sphere({0, 0, 0}, 5);
    Selection inner = Selection::sphere({0, 0, 0}, 3);
    Selection ring = outer.differenceWith(inner);
    CHECK_EQ(ring.count(), outer.count() - inner.count());
    CHECK(ring.contains({5, 0, 0}));
    CHECK(!ring.contains({0, 0, 0}));

    Selection cut = Selection::box({-10, 1, -10}, {10, 10, 10});
    Selection lowerRing = ring.differenceWith(cut);
    lowerRing.forEach([&](const Vec3i& cell) { CHECK(cell.y <= 0); });
    CHECK(lowerRing.count() < ring.count());
    CHECK(lowerRing.count() > 0u);
}

VF_TEST(trimmingAMaskShrinksTheBoundingBoxWithoutLosingCells) {
    SelectionMask sparse(Vec3i{0, 0, 0}, Vec3i{16, 16, 16});
    sparse.add({4, 5, 6});
    sparse.add({7, 5, 6});
    CHECK_EQ(sparse.cellCount(), 4096u);

    SelectionMask tight = sparse.trimmed();
    CHECK_EQ(tight.count(), 2u);
    CHECK(tight.min() == Vec3i(4, 5, 6));
    CHECK(tight.max() == Vec3i(7, 5, 6));
    CHECK_EQ(tight.cellCount(), 4u);
    CHECK(tight.sameCells(sparse));
}
