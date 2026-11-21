#include "Scheduler.h"
#include "Config.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

constexpr int THREAD_QUANTUM = 2;   // RR/MLFQ rotate threads inside a process every 2 ticks

std::string policyName(SchedulingPolicy p) {
    switch (p) {
        case SchedulingPolicy::FCFS: return "FCFS";
        case SchedulingPolicy::RoundRobin: return "RR";
        case SchedulingPolicy::Priority: return "Priority";
        case SchedulingPolicy::MLFQ: return "MLFQ";
    }
    return "?";
}

Scheduler::Scheduler(SchedulingPolicy policy, int tlbSize, int frames, bool enhancedClock)
    : policy(policy),
      readyQueues(policy == SchedulingPolicy::MLFQ ? MLFQ_LEVELS : 1),
      quantum(policy == SchedulingPolicy::RoundRobin ? RR_QUANTUM : 0),
      tlb(tlbSize), pageTable(NUM_PAGES), clock(frames, enhancedClock), cpu(tlb, pageTable, clock) {}

void Scheduler::addProcessIfNotExists(int pid) {
    if (processes.count(pid)) return;
    processes.emplace(pid, Process(pid, pid));      // default priority = pid
    arrivalOrder.push_back(pid);
    readyQueues[0].push_back(pid);                  // everyone arrives at t=0
}

Process& Scheduler::getProcess(int pid) {
    addProcessIfNotExists(pid);
    return processes.at(pid);
}

int Scheduler::levelQuantum(int level) const { return MLFQ_QUANTA[level]; }

void Scheduler::enqueue(int pid) {
    int level = policy == SchedulingPolicy::MLFQ ? processes.at(pid).mlfqLevel : 0;
    processes.at(pid).waitTicks = 0;
    readyQueues[level].push_back(pid);
}

void Scheduler::removeFromQueues(int pid) {
    for (auto& q : readyQueues) q.erase(std::remove(q.begin(), q.end(), pid), q.end());
}

void Scheduler::adjustPriority(int pid, int threadId) {
    Process& p = processes.at(pid);
    if (p.mlfqLevel < MLFQ_LEVELS - 1) { p.mlfqLevel++; demotions++; }
    p.threadPriorities[threadId]++;                 // thread burned its slice -> lower priority
}

// MLFQ aging: anything waiting below the top queue for AGING_THRESHOLD ticks moves up.
void Scheduler::ageWaiting() {
    if (policy != SchedulingPolicy::MLFQ) return;
    for (int level = 1; level < MLFQ_LEVELS; level++) {
        std::deque<int> stay;
        for (int pid : readyQueues[level]) {
            Process& p = processes.at(pid);
            if (++p.waitTicks >= AGING_THRESHOLD) {
                p.mlfqLevel = level - 1;
                p.waitTicks = 0;
                readyQueues[level - 1].push_back(pid);
                promotions++;
            } else {
                stay.push_back(pid);
            }
        }
        readyQueues[level] = stay;
    }
}

int Scheduler::pickProcess() {
    if (policy == SchedulingPolicy::Priority) {
        auto& q = readyQueues[0];
        auto best = std::min_element(q.begin(), q.end(), [&](int a, int b) {
            return processes.at(a).priority < processes.at(b).priority;
        });
        int pid = *best;
        q.erase(best);
        return pid;
    }
    for (auto& q : readyQueues) {                   // FCFS/RR: single queue; MLFQ: top-down
        if (!q.empty()) { int pid = q.front(); q.pop_front(); return pid; }
    }
    return -1;
}

int Scheduler::pickThread(Process& p) {
    std::vector<int> active = p.activeThreads();
    int tid = -1;
    bool curOk = curThread != -1 && p.threadHasMore(curThread);

    switch (policy) {
    case SchedulingPolicy::FCFS:                    // follow trace order
        tid = p.memoryAccesses[p.accessIndex].threadId;
        break;
    case SchedulingPolicy::RoundRobin: {
        if (curOk && threadSliceUsed < THREAD_QUANTUM) { tid = curThread; break; }
        int after = curThread != -1 ? curThread : p.lastThread;
        auto it = std::upper_bound(active.begin(), active.end(), after);
        tid = it == active.end() ? active.front() : *it;
        break;
    }
    case SchedulingPolicy::Priority: {              // highest-priority thread runs first
        tid = *std::min_element(active.begin(), active.end(), [&](int a, int b) {
            return p.threadPriorities[a] < p.threadPriorities[b];
        });
        break;
    }
    case SchedulingPolicy::MLFQ: {                  // best thread priority, rotate ties
        int best = 1 << 30;
        for (int t : active) best = std::min(best, p.threadPriorities[t]);
        if (curOk && p.threadPriorities[curThread] == best && threadSliceUsed < THREAD_QUANTUM) {
            tid = curThread; break;
        }
        std::vector<int> tied;
        for (int t : active) if (p.threadPriorities[t] == best) tied.push_back(t);
        int after = curThread != -1 ? curThread : p.lastThread;
        auto it = std::upper_bound(tied.begin(), tied.end(), after);
        tid = it == tied.end() ? tied.front() : *it;
        break;
    }
    }
    if (tid != curThread) threadSliceUsed = 0;
    curThread = tid;
    p.lastThread = tid;
    return tid;
}

