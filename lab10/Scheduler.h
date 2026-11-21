#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "CPU.h"
#include "Clock.h"
#include "Process.h"
#include "TLB.h"
#include "TwoLevelPageTable.h"
#include <deque>
#include <map>
#include <ostream>
#include <string>
#include <vector>

enum class SchedulingPolicy { FCFS, RoundRobin, Priority, MLFQ };

std::string policyName(SchedulingPolicy p);

class Scheduler {
public:
    Scheduler(SchedulingPolicy policy, int tlbSize, int frames, bool enhancedClock = true);

    void addProcessIfNotExists(int pid);
    Process& getProcess(int pid);
    bool tick();                                    // false once every process is done
    void adjustPriority(int pid, int threadId);     // MLFQ demotion (process + thread)
    void run(std::ostream* log, std::ostream* timeline);

    void printStats() const;
    void writeSummaryCSV(std::ostream& out, const std::string& trace) const;
    void printAsciiTimeline(std::ostream& out) const;

    const std::vector<ExecResult>& history() const { return events; }
private:
    SchedulingPolicy policy;
    std::map<int, Process> processes;
    std::vector<std::deque<int>> readyQueues;       // 1 queue for FCFS/RR/Priority, 3 for MLFQ
    int quantum;

    TLB tlb;
    TwoLevelPageTable pageTable;
    Clock clock;
    CPU cpu;

    std::vector<int> arrivalOrder;
    int current = -1;          // running pid
    int sliceUsed = 0;         // ticks of current slice (process level)
    int curThread = -1;        // running thread of current process
    int threadSliceUsed = 0;
    long time = 0;
    long promotions = 0, demotions = 0;
    std::vector<ExecResult> events;

    int pickProcess();
    int pickThread(Process& p);
    int levelQuantum(int level) const;
    void ageWaiting();
    void enqueue(int pid);
    void removeFromQueues(int pid);
};

#endif
