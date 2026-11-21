// CS375 Lab 10 - Demand Paging and Advanced Clock Replacement
// Usage: ./paging [--policy fcfs|rr|priority|mlfq|all] [--tlb N] [--frames N]
//                 [--clock enhanced|plain] [--trace FILE] [--log FILE] [--timeline-dir DIR] [--csv FILE] [--quiet]
#include "Config.h"
#include "Scheduler.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct TraceLine { int pid, tid; unsigned addr; bool write; };
struct Directive { std::string kind; int a, b, c; };

static bool loadTrace(const std::string& path, std::vector<TraceLine>& out, std::vector<Directive>& dirs) {
    std::ifstream in(path);
    if (!in) { std::cerr << "Cannot open trace file: " << path << "\n"; return false; }
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (line.empty()) continue;
        if (line.rfind("#!", 0) == 0) {          // optional priority directives
            std::istringstream ds(line.substr(2));
            Directive d{};
            bool ok = (bool)(ds >> d.kind >> d.a >> d.b);
            if (ok && d.kind == "thread") ok = (bool)(ds >> d.c);
            if (ok) dirs.push_back(d);                // anything else is just a comment
            continue;
        }
        if (line[0] == '#') continue;
        std::istringstream ss(line);
        TraceLine t; std::string addr, type;
        if (!(ss >> t.pid >> t.tid >> addr >> type)) {
            std::cerr << "Skipping malformed line " << lineNo << ": " << line << "\n";
            continue;
        }
        t.addr = (unsigned)std::stoul(addr, nullptr, 0);
        t.write = (type == "W" || type == "w");
        out.push_back(t);
    }
    return true;
}

static std::string baseName(const std::string& p) {
    size_t s = p.find_last_of('/');
    std::string b = s == std::string::npos ? p : p.substr(s + 1);
    size_t d = b.find_last_of('.');
    return d == std::string::npos ? b : b.substr(0, d);
}

int main(int argc, char* argv[]) {
    std::string policyArg = "all", trace = "input/trace.txt", logPath = "output/output_log.txt";
    std::string timelineDir = "output", csvPath;
    int tlbSize = DEFAULT_TLB_SIZE, frames = DEFAULT_FRAMES;
    bool quiet = false, enhancedClock = true;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--policy") policyArg = next();
        else if (a == "--tlb") tlbSize = std::atoi(next().c_str());
        else if (a == "--frames") frames = std::atoi(next().c_str());
        else if (a == "--trace") trace = next();
        else if (a == "--log") logPath = next();
        else if (a == "--timeline-dir") timelineDir = next();
        else if (a == "--csv") csvPath = next();
        else if (a == "--quiet") quiet = true;
        else if (a == "--clock") enhancedClock = next() != "plain";
        else { std::cerr << "Unknown argument " << a << "\n"; return 1; }
    }
    if (tlbSize < 0 || frames < 1) { std::cerr << "Bad --tlb/--frames value\n"; return 1; }

    std::vector<SchedulingPolicy> policies;
    if (policyArg == "fcfs") policies = {SchedulingPolicy::FCFS};
    else if (policyArg == "rr") policies = {SchedulingPolicy::RoundRobin};
    else if (policyArg == "priority") policies = {SchedulingPolicy::Priority};
    else if (policyArg == "mlfq") policies = {SchedulingPolicy::MLFQ};
    else if (policyArg == "all") policies = {SchedulingPolicy::FCFS, SchedulingPolicy::RoundRobin,
                                             SchedulingPolicy::Priority, SchedulingPolicy::MLFQ};
    else { std::cerr << "Unknown policy " << policyArg << "\n"; return 1; }

    std::vector<TraceLine> lines;
    std::vector<Directive> dirs;
    if (!loadTrace(trace, lines, dirs)) return 1;

    std::ofstream log;
    if (!quiet) {
        log.open(logPath);
        if (!log) { std::cerr << "Cannot write log " << logPath << " (does the folder exist?)\n"; return 1; }
    }
    std::ofstream csv;
    if (!csvPath.empty()) {
        std::ifstream exists(csvPath);
        bool header = !exists.good() || exists.peek() == std::ifstream::traits_type::eof();
        csv.open(csvPath, std::ios::app);
        if (header) csv << "policy,trace,tlb_size,references,tlb_hits,tlb_hit_rate,page_faults,"
                           "page_fault_rate,write_backs,context_switches,thread_switches,"
                           "eat_cycles,avg_turnaround,avg_response,jain_fairness\n";
    }

    std::cout << "Trace " << trace << ": " << lines.size() << " references | TLB size " << tlbSize
              << " | frames " << frames << " | page size " << PAGE_SIZE << " B | "
              << OUTER_BITS << "/" << INNER_BITS << "/" << OFFSET_BITS << " address split\n";

    for (SchedulingPolicy pol : policies) {
        auto sched = std::make_unique<Scheduler>(pol, tlbSize, frames, enhancedClock);
        for (auto& t : lines) sched->getProcess(t.pid).addAccess(t.tid, t.addr, t.write);
        for (auto& d : dirs) {
            if (d.kind == "proc") sched->getProcess(d.a).priority = d.b;
            else if (d.kind == "thread") sched->getProcess(d.a).addThread(d.b, d.c);
        }

        std::ofstream tl;
        if (!quiet) {
            tl.open(timelineDir + "/timeline_" + policyName(pol) + "_" + baseName(trace) + ".csv");
            log << "==================== " << policyName(pol) << " | " << trace << " | TLB "
                << tlbSize << " | frames " << frames << " ====================\n";
        }
        sched->run(quiet ? nullptr : &log, quiet ? nullptr : &tl);

        // Summary goes to stdout and the log
        std::streambuf* old = nullptr;
        std::ostringstream summary;
        old = std::cout.rdbuf(summary.rdbuf());
        sched->printStats();
        std::cout.rdbuf(old);
        std::cout << summary.str();
        if (!quiet) {
            log << summary.str();
            sched->printAsciiTimeline(log);
            log << "\n";
        }
        if (csv.is_open()) sched->writeSummaryCSV(csv, baseName(trace));
    }
    if (!quiet) std::cout << "\nLog written to " << logPath << "\n";
    return 0;
}
