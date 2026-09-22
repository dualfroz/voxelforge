#include "testHarness.h"
#include "vf/JobSystem.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace vf;

namespace {

class Latch {
public:
    void open() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            opened = true;
        }
        cv.notify_all();
    }

    void await() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return opened; });
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    bool opened = false;
};

int payload(int index) {
    int value = index * 7919 + 13;
    for (int step = 0; step < 24; ++step) value = (value * 31 + step) % 1000003;
    return value;
}

std::vector<int> runPayloads(unsigned threadCount, int jobCount) {
    std::vector<int> output(static_cast<std::size_t>(jobCount), 0);
    JobSystem jobs(threadCount);
    JobBatch batch;
    for (int i = 0; i < jobCount; ++i) {
        std::size_t slot = static_cast<std::size_t>(i);
        batch.add(jobs.submit([&output, slot, i] { output[slot] = payload(i); }));
    }
    batch.wait();
    jobs.waitAll();
    return output;
}

}

VF_TEST(serialPoolRunsEveryJobOnTheCallingThread) {
    JobSystem jobs(0);
    CHECK(jobs.serialMode());
    CHECK_EQ(jobs.threadCount(), 0u);

    std::thread::id caller = std::this_thread::get_id();
    std::thread::id observed;
    int ran = 0;
    JobHandle handle = jobs.submit([&] {
        observed = std::this_thread::get_id();
        ++ran;
    });
    CHECK_EQ(ran, 1);
    CHECK(handle.done());
    CHECK(observed == caller);
    CHECK(!handle.failed());
}

VF_TEST(serialAndParallelPoolsProduceTheSameResults) {
    const int jobCount = 256;
    std::vector<int> serial = runPayloads(0, jobCount);
    std::vector<int> parallel = runPayloads(4, jobCount);
    CHECK_EQ(serial.size(), parallel.size());
    CHECK_EQ(std::memcmp(serial.data(), parallel.data(), serial.size() * sizeof(int)), 0);
    CHECK(serial[0] != serial[1]);
}

VF_TEST(handleReportsCompletionOnlyAfterTheJobBody) {
    JobSystem jobs(2);
    Latch release;
    std::atomic<bool> finished{false};
    JobHandle handle = jobs.submit([&] {
        release.await();
        finished.store(true);
    });
    CHECK(!handle.done());
    release.open();
    handle.wait();
    CHECK(handle.done());
    CHECK(finished.load());
}

VF_TEST(batchWaitsForEveryHandle) {
    JobSystem jobs(4);
    Latch release;
    std::atomic<int> completed{0};
    JobBatch batch;
    for (int i = 0; i < 16; ++i) {
        batch.add(jobs.submit([&] {
            release.await();
            completed.fetch_add(1);
        }));
    }
    CHECK_EQ(batch.size(), static_cast<std::size_t>(16));
    release.open();
    batch.wait();
    CHECK(batch.done());
    CHECK_EQ(completed.load(), 16);
    CHECK_EQ(batch.failedCount(), static_cast<std::size_t>(0));
}

VF_TEST(graphNeverStartsNodeBeforeItsDependencies) {
    for (unsigned threads : {0u, 1u, 4u}) {
        JobSystem jobs(threads);
        std::atomic<int> ticket{0};
        const std::size_t nodeCount = 8;
        std::vector<int> entered(nodeCount, -1);
        std::vector<int> exited(nodeCount, -1);

        JobGraph graph;
        for (std::size_t i = 0; i < nodeCount; ++i) {
            graph.addNode("node" + std::to_string(i), [&, i] {
                entered[i] = ticket.fetch_add(1);
                exited[i] = ticket.fetch_add(1);
            });
        }
        const std::size_t edges[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4},
                                        {4, 5}, {5, 6}, {6, 7}, {3, 7}};
        for (const auto& edge : edges) graph.addDependency(edge[0], edge[1]);

        CHECK(!graph.hasCycle());
        JobBatch batch = jobs.runGraph(graph);
        CHECK(batch.valid());
        batch.wait();

        for (std::size_t i = 0; i < nodeCount; ++i) CHECK(entered[i] >= 0);
        for (const auto& edge : edges)
            CHECK(exited[edge[1]] < entered[edge[0]]);
        CHECK_EQ(entered[7], 0);
        CHECK_EQ(entered[0], 14);
        CHECK_EQ(batch.failedCount(), static_cast<std::size_t>(0));
    }
}

