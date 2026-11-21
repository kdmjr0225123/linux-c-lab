#include "Process.h"

Process::Process(int pid, int priority)
    : pid(pid), priority(priority), cpuBurstRemaining(0), accessIndex(0) {}

void Process::addAccess(int threadId, unsigned address, bool isWrite) {
    perThread[threadId].push_back(memoryAccesses.size());
    memoryAccesses.push_back({threadId, address, isWrite});
    done.push_back(false);
    cpuBurstRemaining++;
    if (!threadPriorities.count(threadId)) threadPriorities[threadId] = threadId;
    threadPos.emplace(threadId, 0);
}

void Process::addThread(int threadId, int threadPriority) {
    threadPriorities[threadId] = threadPriority;
    threadPos.emplace(threadId, 0);
}

bool Process::hasMoreAccesses() const { return cpuBurstRemaining > 0; }

bool Process::threadHasMore(int threadId) const {
    auto it = perThread.find(threadId);
    if (it == perThread.end()) return false;
    return threadPos.at(threadId) < it->second.size();
}

std::vector<int> Process::activeThreads() const {
    std::vector<int> out;
    for (auto& [tid, idx] : perThread)
        if (threadPos.at(tid) < idx.size()) out.push_back(tid);
    return out;
}

std::tuple<int, unsigned, bool> Process::nextAccess() {
    while (accessIndex < done.size() && done[accessIndex]) accessIndex++;
    const MemoryAccess& a = memoryAccesses[accessIndex];
    return nextAccess(a.threadId);
}

std::tuple<int, unsigned, bool> Process::nextAccess(int threadId) {
    size_t& pos = threadPos[threadId];
    size_t i = perThread[threadId][pos++];
    done[i] = true;
    while (accessIndex < done.size() && done[accessIndex]) accessIndex++;
    const MemoryAccess& a = memoryAccesses[i];
    return {a.threadId, a.address, a.isWrite};
}

void Process::tick(int threadId) {
    cpuBurstRemaining--;
    threadCycles[threadId]++;
}
