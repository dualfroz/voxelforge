#include "testHarness.h"
#include "vf/ChunkCache.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace vf;

namespace {

Chunk markedChunk(int marker) {
    Chunk chunk;
    chunk.set(0, 0, 0, Block::Stone);
    chunk.set(1, 0, 0, static_cast<Block>(marker));
    return chunk;
}

ChunkMesh meshWithQuads(int quads, float marker) {
    ChunkMesh mesh;
    for (int quad = 0; quad < quads; ++quad) {
        std::uint32_t start = static_cast<std::uint32_t>(mesh.opaque.vertices.size());
        for (int corner = 0; corner < 4; ++corner) {
            Vertex vertex;
            vertex.px = marker;
            vertex.py = static_cast<float>(quad);
            vertex.pz = static_cast<float>(corner);
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
            vertex.u = corner == 1 || corner == 2 ? 1.0f : 0.0f;
            vertex.v = corner >= 2 ? 1.0f : 0.0f;
            vertex.tile = marker;
            vertex.light = 1.0f;
            vertex.ao = 1.0f;
            mesh.opaque.vertices.push_back(vertex);
        }
        const std::uint32_t triangles[6] = {start, start + 1, start + 2,
                                            start, start + 2, start + 3};
        for (std::uint32_t index : triangles) mesh.opaque.indices.push_back(index);
    }
    return mesh;
}

std::uint64_t sizeOfEntry(int quads) {
    return ChunkCache::entrySizeBytes(markedChunk(1), meshWithQuads(quads, 0.0f));
}

void storeMarked(ChunkCache& cache, const Vec3i& coord, int quads) {
    cache.store(coord, markedChunk(coord.x), meshWithQuads(quads, static_cast<float>(coord.x)));
}

} // namespace

VF_TEST(entrySizeIsMeasuredFromTheRealPayload) {
    std::uint64_t blocks = static_cast<std::uint64_t>(chunkVolume) * sizeof(Block);
    std::uint64_t perQuad = 4 * sizeof(Vertex) + 6 * sizeof(std::uint32_t);
    CHECK_EQ(sizeOfEntry(0), blocks);
    CHECK_EQ(sizeOfEntry(10), blocks + 10 * perQuad);

    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 10);
    CHECK_EQ(cache.bytesUsed(), sizeOfEntry(10));
    storeMarked(cache, {1, 0, 0}, 4);
    CHECK_EQ(cache.bytesUsed(), sizeOfEntry(10) + sizeOfEntry(4));
    CHECK_EQ(cache.size(), static_cast<std::size_t>(2));
}

VF_TEST(hardByteBudgetEvictsUntilItFits) {
    std::uint64_t entry = sizeOfEntry(10);
    ChunkCache cache(entry * 3, CachePolicy::Lru);
    for (int i = 0; i < 4; ++i) storeMarked(cache, {i, 0, 0}, 10);

    CHECK_EQ(cache.size(), static_cast<std::size_t>(3));
    CHECK_EQ(cache.bytesUsed(), entry * 3);
    CHECK(cache.bytesUsed() <= cache.byteBudget());
    CHECK_EQ(cache.stats().evictions, static_cast<std::uint64_t>(1));
    CHECK(!cache.contains({0, 0, 0}));
    CHECK(cache.contains({3, 0, 0}));
}

VF_TEST(lruEvictsTheLeastRecentlyUsedEntry) {
    std::uint64_t entry = sizeOfEntry(6);
    ChunkCache cache(entry * 2, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 6);
    storeMarked(cache, {1, 0, 0}, 6);

    Chunk fetched;
    CHECK(cache.fetchChunk({0, 0, 0}, fetched));   // refreshes the older entry
    storeMarked(cache, {2, 0, 0}, 6);

    CHECK_EQ(cache.size(), static_cast<std::size_t>(2));
    CHECK(cache.contains({0, 0, 0}));
    CHECK(!cache.contains({1, 0, 0}));
    CHECK(cache.contains({2, 0, 0}));
    CHECK_EQ(cache.stats().evictions, static_cast<std::uint64_t>(1));
}

