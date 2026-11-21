#ifndef PROCESS_H
#define PROCESS_H

#include <map>
#include <tuple>
#include <vector>

struct MemoryAccess {
    int threadId;
    unsigned address;
    bool isWrite;
};

class Process {
public:
    int pid;
    int priority;                                   // lower number = higher priority
    int cpuBurstRemaining;                          // accesses left (1 access = 1 cycle)
    std::vector<MemoryAccess> memoryAccesses;       // (thread_id, address, R/W), trace order
    size_t accessIndex;                             // next unexecuted access in trace order
    std::map<int, int> threadPriorities;            // tid -> priority

    // Scheduling bookkeeping
    int mlfqLevel = 0;
    int waitTicks = 0;
    int firstRun = -1;
    int finishTime = -1;
    int lastThread = -1;

    Process() : Process(0, 0) {}
    Process(int pid, int priority);

    void addAccess(int threadId, unsigned address, bool isWrite);
    void addThread(int threadId, int threadPriority);

    bool hasMoreAccesses() const;
    bool threadHasMore(int threadId) const;
    std::vector<int> activeThreads() const;        // threads with work left

    std::tuple<int, unsigned, bool> nextAccess();              // next in trace order
    std::tuple<int, unsigned, bool> nextAccess(int threadId);  // next for a given thread
    void tick(int threadId);                       // account one cycle for thread

    std::map<int, int> threadCycles;               // tid -> cycles executed
private:
    std::map<int, std::vector<size_t>> perThread;  // tid -> indices into memoryAccesses
    std::map<int, size_t> threadPos;               // tid -> position in perThread[tid]
    std::vector<bool> done;
};

#endif
