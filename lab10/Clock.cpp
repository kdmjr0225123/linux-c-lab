#include "Clock.h"
#include <iostream>

Clock::Clock(int frameCount, bool useModifiedBit)
    : frames(frameCount), clockHand(0), enhanced(useModifiedBit) {}

void Clock::touch(int frame, bool isWrite) {
    frames[frame].referenced = true;
    if (isWrite) frames[frame].modified = true;
}

// Enhanced (two-bit) clock. Classes by (R,M):
//   (0,0) best victim  (0,1) needs write-back  (1,0) recently used  (1,1) worst
// Pass A: sweep for (0,0) without touching bits.
// Pass B: sweep for (0,1), clearing R on every frame passed.
// Repeat; after one A+B round every R is 0, so the next round must succeed.
int Clock::evictAndReplace(int pid, int pageNum, std::pair<int, int>& evictedPage, bool& isModified) {
    evictedPage = {-1, -1};
    isModified = false;
    size_t n = frames.size();

    for (size_t i = 0; i < n; i++) {                       // free frame first
        if (frames[i].pid == -1) {
            frames[i] = {pid, pageNum, true, false};
            return (int)i;
        }
    }

    int victim = -1;
    if (!enhanced) {                                       // baseline: ignore M bit
        while (frames[clockHand].referenced) {
            frames[clockHand].referenced = false;
            clockHand = (clockHand + 1) % n; handMoves++;
        }
        victim = (int)clockHand;
    }
    for (int round = 0; round < 2 && victim < 0; round++) {
        for (size_t k = 0; k < n; k++) {                   // pass A
            FrameEntry& f = frames[clockHand];
            if (!f.referenced && !f.modified) { victim = (int)clockHand; break; }
            clockHand = (clockHand + 1) % n; handMoves++;
        }
        if (victim >= 0) break;
        for (size_t k = 0; k < n; k++) {                   // pass B
            FrameEntry& f = frames[clockHand];
            if (!f.referenced && f.modified) { victim = (int)clockHand; break; }
            f.referenced = false;
            clockHand = (clockHand + 1) % n; handMoves++;
        }
    }

    FrameEntry& f = frames[victim];
    evictedPage = {f.pid, f.pageNumber};
    isModified = f.modified;
    if (isModified) dirtyEvictions++; else cleanEvictions++;
    f = {pid, pageNum, true, false};
    clockHand = (victim + 1) % n;
    return victim;
}

void Clock::printStats() const {
    std::cout << "Clock (" << frames.size() << " frames): clean evictions=" << cleanEvictions
              << " dirty evictions (write-backs)=" << dirtyEvictions
              << " hand moves=" << handMoves << "\n";
}
