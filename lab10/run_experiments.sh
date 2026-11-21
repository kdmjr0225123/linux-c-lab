#!/usr/bin/env bash
# Builds, regenerates traces, runs every policy on every trace and TLB size,
# writes the main log + timelines, then draws the graphs.
set -e
cd "$(dirname "$0")"
make -s
python3 gen_trace.py
mkdir -p output graphs
rm -f output/results.csv output/clock_compare.txt

# Main run: full log + per-policy timelines on the modified trace (TLB 4, 8 frames)
./paging --policy all --tlb 4 --trace input/trace.txt --log output/output_log.txt --csv output/results.csv

# Pattern runs (timelines only, logs kept separately)
for p in sequential random localized; do
  ./paging --policy all --tlb 4 --trace input/$p.txt --log output/log_$p.txt --csv output/results.csv > /dev/null
done

# TLB size sweep
rm -f output/tlb_sweep.csv
for t in 1 2 4 8 16 32; do
  for p in trace sequential random localized; do
    ./paging --policy all --tlb $t --trace input/$p.txt --csv output/tlb_sweep.csv --quiet > /dev/null
  done
done

# Clock comparison: modified-bit clock vs plain second-chance
for c in enhanced plain; do
  echo "== $c clock ==" >> output/clock_compare.txt
  for p in trace sequential random localized; do
    ./paging --policy all --tlb 4 --clock $c --trace input/$p.txt --quiet | grep -E "Trace|=====|Clock" >> output/clock_compare.txt
  done
done

python3 plotter.py
echo "Done: output/output_log.txt, output/results.csv, output/tlb_sweep.csv, graphs/*.png"