VF_TEST(fetchMovesAnEntryToTheBackOfTheEvictionQueue) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    for (int i = 0; i < 3; ++i) storeMarked(cache, {i, 0, 0}, 2);

    std::vector<Vec3i> order = cache.evictionOrder();
    CHECK_EQ(order.size(), static_cast<std::size_t>(3));
    CHECK(order[0] == (Vec3i{0, 0, 0}));
    CHECK(order[2] == (Vec3i{2, 0, 0}));

    Chunk fetched;
    CHECK(cache.fetchChunk({0, 0, 0}, fetched));
    order = cache.evictionOrder();
    CHECK(order[0] == (Vec3i{1, 0, 0}));
    CHECK(order[2] == (Vec3i{0, 0, 0}));
}

VF_TEST(pinnedEntryIsNeverEvicted) {
    std::uint64_t entry = sizeOfEntry(6);
    ChunkCache cache(entry, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 6);
    cache.pin({0, 0, 0});
    CHECK(cache.isPinned({0, 0, 0}));

    for (int i = 1; i <= 5; ++i) storeMarked(cache, {i, 0, 0}, 6);

    CHECK(cache.contains({0, 0, 0}));
    CHECK_EQ(cache.size(), static_cast<std::size_t>(1));
    std::vector<Vec3i> order = cache.evictionOrder();
    CHECK(order.empty());                          // nothing left that may be dropped
}

VF_TEST(unpinningMakesAnEntryEvictableAgain) {
    std::uint64_t entry = sizeOfEntry(6);
    ChunkCache cache(entry, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 6);
    cache.pin({0, 0, 0});
    storeMarked(cache, {1, 0, 0}, 6);
    CHECK(cache.contains({0, 0, 0}));
    CHECK(!cache.contains({1, 0, 0}));

    cache.unpin({0, 0, 0});
    CHECK(!cache.isPinned({0, 0, 0}));
    storeMarked(cache, {1, 0, 0}, 6);
    CHECK(!cache.contains({0, 0, 0}));
    CHECK(cache.contains({1, 0, 0}));
}

VF_TEST(distanceWeightedPolicyDropsFarChunksFirst) {
    const Vec3i nearby{0, 0, 0};
    const Vec3i distant{20, 0, 0};

    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    cache.setObserver({0, 0, 0});
    storeMarked(cache, nearby, 4);
    storeMarked(cache, distant, 4);
    CHECK(cache.evictionOrder().front() == nearby);   // pure recency picks the older one

    cache.setPolicy(CachePolicy::DistanceWeighted);
    CHECK(cache.evictionOrder().front() == distant);

    std::uint64_t entry = sizeOfEntry(4);
    ChunkCache tight(entry, CachePolicy::DistanceWeighted);
    tight.setObserver({0, 0, 0});
    storeMarked(tight, nearby, 4);
    storeMarked(tight, distant, 4);
    CHECK(tight.contains(nearby));
    CHECK(!tight.contains(distant));

    ChunkCache recencyOnly(entry, CachePolicy::Lru);
    recencyOnly.setObserver({0, 0, 0});
    storeMarked(recencyOnly, nearby, 4);
    storeMarked(recencyOnly, distant, 4);
    CHECK(!recencyOnly.contains(nearby));
    CHECK(recencyOnly.contains(distant));
}

VF_TEST(observerMoveRescoresResidentEntries) {
    ChunkCache cache(1ull << 30, CachePolicy::DistanceWeighted);
    cache.setObserver({0, 0, 0});
    storeMarked(cache, {0, 0, 0}, 4);
    storeMarked(cache, {8, 0, 0}, 4);
    CHECK(cache.evictionOrder().front() == (Vec3i{8, 0, 0}));

    cache.setObserver({8, 0, 0});
    CHECK(cache.observer() == (Vec3i{8, 0, 0}));
    CHECK(cache.evictionOrder().front() == (Vec3i{0, 0, 0}));
}

