#include "testHarness.h"
#include "vf/Pathfind.h"

using namespace vf;

namespace {

void floor(World& world, int y, int x0, int z0, int x1, int z1) {
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z)
            world.setBlock(x, y, z, Block::Stone);
}

void pillar(World& world, int x, int z, int y0, int y1) {
    for (int y = y0; y <= y1; ++y) world.setBlock(x, y, z, Block::Stone);
}

bool contiguous(const std::vector<Vec3i>& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        int dx = std::abs(path[i].x - path[i - 1].x);
        int dz = std::abs(path[i].z - path[i - 1].z);
        if (dx + dz != 1) return false;   // exactly one horizontal step apart
    }
    return true;
}

} // namespace

VF_TEST(pathStraightLineAcrossOpenFloor) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    std::vector<Vec3i> path = findPath(world, {0, 1, 0}, {5, 1, 0});
    CHECK_EQ(path.size(), static_cast<std::size_t>(6));   // 5 steps + start
    CHECK(path.front() == Vec3i(0, 1, 0));
    CHECK(path.back() == Vec3i(5, 1, 0));
    CHECK(contiguous(path));
}

VF_TEST(pathRoutesAroundWallThroughGap) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    for (int z = 0; z <= 6; ++z) pillar(world, 4, z, 1, 2);   // wall x=4, gap at z=7,8
    std::vector<Vec3i> path = findPath(world, {1, 1, 1}, {7, 1, 1});
    CHECK(!path.empty());
    CHECK(path.back() == Vec3i(7, 1, 1));
    CHECK(contiguous(path));
    CHECK(path.size() > 7);   // longer than the blocked straight line
    for (const Vec3i& cell : path)
        CHECK(!world.solidAt(cell.x, cell.y, cell.z));   // never inside the wall
}

VF_TEST(pathReturnsEmptyWhenGoalSealed) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    pillar(world, 4, 5, 1, 2);
    pillar(world, 6, 5, 1, 2);
    pillar(world, 5, 4, 1, 2);
    pillar(world, 5, 6, 1, 2);   // seal all four neighbours of {5,1,5}
    std::vector<Vec3i> path = findPath(world, {1, 1, 1}, {5, 1, 5});
    CHECK(path.empty());
}

VF_TEST(pathFoundWhenSealBreached) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    pillar(world, 4, 5, 1, 2);
    pillar(world, 6, 5, 1, 2);
    pillar(world, 5, 4, 1, 2);
    // {5,1,6} left open -> reachable
    std::vector<Vec3i> path = findPath(world, {1, 1, 1}, {5, 1, 5});
    CHECK(!path.empty());
    CHECK(path.back() == Vec3i(5, 1, 5));
}

VF_TEST(pathClimbsSingleStep) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    floor(world, 1, 4, 0, 8, 8);   // raised terrace from x=4
    std::vector<Vec3i> path = findPath(world, {0, 1, 0}, {6, 2, 0});
    CHECK(!path.empty());
    CHECK(path.front() == Vec3i(0, 1, 0));
    CHECK(path.back() == Vec3i(6, 2, 0));
    CHECK(contiguous(path));
}

VF_TEST(pathTwoHighWallIsImpassable) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    for (int z = 0; z <= 8; ++z) pillar(world, 4, z, 1, 3);   // full 3-high wall, no gap
    std::vector<Vec3i> path = findPath(world, {1, 1, 1}, {7, 1, 1});
    CHECK(path.empty());
}

VF_TEST(pathIsDeterministic) {
    World world;
    floor(world, 0, 0, 0, 8, 8);
    for (int z = 0; z <= 6; ++z) pillar(world, 4, z, 1, 2);
    std::vector<Vec3i> a = findPath(world, {1, 1, 1}, {7, 1, 1});
    std::vector<Vec3i> b = findPath(world, {1, 1, 1}, {7, 1, 1});
    CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) CHECK(a[i] == b[i]);
}
