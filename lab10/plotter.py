#!/usr/bin/env python3
"""Timeline + summary graphs from the simulator's CSV output."""
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

POLICIES = ["FCFS", "RR", "Priority", "MLFQ"]
os.makedirs("graphs", exist_ok=True)
plt.rcParams.update({"font.family": "serif", "font.size": 9})


def read(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def timeline(policy, trace="trace"):
    path = f"output/timeline_{policy}_{trace}.csv"
    if not os.path.exists(path):
        return
    rows = read(path)
    threads = sorted({(int(r["pid"]), int(r["tid"])) for r in rows})
    ypos = {k: i for i, k in enumerate(threads)}
    fig, ax = plt.subplots(figsize=(11, 3.6))
    for r in rows:
        t, y = int(r["tick"]), ypos[(int(r["pid"]), int(r["tid"]))]
        ax.broken_barh([(t, 1)], (y - 0.35, 0.7), facecolors="0.75")      # CPU burst
        if r["page_fault"] == "1":
            ax.plot(t + 0.5, y, marker="x", color="black", ms=4)
        elif r["tlb_hit"] == "1":
            ax.plot(t + 0.5, y, marker=".", color="black", ms=2)
        else:
            ax.plot(t + 0.5, y, marker="o", mfc="none", color="black", ms=3)
        if r["context_switch"] == "1":
            ax.axvline(t, color="black", lw=0.6)
        elif r["thread_switch"] == "1":
            ax.axvline(t, color="black", lw=0.4, ls=":")
    ax.set_yticks(range(len(threads)))
    ax.set_yticklabels([f"P{p} T{t}" for p, t in threads])
    ax.set_xlabel("tick")
    ax.set_xlim(0, len(rows))
    ax.set_title(f"{policy} timeline ({trace}): bars = CPU burst, x = page fault, "
                 f"o = TLB miss, . = TLB hit, solid line = context switch, dotted = thread switch")
    fig.tight_layout()
    fig.savefig(f"graphs/timeline_{policy}_{trace}.png", dpi=150)
    plt.close(fig)


def bars(results):
    traces = ["trace", "sequential", "random", "localized"]
    metrics = [("tlb_hit_rate", "TLB hit rate (%)"), ("page_fault_rate", "Page fault rate (%)"),
               ("context_switches", "Context switches"), ("thread_switches", "Thread switches")]
    hatches = ["", "//", "..", "xx"]
    fig, axes = plt.subplots(2, 2, figsize=(10, 6))
    for ax, (key, label) in zip(axes.flat, metrics):
        w = 0.2
        for i, pol in enumerate(POLICIES):
            vals = [float(next(r[key] for r in results if r["policy"] == pol and r["trace"] == t))
                    for t in traces]
            ax.bar([x + i * w for x in range(len(traces))], vals, w, label=pol,
                   color="white", edgecolor="black", hatch=hatches[i])
        ax.set_xticks([x + 1.5 * w for x in range(len(traces))])
        ax.set_xticklabels(["mixed", "sequential", "random", "localized"])
        ax.set_title(label)
    h, l = axes[0, 0].get_legend_handles_labels()
    fig.legend(h, l, loc="upper center", ncol=4, fontsize=8)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig("graphs/policy_comparison.png", dpi=150)
    plt.close(fig)


def tlb_sweep(rows):
    styles = {"trace": "-o", "sequential": "-s", "random": "-^", "localized": "-d"}
    fig, ax = plt.subplots(figsize=(6, 3.8))
    for trace, st in styles.items():
        pts = sorted((int(r["tlb_size"]), float(r["tlb_hit_rate"])) for r in rows
                     if r["trace"] == trace and r["policy"] == "FCFS")
        ax.plot([p[0] for p in pts], [p[1] for p in pts], st, color="black", ms=4,
                label="mixed" if trace == "trace" else trace)
    ax.set_xscale("log", base=2)
    ax.set_xlabel("TLB entries")
    ax.set_ylabel("TLB hit rate (%)")
    ax.set_title("TLB size vs hit rate (FCFS, 8 frames)")
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig("graphs/tlb_size_sweep.png", dpi=150)
    plt.close(fig)


for pol in POLICIES:
    timeline(pol)
bars(read("output/results.csv"))
tlb_sweep(read("output/tlb_sweep.csv"))
print("graphs written to graphs/")