VF_TEST(blockEditDropsTheOwnerAndTheMeshesAcrossThatBorder) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    const Vec3i neighbours[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    storeMarked(cache, {0, 0, 0}, 8);
    for (const Vec3i& coord : neighbours) storeMarked(cache, coord, 8);
    std::uint64_t before = cache.bytesUsed();

    std::size_t dropped = cache.invalidateBlock(chunkSize - 1, 5, 5);
    CHECK_EQ(dropped, static_cast<std::size_t>(1));
    CHECK(!cache.contains({0, 0, 0}));
    CHECK(cache.contains({1, 0, 0}));
    CHECK(!cache.hasMesh({1, 0, 0}));            // its ambient occlusion read the edit
    CHECK(cache.hasMesh({-1, 0, 0}));
    CHECK(cache.hasMesh({0, 1, 0}));
    CHECK(cache.hasMesh({0, 0, 1}));

    CHECK_EQ(cache.bytesUsed(), before - sizeOfEntry(8) - (sizeOfEntry(8) - sizeOfEntry(0)));
    CacheStats counted = cache.stats();
    CHECK_EQ(counted.invalidations, static_cast<std::uint64_t>(1));
    CHECK_EQ(counted.meshInvalidations, static_cast<std::uint64_t>(1));
}

VF_TEST(cornerBlockEditCascadesToEverySharingNeighbour) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 3);
    for (int dx = 0; dx <= 1; ++dx)
        for (int dy = 0; dy <= 1; ++dy)
            for (int dz = 0; dz <= 1; ++dz)
                if (dx || dy || dz) storeMarked(cache, {dx, dy, dz}, 3);
    storeMarked(cache, {-1, 0, 0}, 3);

    std::size_t dropped = cache.invalidateBlock(chunkSize - 1, chunkSize - 1, chunkSize - 1);
    CHECK_EQ(dropped, static_cast<std::size_t>(7));
    CHECK(cache.hasMesh({-1, 0, 0}));            // opposite side, untouched
    CHECK(!cache.hasMesh({1, 1, 1}));
    CHECK(!cache.hasMesh({0, 0, 1}));
    CHECK_EQ(cache.stats().meshInvalidations, static_cast<std::uint64_t>(7));
}

VF_TEST(interiorBlockEditLeavesNeighbourMeshesAlone) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 5);
    storeMarked(cache, {1, 0, 0}, 5);
    storeMarked(cache, {0, 0, 1}, 5);

    std::size_t dropped = cache.invalidateBlock(5, 5, 5);
    CHECK_EQ(dropped, static_cast<std::size_t>(0));
    CHECK(!cache.contains({0, 0, 0}));
    CHECK(cache.hasMesh({1, 0, 0}));
    CHECK(cache.hasMesh({0, 0, 1}));
    CHECK_EQ(cache.stats().meshInvalidations, static_cast<std::uint64_t>(0));
}

VF_TEST(statsTrackHitsMissesAndHitRate) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 2);

    Chunk chunk;
    ChunkMesh mesh;
    CHECK(cache.fetchChunk({0, 0, 0}, chunk));
    CHECK(cache.fetchMesh({0, 0, 0}, mesh));
    CHECK(cache.fetchChunk({0, 0, 0}, chunk));
    CHECK(!cache.fetchChunk({7, 0, 0}, chunk));

    CacheStats counted = cache.stats();
    CHECK_EQ(counted.hits, static_cast<std::uint64_t>(3));
    CHECK_EQ(counted.misses, static_cast<std::uint64_t>(1));
    CHECK_NEAR(counted.hitRate(), 0.75, 1e-9);
    CHECK_EQ(counted.entries, static_cast<std::size_t>(1));
    CHECK_EQ(counted.bytesUsed, sizeOfEntry(2));

    CHECK_EQ(mesh.opaque.quadCount(), static_cast<std::size_t>(2));
    cache.resetStats();
    CHECK_EQ(cache.stats().hits, static_cast<std::uint64_t>(0));
    CHECK_NEAR(cache.stats().hitRate(), 0.0, 1e-9);
}