VF_TEST(graphJoinNodeWaitsForTheWholeFanOut) {
    for (unsigned threads : {0u, 2u, 6u}) {
        JobSystem jobs(threads);
        std::atomic<int> liveFanOut{0};
        std::atomic<int> peakLiveFanOut{0};
        std::atomic<int> doneFanOut{0};
        std::atomic<int> joinSawLive{-1};
        std::atomic<int> joinSawDone{-1};
        std::atomic<int> rootDone{0};
        std::atomic<int> fanOutSawRoot{0};

        JobGraph graph;
        std::size_t root = graph.addNode("root", [&] { rootDone.store(1); });
        std::vector<std::size_t> fanOut;
        for (int i = 0; i < 12; ++i) {
            fanOut.push_back(graph.addNode("fan" + std::to_string(i), [&] {
                fanOutSawRoot.fetch_add(rootDone.load());
                int live = liveFanOut.fetch_add(1) + 1;
                int seen = peakLiveFanOut.load();
                while (live > seen && !peakLiveFanOut.compare_exchange_weak(seen, live)) {
                }
                liveFanOut.fetch_sub(1);
                doneFanOut.fetch_add(1);
            }));
        }
        std::size_t join = graph.addNode("join", [&] {
            joinSawLive.store(liveFanOut.load());
            joinSawDone.store(doneFanOut.load());
        });
        for (std::size_t node : fanOut) {
            graph.addDependency(node, root);
            graph.addDependency(join, node);
        }

        JobBatch batch = jobs.runGraph(graph);
        batch.wait();
        CHECK_EQ(joinSawLive.load(), 0);
        CHECK_EQ(joinSawDone.load(), 12);
        CHECK_EQ(fanOutSawRoot.load(), 12);
        CHECK(peakLiveFanOut.load() >= 1);
        if (threads == 0u) CHECK_EQ(peakLiveFanOut.load(), 1);
        CHECK_EQ(batch.failedCount(), static_cast<std::size_t>(0));
    }
}

VF_TEST(graphWithCycleIsRejectedInsteadOfHanging) {
    for (unsigned threads : {0u, 4u}) {
        JobSystem jobs(threads);
        std::atomic<int> ran{0};
        JobGraph graph;
        for (int i = 0; i < 3; ++i)
            graph.addNode("loop" + std::to_string(i), [&ran] { ran.fetch_add(1); });
        graph.addDependency(0, 2);
        graph.addDependency(1, 0);
        graph.addDependency(2, 1);

        CHECK(graph.hasCycle());
        CHECK_EQ(graph.topologicalOrder().size(), static_cast<std::size_t>(0));
        JobBatch batch = jobs.runGraph(graph);
        CHECK(!batch.valid());
        CHECK(batch.empty());
        batch.wait();
        CHECK_EQ(ran.load(), 0);
    }
}

VF_TEST(selfDependentNodeCountsAsACycle) {
    JobSystem jobs(2);
    JobGraph graph;
    std::atomic<int> ran{0};
    graph.addNode("lonely", [&ran] { ran.fetch_add(1); });
    graph.addDependency(0, 0);
    CHECK(graph.hasCycle());
    JobBatch batch = jobs.runGraph(graph);
    CHECK(!batch.valid());
    CHECK_EQ(ran.load(), 0);
}

VF_TEST(jobExceptionTravelsBackThroughItsHandle) {
    for (unsigned threads : {0u, 3u}) {
        JobSystem jobs(threads);
        JobHandle bad = jobs.submit([] { throw std::runtime_error("chunk generator blew up"); });
        bad.wait();
        CHECK(bad.done());
        CHECK(bad.failed());
        CHECK(bad.errorMessage() == std::string("chunk generator blew up"));

        bool rethrown = false;
        try {
            bad.rethrowError();
        } catch (const std::runtime_error& ex) {
            rethrown = std::string(ex.what()) == "chunk generator blew up";
        }
        CHECK(rethrown);

        std::atomic<int> after{0};
        JobBatch survivors;
        for (int i = 0; i < 32; ++i)
            survivors.add(jobs.submit([&after] { after.fetch_add(1); }));
        survivors.wait();
        CHECK_EQ(after.load(), 32);
        CHECK_EQ(survivors.failedCount(), static_cast<std::size_t>(0));
        CHECK_EQ(jobs.stats().failed, static_cast<std::uint64_t>(1));
    }
}

VF_TEST(graphNodeWithFailedDependencyIsSkippedNotHung) {
    for (unsigned threads : {0u, 4u}) {
        JobSystem jobs(threads);
        std::atomic<int> ranDependent{0};
        std::atomic<int> ranIndependent{0};
        JobGraph graph;
        std::size_t broken = graph.addNode("broken", [] { throw std::runtime_error("stage lost"); });
        std::size_t child = graph.addNode("child", [&] { ranDependent.fetch_add(1); });
        std::size_t grandchild = graph.addNode("grandchild", [&] { ranDependent.fetch_add(1); });
        std::size_t sibling = graph.addNode("sibling", [&] { ranIndependent.fetch_add(1); });
        graph.addDependency(child, broken);
        graph.addDependency(grandchild, child);

        JobBatch batch = jobs.runGraph(graph);
        CHECK(batch.valid());
        batch.wait();

        CHECK(batch.at(broken).failed());
        CHECK(batch.at(child).failed());
        CHECK(batch.at(grandchild).failed());
        CHECK(!batch.at(sibling).failed());
        CHECK_EQ(ranDependent.load(), 0);
        CHECK_EQ(ranIndependent.load(), 1);
        CHECK_EQ(batch.failedCount(), static_cast<std::size_t>(3));
        CHECK(batch.firstError() != nullptr);
    }
}

