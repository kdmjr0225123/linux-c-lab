#ifndef CONFIG_H
#define CONFIG_H

// 16-bit virtual addresses: [ outer:4 | inner:4 | offset:8 ]
constexpr int ADDRESS_BITS = 16;
constexpr int OFFSET_BITS  = 8;                 // 256-byte pages
constexpr int INNER_BITS   = 4;                 // 16 entries per 2nd-level table
constexpr int OUTER_BITS   = ADDRESS_BITS - OFFSET_BITS - INNER_BITS;  // 4
constexpr int PAGE_SIZE    = 1 << OFFSET_BITS;
constexpr int NUM_PAGES    = 1 << (ADDRESS_BITS - OFFSET_BITS);        // 256 virtual pages / process

constexpr int DEFAULT_FRAMES   = 8;   // physical frames (kept small to force replacement)
constexpr int DEFAULT_TLB_SIZE = 4;
constexpr int RR_QUANTUM       = 4;

// MLFQ: three queues, quantum doubles per level, aging promotes starved processes
constexpr int MLFQ_LEVELS = 3;
constexpr int MLFQ_QUANTA[MLFQ_LEVELS] = {2, 4, 8};
constexpr int AGING_THRESHOLD = 12;   // ticks waiting before promotion

inline int pageOf(unsigned addr)  { return (addr >> OFFSET_BITS) & (NUM_PAGES - 1); }
inline int outerOf(int page)      { return page >> INNER_BITS; }
inline int innerOf(int page)      { return page & ((1 << INNER_BITS) - 1); }

#endif