VF_TEST(peekDoesNotDisturbRecencyOrStatistics) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 2);
    storeMarked(cache, {1, 0, 0}, 2);

    Chunk chunk;
    CHECK(cache.peekChunk({0, 0, 0}, chunk));
    CHECK(!cache.peekChunk({5, 0, 0}, chunk));

    CacheStats counted = cache.stats();
    CHECK_EQ(counted.hits, static_cast<std::uint64_t>(0));
    CHECK_EQ(counted.misses, static_cast<std::uint64_t>(0));
    CHECK(cache.evictionOrder().front() == (Vec3i{0, 0, 0}));
}

VF_TEST(storingOverAnEntryKeepsByteAccountingAndPinning) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    storeMarked(cache, {0, 0, 0}, 12);
    cache.pin({0, 0, 0});
    CHECK_EQ(cache.bytesUsed(), sizeOfEntry(12));

    storeMarked(cache, {0, 0, 0}, 3);
    CHECK_EQ(cache.size(), static_cast<std::size_t>(1));
    CHECK_EQ(cache.bytesUsed(), sizeOfEntry(3));
    CHECK(cache.isPinned({0, 0, 0}));

    CacheEntry entry;
    CHECK(cache.entryFor({0, 0, 0}, entry));
    CHECK_EQ(entry.mesh.opaque.quadCount(), static_cast<std::size_t>(3));
    CHECK(entry.hasMesh);
    CHECK(entry.pinned);
}

VF_TEST(clearEmptiesTheCache) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    for (int i = 0; i < 5; ++i) storeMarked(cache, {i, 0, 0}, 4);
    CHECK_EQ(cache.size(), static_cast<std::size_t>(5));

    cache.clear();
    CHECK_EQ(cache.size(), static_cast<std::size_t>(0));
    CHECK_EQ(cache.bytesUsed(), static_cast<std::uint64_t>(0));
    CHECK(cache.residentCoords().empty());
    CHECK(!cache.contains({0, 0, 0}));
}

VF_TEST(concurrentReadersNeverSeeAHalfWrittenEntry) {
    ChunkCache cache(1ull << 30, CachePolicy::Lru);
    const int coordCount = 8;
    for (int i = 0; i < coordCount; ++i) storeMarked(cache, {i, 0, 0}, 4 + i);

    std::atomic<int> mismatches{0};
    std::atomic<long long> observed{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int reader = 0; reader < 4; ++reader) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_acquire)) {
                for (int i = 0; i < coordCount; ++i) {
                    ChunkMesh mesh;
                    if (!cache.peekMesh({i, 0, 0}, mesh)) continue;
                    observed.fetch_add(1);
                    if (mesh.opaque.quadCount() != static_cast<std::size_t>(4 + i))
                        mismatches.fetch_add(1);
                    else if (mesh.opaque.vertices[0].px != static_cast<float>(i))
                        mismatches.fetch_add(1);
                    if (mesh.opaque.indices.size() != mesh.opaque.quadCount() * 6)
                        mismatches.fetch_add(1);
                }
            }
        });
    }

    for (int round = 0; round < 150; ++round)
        for (int i = 0; i < coordCount; ++i) storeMarked(cache, {i, 0, 0}, 4 + i);

    stop.store(true, std::memory_order_release);
    for (std::thread& reader : readers) reader.join();

    CHECK_EQ(mismatches.load(), 0);
    CHECK(observed.load() > 0);
    CHECK_EQ(cache.size(), static_cast<std::size_t>(coordCount));
}