bool Scheduler::tick() {
    bool anyLeft = false;
    for (auto& [pid, p] : processes) if (p.hasMoreAccesses()) { anyLeft = true; break; }
    if (!anyLeft) return false;

    ageWaiting();

    // Preemption decisions for the running process
    if (current != -1) {
        Process& p = processes.at(current);
        bool preempt = false;
        switch (policy) {
        case SchedulingPolicy::FCFS: break;
        case SchedulingPolicy::RoundRobin: preempt = sliceUsed >= quantum; break;
        case SchedulingPolicy::Priority:
            for (int pid : readyQueues[0])
                if (processes.at(pid).priority < p.priority) preempt = true;
            break;
        case SchedulingPolicy::MLFQ:
            if (sliceUsed >= levelQuantum(p.mlfqLevel)) {
                adjustPriority(current, curThread);
                preempt = true;
            } else {
                for (int l = 0; l < p.mlfqLevel; l++) if (!readyQueues[l].empty()) preempt = true;
            }
            break;
        }
        if (preempt) { enqueue(current); current = -1; }
    }

    if (current == -1) {
        current = pickProcess();
        sliceUsed = 0;
        curThread = -1;
        threadSliceUsed = 0;
    }

    Process& p = processes.at(current);
    if (p.firstRun < 0) p.firstRun = (int)time;
    int tid = pickThread(p);
    events.push_back(cpu.execute(p, tid));
    sliceUsed++;
    threadSliceUsed++;
    time++;

    if (!p.hasMoreAccesses()) { p.finishTime = (int)time; current = -1; }
    return true;
}

static std::string hex4(unsigned v) {
    std::ostringstream s;
    s << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << v;
    return s.str();
}

void Scheduler::run(std::ostream* log, std::ostream* timeline) {
    if (timeline) *timeline << "tick,pid,tid,address,page,rw,tlb_hit,page_fault,write_back,context_switch,thread_switch,cost\n";
    while (tick()) {
        const ExecResult& r = events.back();
        long t = (long)events.size() - 1;
        if (log) {
            std::ostringstream s;
            s << "t=" << std::setw(4) << std::setfill('0') << t << std::setfill(' ')
              << "  P" << r.pid << " T" << r.tid << "  " << (r.isWrite ? 'W' : 'R') << " "
              << hex4(r.address) << "  page=" << std::setw(3) << r.page
              << " (outer " << outerOf(r.page) << ", inner " << innerOf(r.page) << ")  ";
            if (r.tlbHit) s << "TLB HIT  frame=" << r.frame;
            else if (!r.pageFault) s << "TLB MISS -> page table hit, frame=" << r.frame;
            else {
                s << "TLB MISS -> PAGE FAULT, loaded into frame " << r.frame;
                if (r.evictedPid != -1)
                    s << ", evicted P" << r.evictedPid << " page " << r.evictedPage
                      << (r.writeBack ? " (dirty, written back)" : " (clean)");
            }
            if (r.contextSwitch) s << "  [CONTEXT SWITCH]";
            if (r.threadSwitch) s << "  [THREAD SWITCH]";
            *log << s.str() << "\n";
        }
        if (timeline)
            *timeline << t << ',' << r.pid << ',' << r.tid << ',' << hex4(r.address) << ','
                      << r.page << ',' << (r.isWrite ? 'W' : 'R') << ',' << r.tlbHit << ','
                      << r.pageFault << ',' << r.writeBack << ',' << r.contextSwitch << ','
                      << r.threadSwitch << ',' << r.cost << '\n';
    }
}

struct Metrics {
    long refs, hits, faults, wb, ctx, thr, mem;
    double hitRate, faultRate, eat, avgTurn, avgResp, jain;
};

