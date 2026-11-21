#ifndef CLOCK_H
#define CLOCK_H

#include <utility>
#include <vector>

struct FrameEntry {
    int pid = -1;            // -1 = free frame
    int pageNumber = -1;
    bool referenced = false;
    bool modified = false;
};

class Clock {
public:
    explicit Clock(int frameCount, bool useModifiedBit = true);

    // Picks a frame for (pid, pageNum). If a resident page is evicted, evictedPage is set
    // to its (pid, page) and isModified tells the caller a write-back is needed.
    int evictAndReplace(int pid, int pageNum, std::pair<int, int>& evictedPage, bool& isModified);
    void touch(int frame, bool isWrite);             // set R (and M on writes) on every access
    void printStats() const;

    long writeBacks() const { return dirtyEvictions; }
private:
    std::vector<FrameEntry> frames;
    size_t clockHand;
    bool enhanced;                                   // false = plain second-chance baseline
    long cleanEvictions = 0, dirtyEvictions = 0, handMoves = 0;
};

#endif
