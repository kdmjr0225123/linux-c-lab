#include "TwoLevelPageTable.h"
#include "Config.h"
#include <iostream>

TwoLevelPageTable::TwoLevelPageTable(int pageCount) : pageCount(pageCount) {}

bool TwoLevelPageTable::isInMemory(int pid, int pageNum) const {
    return memoryPages.count({pid, pageNum}) > 0;
}

int TwoLevelPageTable::getFrame(int pid, int pageNum) const {
    auto outer = firstLevel.find({pid, outerOf(pageNum)});
    if (outer == firstLevel.end()) return -1;              // no 2nd-level table yet
    auto inner = outer->second->find(innerOf(pageNum));
    return inner == outer->second->end() ? -1 : inner->second;
}

void TwoLevelPageTable::loadPage(int pid, int pageNum, int frameNum) {
    auto key = std::make_pair(pid, outerOf(pageNum));
    auto& table = firstLevel[key];                         // update level 1
    if (!table) table = std::make_shared<SecondLevelTable>();
    (*table)[innerOf(pageNum)] = frameNum;                 // update level 2
    memoryPages.insert({pid, pageNum});
    loads++;
}

void TwoLevelPageTable::unloadPage(int pid, int pageNum) {
    auto outer = firstLevel.find({pid, outerOf(pageNum)});
    if (outer != firstLevel.end()) {
        outer->second->erase(innerOf(pageNum));
        if (outer->second->empty()) firstLevel.erase(outer); // free empty 2nd-level table
    }
    memoryPages.erase({pid, pageNum});
    unloads++;
}

void TwoLevelPageTable::printStats() const {
    std::cout << "Page table: faults=" << pageFaults << " loads=" << loads
              << " evictions=" << unloads << " resident pages=" << memoryPages.size()
              << " live 2nd-level tables=" << firstLevel.size()
              << " (" << OUTER_BITS << " outer bits / " << INNER_BITS << " inner bits)\n";
}