static Metrics computeMetrics(const std::vector<ExecResult>& ev, const std::map<int, Process>& procs,
                              const TLB& tlb, const TwoLevelPageTable& pt, const Clock& clk,
                              const CPU& cpu) {
    Metrics m{};
    m.refs = (long)ev.size();
    m.hits = tlb.hits();
    m.faults = pt.faults();
    m.wb = clk.writeBacks();
    m.ctx = cpu.ctxSwitches();
    m.thr = cpu.thrSwitches();
    m.mem = cpu.memCycles();
    m.hitRate = m.refs ? 100.0 * m.hits / m.refs : 0;
    m.faultRate = m.refs ? 100.0 * m.faults / m.refs : 0;
    m.eat = m.refs ? (double)m.mem / m.refs : 0;

    double turn = 0, resp = 0;
    for (auto& [pid, p] : procs) { turn += p.finishTime; resp += p.firstRun; }
    m.avgTurn = procs.empty() ? 0 : turn / procs.size();
    m.avgResp = procs.empty() ? 0 : resp / procs.size();

    // Jain's fairness index over per-thread slowdown (finish tick / cycles it needed)
    std::map<std::pair<int, int>, long> finish, work;
    for (size_t i = 0; i < ev.size(); i++) {
        finish[{ev[i].pid, ev[i].tid}] = (long)i + 1;
        work[{ev[i].pid, ev[i].tid}]++;
    }
    double sum = 0, sq = 0;
    for (auto& [k, f] : finish) { double s = (double)f / work[k]; sum += s; sq += s * s; }
    m.jain = finish.empty() ? 1.0 : (sum * sum) / (finish.size() * sq);
    return m;
}

void Scheduler::printStats() const {
    Metrics m = computeMetrics(events, processes, tlb, pageTable, clock, cpu);
    std::cout << "\n===== " << policyName(policy) << " summary =====\n";
    cpu.printStats();
    tlb.printStats();
    pageTable.printStats();
    clock.printStats();
    std::cout << std::fixed << std::setprecision(2)
              << "Page fault rate=" << m.faultRate << "%  effective access time="
              << m.eat << " cycles/ref\n";
    if (policy == SchedulingPolicy::MLFQ)
        std::cout << "MLFQ: demotions=" << demotions << " aging promotions=" << promotions << "\n";
    std::cout << "Per-process: ";
    for (auto& [pid, p] : processes)
        std::cout << "P" << pid << "(start " << p.firstRun << ", finish " << p.finishTime << ") ";
    std::cout << "\nAvg turnaround=" << m.avgTurn << "  avg response=" << m.avgResp
              << "  thread fairness (Jain)=" << std::setprecision(3) << m.jain << "\n";
}

void Scheduler::writeSummaryCSV(std::ostream& out, const std::string& trace) const {
    Metrics m = computeMetrics(events, processes, tlb, pageTable, clock, cpu);
    out << std::fixed << std::setprecision(3) << policyName(policy) << ',' << trace << ','
        << tlb.size() << ',' << m.refs << ',' << m.hits << ',' << m.hitRate << ',' << m.faults
        << ',' << m.faultRate << ',' << m.wb << ',' << m.ctx << ',' << m.thr << ',' << m.eat
        << ',' << m.avgTurn << ',' << m.avgResp << ',' << m.jain << '\n';
}

// One row per thread, one column per tick: '#' TLB hit, 'm' TLB miss (no fault),
// 'F' page fault, 'W' fault with dirty write-back. Bottom row marks switches.
void Scheduler::printAsciiTimeline(std::ostream& out) const {
    std::map<std::pair<int, int>, std::string> rows;
    std::string sw(events.size(), ' ');
    for (auto& e : events) rows[{e.pid, e.tid}];
    for (auto& [k, s] : rows) s.assign(events.size(), '.');
    for (size_t i = 0; i < events.size(); i++) {
        const ExecResult& e = events[i];
        char c = e.tlbHit ? '#' : (!e.pageFault ? 'm' : (e.writeBack ? 'W' : 'F'));
        rows[{e.pid, e.tid}][i] = c;
        if (e.contextSwitch) sw[i] = 'C';
        else if (e.threadSwitch) sw[i] = 't';
    }
    out << "\nTimeline (" << policyName(policy) << ")  # hit  m TLB miss  F fault  W fault+write-back"
        << "  | C context switch  t thread switch\n";
    const size_t width = 100;
    for (size_t start = 0; start < events.size(); start += width) {
        size_t len = std::min(width, events.size() - start);
        out << "tick " << start << "-" << start + len - 1 << "\n";
        for (auto& [k, s] : rows)
            out << "P" << k.first << "T" << k.second << " |" << s.substr(start, len) << "|\n";
        out << "sw   |" << sw.substr(start, len) << "|\n";
    }
}
