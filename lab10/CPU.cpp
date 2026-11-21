#include "CPU.h"
#include "Config.h"
#include <iostream>

CPU::CPU(TLB& tlb, TwoLevelPageTable& pt, Clock& clock)
    : tlb(tlb), pageTable(pt), clock(clock) {}

ExecResult CPU::execute(Process& p, int threadId) {
    ExecResult r;
    r.pid = p.pid;
    r.tid = threadId;

    // Switch accounting: a new process is a context switch; a new thread inside the
    // same process is a (cheaper) thread switch.
    if (lastPid != -1 && p.pid != lastPid) { contextSwitches++; r.contextSwitch = true; }
    else if (lastTid != -1 && threadId != lastTid) { threadSwitches++; r.threadSwitch = true; }
    lastPid = p.pid;
    lastTid = threadId;

    auto [tid, addr, isWrite] = p.nextAccess(threadId);
    r.address = addr;
    r.isWrite = isWrite;
    r.page = pageOf(addr);

    int frame = -1;
    if (tlb.lookup(p.pid, r.page, frame)) {          // 1) TLB
        r.tlbHit = true;
        r.cost = COST_TLB_HIT;
    } else {
        r.cost = COST_TLB_HIT + COST_WALK;            // 2) walk outer -> inner table
        if (pageTable.isInMemory(p.pid, r.page)) {
            frame = pageTable.getFrame(p.pid, r.page);
        } else {                                     // 3) page fault
            r.pageFault = true;
            pageTable.recordFault();
            std::pair<int, int> victim;
            bool dirty = false;
            frame = clock.evictAndReplace(p.pid, r.page, victim, dirty);
            if (victim.first != -1) {
                pageTable.unloadPage(victim.first, victim.second);
                tlb.invalidate(victim.first, victim.second);
                r.evictedPid = victim.first;
                r.evictedPage = victim.second;
                r.writeBack = dirty;
                if (dirty) r.cost += COST_WRITEBACK;
            }
            pageTable.loadPage(p.pid, r.page, frame);
            r.cost += COST_FAULT;
        }
        tlb.insert(p.pid, r.page, frame);
    }
    clock.touch(frame, isWrite);                     // set R / M bits
    r.frame = frame;

    p.tick(threadId);
    cyclesExecuted++;
    memoryCycles += r.cost;
    return r;
}

void CPU::printStats() const {
    std::cout << "CPU: cycles=" << cyclesExecuted << " context switches=" << contextSwitches
              << " thread switches=" << threadSwitches
              << " modeled memory cycles=" << memoryCycles << "\n";
}
