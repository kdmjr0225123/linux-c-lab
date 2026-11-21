#ifndef TWOLEVELPAGETABLE_H
#define TWOLEVELPAGETABLE_H

#include <map>
#include <memory>
#include <set>
#include <utility>

// Second-level table: inner page number -> frame number
using SecondLevelTable = std::map<int, int>;

class TwoLevelPageTable {
public:
    explicit TwoLevelPageTable(int pageCount);

    bool isInMemory(int pid, int pageNum) const;
    int  getFrame(int pid, int pageNum) const;     // -1 if not resident
    void loadPage(int pid, int pageNum, int frameNum);
    void unloadPage(int pid, int pageNum);          // victim of replacement
    void printStats() const;

    long faults() const { return pageFaults; }
    void recordFault() { pageFaults++; }
private:
    // (pid, outer page) -> second-level table (created on first fault in that region)
    std::map<std::pair<int, int>, std::shared_ptr<SecondLevelTable>> firstLevel;
    std::set<std::pair<int, int>> memoryPages;      // resident (pid, page)
    int pageCount;
    long pageFaults = 0, loads = 0, unloads = 0;
};

#endif
