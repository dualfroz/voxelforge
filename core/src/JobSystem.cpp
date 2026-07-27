#include "vf/JobSystem.h"

#include <atomic>
#include <stdexcept>
#include <utility>

namespace vf {

struct JobHandle::State {
    std::mutex mutex;
    std::condition_variable cv;
    std::exception_ptr error;
    bool finished = false;
};

struct JobSystem::GraphRun {
    explicit GraphRun(std::size_t count)
        : work(count),
          names(count),
          priorities(count, JobPriority::Normal),
          dependents(count),
          remaining(count),
          depFailed(count),
          states(count) {}

    std::vector<std::function<void()>> work;
    std::vector<std::string> names;
    std::vector<JobPriority> priorities;
    std::vector<std::vector<std::size_t>> dependents;
    std::vector<std::atomic<int>> remaining;
    std::vector<std::atomic<bool>> depFailed;
    std::vector<std::shared_ptr<JobHandle::State>> states;
    bool serial = false;
};

bool JobHandle::done() const {
    if (!state) return true;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->finished;
}

bool JobHandle::failed() const {
    if (!state) return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->finished && state->error != nullptr;
}

void JobHandle::wait() const {
    if (!state) return;
    std::unique_lock<std::mutex> lock(state->mutex);
    state->cv.wait(lock, [this] { return state->finished; });
}

std::exception_ptr JobHandle::error() const {
    if (!state) return nullptr;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->error;
}

std::string JobHandle::errorMessage() const {
    std::exception_ptr captured = error();
    if (!captured) return std::string();
    try {
        std::rethrow_exception(captured);
    } catch (const std::exception& ex) {
        return ex.what();
    } catch (...) {
        return std::string("unknown exception");
    }
}

void JobHandle::rethrowError() const {
    std::exception_ptr captured = error();
    if (captured) std::rethrow_exception(captured);
}

void JobBatch::wait() const {
    for (const JobHandle& handle : handles) handle.wait();
}

bool JobBatch::done() const {
    for (const JobHandle& handle : handles)
        if (!handle.done()) return false;
    return true;
}

std::size_t JobBatch::failedCount() const {
    std::size_t count = 0;
    for (const JobHandle& handle : handles)
        if (handle.failed()) ++count;
    return count;
}

std::exception_ptr JobBatch::firstError() const {
    for (const JobHandle& handle : handles) {
        std::exception_ptr captured = handle.error();
        if (captured) return captured;
    }
    return nullptr;
}

std::size_t JobGraph::addNode(std::string name, std::function<void()> work, JobPriority priority) {
    JobNode node;
    node.name = std::move(name);
    node.work = std::move(work);
    node.priority = priority;
    nodes.push_back(std::move(node));
    return nodes.size() - 1;
}

void JobGraph::addDependency(std::size_t node, std::size_t dependsOn) {
    nodes[node].dependencies.push_back(dependsOn);
}

std::vector<std::size_t> JobGraph::topologicalOrder() const {
    std::vector<int> indegree(nodes.size(), 0);
    std::vector<std::vector<std::size_t>> dependents(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (std::size_t dep : nodes[i].dependencies) {
            ++indegree[i];
            dependents[dep].push_back(i);
        }
    }

    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (indegree[i] == 0) ready.push_back(i);

    std::vector<std::size_t> order;
    order.reserve(nodes.size());
    for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
        std::size_t current = ready[cursor];
        order.push_back(current);
        for (std::size_t dependent : dependents[current])
            if (--indegree[dependent] == 0) ready.push_back(dependent);
    }
    return order;
}

bool JobGraph::hasCycle() const {
    return topologicalOrder().size() != nodes.size();
}

JobSystem::JobSystem(unsigned threadCount) {
    workers.reserve(threadCount);
    for (unsigned i = 0; i < threadCount; ++i)
        workers.emplace_back([this] { workerLoop(); });
}

JobSystem::~JobSystem() {
    if (workers.empty()) return;
    waitAll();
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    workCv.notify_all();
    for (std::thread& worker : workers) worker.join();
}

void JobSystem::completeState(const std::shared_ptr<JobHandle::State>& state,
                              std::exception_ptr error) {
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->error = error;
        state->finished = true;
    }
    state->cv.notify_all();
}

std::size_t JobSystem::queuedLocked() const {
    std::size_t total = 0;
    for (int i = 0; i < jobPriorityCount; ++i) total += queues[i].size();
    return total;
}

JobSystem::QueuedJob JobSystem::popLocked() {
    for (int i = 0; i < jobPriorityCount; ++i) {
        if (queues[i].empty()) continue;
        QueuedJob job = std::move(queues[i].front());
        queues[i].pop_front();
        return job;
    }
    return QueuedJob{};
}

void JobSystem::enqueue(std::function<void()> work, std::shared_ptr<JobHandle::State> state,
                        JobPriority priority) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++counters.submitted;
        queues[static_cast<int>(priority)].push_back(QueuedJob{std::move(work), std::move(state)});
        std::uint32_t depth = static_cast<std::uint32_t>(queuedLocked());
        if (depth > counters.peakQueueDepth) counters.peakQueueDepth = depth;
    }
    workCv.notify_one();
}

