#!/usr/bin/env python3
"""Builds input/trace.txt (original sample + sequential/random/localized sections)
and one single-pattern trace per pattern for side-by-side comparison.
Seeded, so every run produces the same files."""
import random

REFS_PER_THREAD = 40
WRITE_PROB = 0.3


def sequential(rng, tid):
    base = 0x0000 if tid == 1 else 0x0800
    return [base + i * 0x40 for i in range(REFS_PER_THREAD)]          # 4 refs per page


def rand_pattern(rng, tid):
    return [rng.randrange(0x0000, 0x2000) for _ in range(REFS_PER_THREAD)]  # 32 pages


def localized(rng, tid):
    hot = [0x0100, 0x0200, 0x0300] if tid == 1 else [0x0A00, 0x0B00, 0x0C00]
    out = []
    for i in range(REFS_PER_THREAD):
        if rng.random() < 0.85:                      # 85% inside a 3-page working set
            out.append(rng.choice(hot) + (i * 4) % 0x100)
        else:
            out.append(rng.randrange(0x0000, 0x2000))
    return out


def lines_for(pid, gen, rng):
    t1, t2 = gen(rng, 1), gen(rng, 2)
    out = []
    for a, b in zip(t1, t2):                         # threads interleave in the trace
        for tid, addr in ((1, a), (2, b)):
            rw = "W" if rng.random() < WRITE_PROB else "R"
            out.append(f"{pid} {tid} 0x{addr:04X} {rw}")
    return out


HEADER = ["# Format: <pid> <thread_id> <address> <access_type> (R=read, W=write)",
          "# '#!' lines are optional priorities (lower = higher priority):",
          "#!   proc <pid> <priority>     thread <pid> <tid> <priority>",
          "#! proc 1 2", "#! proc 2 3", "#! proc 3 1",
          "#! thread 1 1 1", "#! thread 1 2 2",
          "#! thread 2 1 2", "#! thread 2 2 1",
          "#! thread 3 1 1", "#! thread 3 2 1"]

ORIGINAL = ["1 1 0x0043 R", "1 2 0x00A1 W", "2 1 0x0210 R", "3 1 0x0F0F R",
            "1 1 0x00B0 W", "2 2 0x0213 R", "3 2 0x0100 W"]


def write(path, body):
    with open(path, "w") as f:
        f.write("\n".join(HEADER + body) + "\n")


rng = random.Random(375)
combined = ["# --- original sample ---"] + ORIGINAL
combined += ["# --- P1: sequential sweep ---"] + lines_for(1, sequential, rng)
combined += ["# --- P2: random ---"] + lines_for(2, rand_pattern, rng)
combined += ["# --- P3: localized (hot working set) ---"] + lines_for(3, localized, rng)
write("input/trace.txt", combined)

for name, gen in (("sequential", sequential), ("random", rand_pattern), ("localized", localized)):
    rng = random.Random(375)
    body = []
    for pid in (1, 2, 3):
        body += lines_for(pid, gen, rng)
    write(f"input/{name}.txt", body)

print("wrote input/trace.txt, input/sequential.txt, input/random.txt, input/localized.txt")
