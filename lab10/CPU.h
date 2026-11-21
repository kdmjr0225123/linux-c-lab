#ifndef CPU_H
#define CPU_H

#include "Clock.h"
#include "Process.h"
#include "TLB.h"
#include "TwoLevelPageTable.h"
#include <string>

// What happened on one CPU cycle (one memory reference)
struct ExecResult {
    int pid = -1, tid = -1;
    unsigned address = 0;
    bool isWrite = false;
    int page = -1, frame = -1;
    bool tlbHit = false, pageFault = false, writeBack = false;
    int evictedPid = -1, evictedPage = -1;
    bool contextSwitch = false, threadSwitch = false;
    long cost = 0;                                  // modeled cycles for this reference
};

// Modeled latencies (cycles) used for the effective-access-time estimate
constexpr long COST_TLB_HIT    = 1;
constexpr long COST_WALK       = 10;    // two memory reads for a 2-level walk
constexpr long COST_FAULT      = 1000;  // read page from disk
constexpr long COST_WRITEBACK  = 1000;  // flush dirty victim to disk

class CPU {
public:
    CPU(TLB& tlb, TwoLevelPageTable& pt, Clock& clock);
    ExecResult execute(Process& p, int threadId);    // runs one access of this thread
    void printStats() const;

    long cycles() const { return cyclesExecuted; }
    long ctxSwitches() const { return contextSwitches; }
    long thrSwitches() const { return threadSwitches; }
    long memCycles() const { return memoryCycles; }
private:
    TLB& tlb;
    TwoLevelPageTable& pageTable;
    Clock& clock;
    long cyclesExecuted = 0;
    long contextSwitches = 0;
    long threadSwitches = 0;
    long memoryCycles = 0;
    int lastPid = -1, lastTid = -1;
};

#endif