void JobSystem::noteInlineStart() {
    std::lock_guard<std::mutex> lock(mutex);
    ++counters.submitted;
    if (counters.peakBusyWorkers < 1u) counters.peakBusyWorkers = 1u;
}

void JobSystem::noteInlineFinish(bool failed) {
    std::lock_guard<std::mutex> lock(mutex);
    ++counters.executed;
    if (failed) ++counters.failed;
}

void JobSystem::noteFailure() {
    std::lock_guard<std::mutex> lock(mutex);
    ++counters.failed;
}

void JobSystem::runInline(std::function<void()> work,
                          const std::shared_ptr<JobHandle::State>& state) {
    noteInlineStart();
    std::exception_ptr captured;
    try {
        work();
    } catch (...) {
        captured = std::current_exception();
    }
    completeState(state, captured);
    noteInlineFinish(captured != nullptr);
}

JobHandle JobSystem::submit(std::function<void()> work, JobPriority priority) {
    auto state = std::make_shared<JobHandle::State>();
    if (workers.empty()) {
        runInline(std::move(work), state);
        return JobHandle(state);
    }
    enqueue(std::move(work), state, priority);
    return JobHandle(state);
}

JobBatch JobSystem::submitBatch(std::vector<std::function<void()>> work, JobPriority priority) {
    JobBatch batch;
    for (std::function<void()>& job : work) batch.add(submit(std::move(job), priority));
    return batch;
}

void JobSystem::workerLoop() {
    for (;;) {
        std::unique_lock<std::mutex> lock(mutex);
        workCv.wait(lock, [this] { return stopping || queuedLocked() > 0; });
        if (queuedLocked() == 0) {
            if (stopping) return;
            continue;
        }

        QueuedJob job = popLocked();
        ++counters.queuePops;
        ++busy;
        if (busy > counters.peakBusyWorkers) counters.peakBusyWorkers = busy;
        lock.unlock();

        std::exception_ptr captured;
        try {
            job.work();
        } catch (...) {
            captured = std::current_exception();
        }
        if (job.state) completeState(job.state, captured);

        lock.lock();
        --busy;
        ++counters.executed;
        if (captured) ++counters.failed;
        bool idle = busy == 0 && queuedLocked() == 0;
        lock.unlock();
        if (idle) idleCv.notify_all();
    }
}

void JobSystem::waitAll() {
    if (workers.empty()) return;
    std::unique_lock<std::mutex> lock(mutex);
    idleCv.wait(lock, [this] { return busy == 0 && queuedLocked() == 0; });
}

bool JobSystem::executeGraphNode(const std::shared_ptr<GraphRun>& run, std::size_t index) {
    std::exception_ptr captured;
    if (run->depFailed[index].load(std::memory_order_acquire)) {
        captured = std::make_exception_ptr(
            std::runtime_error("dependency failed, skipped job " + run->names[index]));
    } else {
        try {
            run->work[index]();
        } catch (...) {
            captured = std::current_exception();
        }
    }
    completeState(run->states[index], captured);

    for (std::size_t dependent : run->dependents[index]) {
        if (captured) run->depFailed[dependent].store(true, std::memory_order_release);
        if (run->remaining[dependent].fetch_sub(1, std::memory_order_acq_rel) == 1 && !run->serial)
            scheduleGraphNode(run, dependent);
    }
    return captured != nullptr;
}

void JobSystem::scheduleGraphNode(const std::shared_ptr<GraphRun>& run, std::size_t index) {
    JobPriority priority = run->priorities[index];
    enqueue([this, run, index] {
                if (executeGraphNode(run, index)) noteFailure();
            },
            nullptr, priority);
}

JobBatch JobSystem::runGraph(const JobGraph& graph) {
    JobBatch batch;
    std::size_t count = graph.nodeCount();
    std::vector<std::size_t> order = graph.topologicalOrder();
    if (order.size() != count) {
        batch.runnable = false;
        return batch;
    }

    auto run = std::make_shared<GraphRun>(count);
    run->serial = workers.empty();
    for (std::size_t i = 0; i < count; ++i) {
        const JobNode& node = graph.nodeAt(i);
        run->work[i] = node.work;
        run->names[i] = node.name;
        run->priorities[i] = node.priority;
        run->remaining[i].store(static_cast<int>(node.dependencies.size()),
                                std::memory_order_relaxed);
        run->states[i] = std::make_shared<JobHandle::State>();
        batch.add(JobHandle(run->states[i]));
    }
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t dep : graph.nodeAt(i).dependencies) run->dependents[dep].push_back(i);

    if (run->serial) {
        for (std::size_t index : order) {
            noteInlineStart();
            bool failed = executeGraphNode(run, index);
            noteInlineFinish(failed);
        }
        return batch;
    }

    std::vector<std::size_t> roots;
    for (std::size_t i = 0; i < count; ++i)
        if (run->remaining[i].load(std::memory_order_relaxed) == 0) roots.push_back(i);
    for (std::size_t index : roots) scheduleGraphNode(run, index);
    return batch;
}

JobStats JobSystem::stats() const {
    std::lock_guard<std::mutex> lock(mutex);
    return counters;
}

void JobSystem::resetStats() {
    std::lock_guard<std::mutex> lock(mutex);
    counters = JobStats{};
}

} // namespace vf
