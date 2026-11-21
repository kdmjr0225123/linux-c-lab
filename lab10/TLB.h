#ifndef TLB_H
#define TLB_H

#include <vector>

struct TLBEntry {
    int pid;
    int pageNumber;
    int frameNumber;
    long lastUsed;
};

class TLB {
public:
    explicit TLB(int size);
    bool lookup(int pid, int pageNumber, int& frameNumber);
    void insert(int pid, int pageNumber, int frameNumber);
    void invalidate(int pid, int pageNumber);     // called when a page is evicted
    void printStats() const;

    long hits() const { return hitCount; }
    long misses() const { return missCount; }
    int size() const { return maxSize; }
private:
    std::vector<TLBEntry> entries;
    long currentTime;
    int maxSize;
    long hitCount = 0, missCount = 0, evictions = 0;
};

#endif