VF_TEST(highPriorityJobsAreServedBeforeLowOnesOnOneWorker) {
    JobSystem jobs(1);
    Latch started;
    Latch release;
    std::mutex orderMutex;
    std::vector<int> order;

    JobHandle blocker = jobs.submit([&] {
        started.open();
        release.await();
    });
    started.await();

    auto record = [&](int tag) {
        return [&, tag] {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(tag);
        };
    };
    JobHandle low = jobs.submit(record(3), JobPriority::Low);
    JobHandle normal = jobs.submit(record(2), JobPriority::Normal);
    JobHandle high = jobs.submit(record(1), JobPriority::High);

    release.open();
    blocker.wait();
    low.wait();
    normal.wait();
    high.wait();
    jobs.waitAll();

    CHECK_EQ(order.size(), static_cast<std::size_t>(3));
    CHECK_EQ(order[0], 1);
    CHECK_EQ(order[1], 2);
    CHECK_EQ(order[2], 3);
}

VF_TEST(statsCountExecutedJobsAndPeakOccupancy) {
    JobSystem jobs(4);
    JobBatch batch;
    for (int i = 0; i < 120; ++i) batch.add(jobs.submit([] {}));
    batch.wait();
    jobs.waitAll();

    JobStats counted = jobs.stats();
    CHECK_EQ(counted.submitted, static_cast<std::uint64_t>(120));
    CHECK_EQ(counted.executed, static_cast<std::uint64_t>(120));
    CHECK_EQ(counted.queuePops, static_cast<std::uint64_t>(120));
    CHECK_EQ(counted.failed, static_cast<std::uint64_t>(0));
    CHECK(counted.peakBusyWorkers >= 1u);
    CHECK(counted.peakBusyWorkers <= 4u);
    CHECK(counted.peakQueueDepth >= 1u);

    jobs.resetStats();
    CHECK_EQ(jobs.stats().executed, static_cast<std::uint64_t>(0));
}

VF_TEST(waitAllReturnsOnlyWithAnEmptyQueue) {
    JobSystem jobs(4);
    std::atomic<int> done{0};
    for (int i = 0; i < 400; ++i) jobs.submit([&done] { done.fetch_add(1); });
    jobs.waitAll();
    CHECK_EQ(done.load(), 400);
    CHECK_EQ(jobs.stats().executed, static_cast<std::uint64_t>(400));
}

VF_TEST(destructorDrainsQueuedJobsWithoutHanging) {
    std::atomic<int> done{0};
    {
        JobSystem jobs(4);
        for (int i = 0; i < 500; ++i) jobs.submit([&done] { done.fetch_add(1); });
    }
    CHECK_EQ(done.load(), 500);
}

VF_TEST(jobsSubmittedFromInsideAJobStillGetDrained) {
    for (unsigned threads : {0u, 3u}) {
        JobSystem jobs(threads);
        std::atomic<int> leaves{0};
        for (int i = 0; i < 8; ++i) {
            jobs.submit([&jobs, &leaves] {
                for (int k = 0; k < 4; ++k) jobs.submit([&leaves] { leaves.fetch_add(1); });
            });
        }
        jobs.waitAll();
        CHECK_EQ(leaves.load(), 32);
        CHECK_EQ(jobs.stats().executed, static_cast<std::uint64_t>(40));
    }
}

VF_TEST(topologicalOrderIsAFunctionOfTheGraphAlone) {
    JobGraph graph;
    for (int i = 0; i < 6; ++i) graph.addNode("n" + std::to_string(i), [] {});
    graph.addDependency(2, 5);
    graph.addDependency(3, 2);
    graph.addDependency(4, 1);

    std::vector<std::size_t> first = graph.topologicalOrder();
    std::vector<std::size_t> second = graph.topologicalOrder();
    CHECK_EQ(first.size(), static_cast<std::size_t>(6));
    CHECK(first == second);
    for (std::size_t i = 0; i < first.size(); ++i) {
        for (std::size_t dep : graph.nodeAt(first[i]).dependencies) {
            std::size_t depPosition = 0;
            for (std::size_t k = 0; k < first.size(); ++k)
                if (first[k] == dep) depPosition = k;
            CHECK(depPosition < i);
        }
    }
}
