#include "TLB.h"
#include <iomanip>
#include <iostream>

TLB::TLB(int size) : currentTime(0), maxSize(size) {}

bool TLB::lookup(int pid, int pageNumber, int& frameNumber) {
    currentTime++;
    for (auto& e : entries) {
        if (e.pid == pid && e.pageNumber == pageNumber) {
            e.lastUsed = currentTime;
            frameNumber = e.frameNumber;
            hitCount++;
            return true;
        }
    }
    missCount++;
    return false;
}

void TLB::insert(int pid, int pageNumber, int frameNumber) {
    if (maxSize <= 0) return;
    currentTime++;
    for (auto& e : entries) {                       // update in place if present
        if (e.pid == pid && e.pageNumber == pageNumber) {
            e.frameNumber = frameNumber;
            e.lastUsed = currentTime;
            return;
        }
    }
    if ((int)entries.size() < maxSize) {
        entries.push_back({pid, pageNumber, frameNumber, currentTime});
        return;
    }
    size_t lru = 0;                                 // evict least recently used
    for (size_t i = 1; i < entries.size(); i++)
        if (entries[i].lastUsed < entries[lru].lastUsed) lru = i;
    entries[lru] = {pid, pageNumber, frameNumber, currentTime};
    evictions++;
}

void TLB::invalidate(int pid, int pageNumber) {
    for (size_t i = 0; i < entries.size(); i++) {
        if (entries[i].pid == pid && entries[i].pageNumber == pageNumber) {
            entries.erase(entries.begin() + i);
            return;
        }
    }
}

void TLB::printStats() const {
    long total = hitCount + missCount;
    double rate = total ? 100.0 * hitCount / total : 0.0;
    std::cout << "TLB (size " << maxSize << "): hits=" << hitCount << " misses=" << missCount
              << " evictions=" << evictions << " hit rate=" << std::fixed
              << std::setprecision(2) << rate << "%\n";
}
