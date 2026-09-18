#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vf {

enum class JobPriority : std::uint8_t {
    High = 0,
    Normal = 1,
    Low = 2,
};

constexpr int jobPriorityCount = 3;

struct JobStats {
    std::uint64_t submitted = 0;
    std::uint64_t executed = 0;
    std::uint64_t failed = 0;
    std::uint64_t queuePops = 0;
    std::uint32_t peakBusyWorkers = 0;
    std::uint32_t peakQueueDepth = 0;
};

class JobHandle {
public:
    JobHandle() = default;

    bool valid() const { return static_cast<bool>(state); }
    bool done() const;
    bool failed() const;
    void wait() const;
    std::exception_ptr error() const;
    std::string errorMessage() const;
    void rethrowError() const;

private:
    friend class JobSystem;

    struct State;

    explicit JobHandle(std::shared_ptr<State> shared) : state(std::move(shared)) {}

    std::shared_ptr<State> state;
};

class JobBatch {
public:
    JobBatch() = default;

    bool valid() const { return runnable; }
    std::size_t size() const { return handles.size(); }
    bool empty() const { return handles.empty(); }
    const JobHandle& at(std::size_t index) const { return handles[index]; }

    void add(const JobHandle& handle) { handles.push_back(handle); }
    void wait() const;
    bool done() const;
    std::size_t failedCount() const;
    std::exception_ptr firstError() const;

private:
    friend class JobSystem;

    std::vector<JobHandle> handles;
    bool runnable = true;
};

struct JobNode {
    std::string name;
    std::function<void()> work;
    JobPriority priority = JobPriority::Normal;
    std::vector<std::size_t> dependencies;
};

class JobGraph {
public:
    std::size_t addNode(std::string name, std::function<void()> work,
                        JobPriority priority = JobPriority::Normal);
    void addDependency(std::size_t node, std::size_t dependsOn);

    std::size_t nodeCount() const { return nodes.size(); }
    const JobNode& nodeAt(std::size_t index) const { return nodes[index]; }

    std::vector<std::size_t> topologicalOrder() const;
    bool hasCycle() const;

    void clear() { nodes.clear(); }

private:
    std::vector<JobNode> nodes;
};

class JobSystem {
public:
    explicit JobSystem(unsigned threadCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    unsigned threadCount() const { return static_cast<unsigned>(workers.size()); }
    bool serialMode() const { return workers.empty(); }

    JobHandle submit(std::function<void()> work, JobPriority priority = JobPriority::Normal);
    JobBatch submitBatch(std::vector<std::function<void()>> work,
                         JobPriority priority = JobPriority::Normal);

    JobBatch runGraph(const JobGraph& graph);

    void waitAll();

    JobStats stats() const;
    void resetStats();

private:
    struct QueuedJob {
        std::function<void()> work;
        std::shared_ptr<JobHandle::State> state;
    };

    struct GraphRun;

    void workerLoop();
    void enqueue(std::function<void()> work, std::shared_ptr<JobHandle::State> state,
                 JobPriority priority);
    void runInline(std::function<void()> work, const std::shared_ptr<JobHandle::State>& state);
    void scheduleGraphNode(const std::shared_ptr<GraphRun>& run, std::size_t index);
    bool executeGraphNode(const std::shared_ptr<GraphRun>& run, std::size_t index);
    void noteInlineStart();
    void noteInlineFinish(bool failed);
    void noteFailure();
    std::size_t queuedLocked() const;
    QueuedJob popLocked();

    static void completeState(const std::shared_ptr<JobHandle::State>& state,
                              std::exception_ptr error);

    mutable std::mutex mutex;
    std::condition_variable workCv;
    std::condition_variable idleCv;
    std::vector<std::thread> workers;
    std::deque<QueuedJob> queues[jobPriorityCount];
    JobStats counters;
    unsigned busy = 0;
    bool stopping = false;
};

}
