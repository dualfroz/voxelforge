#include "testHarness.h"
#include "vf/Streaming.h"

#include <algorithm>

using namespace vf;

namespace {

long long dist2(ColumnCoord a, ColumnCoord b) {
    long long dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

bool listContains(const std::vector<ColumnCoord>& list, ColumnCoord column) {
    return std::find(list.begin(), list.end(), column) != list.end();
}

}

VF_TEST(streamRadiusIsDiscSortedNearestFirst) {
    auto columns = columnsInRadius({0, 0}, 4);
    CHECK(!columns.empty());
    CHECK(columns.front() == (ColumnCoord{0, 0}));
    for (ColumnCoord c : columns) CHECK(dist2(c, {0, 0}) <= 16);
    CHECK(!listContains(columns, {3, 3}));
    for (std::size_t i = 1; i < columns.size(); ++i)
        CHECK(dist2(columns[i - 1], {0, 0}) <= dist2(columns[i], {0, 0}));
}

VF_TEST(streamInitialUpdateLoadsWholeDisc) {
    ChunkStreamer streamer({4, 6});
    StreamDelta delta = streamer.update({0, 0});
    CHECK_EQ(delta.load.size(), columnsInRadius({0, 0}, 4).size());
    CHECK(delta.unload.empty());
    CHECK_EQ(streamer.loadedCount(), delta.load.size());
    CHECK(delta.load.front() == (ColumnCoord{0, 0}));
    for (std::size_t i = 1; i < delta.load.size(); ++i)
        CHECK(dist2(delta.load[i - 1], {0, 0}) <= dist2(delta.load[i], {0, 0}));
}

VF_TEST(streamRepeatedUpdateIsIdempotent) {
    ChunkStreamer streamer({4, 6});
    streamer.update({0, 0});
    StreamDelta again = streamer.update({0, 0});
    CHECK(again.load.empty());
    CHECK(again.unload.empty());
}

VF_TEST(streamStepKeepsOverlapResident) {
    ChunkStreamer streamer({4, 6});
    streamer.update({0, 0});
    StreamDelta delta = streamer.update({1, 0});
    CHECK(streamer.isLoaded({0, 0}));
    CHECK(!listContains(delta.load, ColumnCoord{0, 0}));
    CHECK(!listContains(delta.unload, ColumnCoord{0, 0}));
}

VF_TEST(streamHysteresisAvoidsThrash) {
    ChunkStreamer streamer({4, 6});
    streamer.update({0, 0});
    streamer.update({5, 0});
    CHECK(streamer.isLoaded({0, 0}));

    StreamDelta far = streamer.update({20, 0});
    CHECK(!streamer.isLoaded({0, 0}));
    CHECK(listContains(far.unload, ColumnCoord{0, 0}));
}
