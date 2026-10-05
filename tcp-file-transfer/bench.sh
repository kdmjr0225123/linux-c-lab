#!/usr/bin/env bash
# Performance experiment: sequential server vs thread pool server.
# Usage: ./bench.sh [num_clients] [file_size_MB] [threads]
# Defaults: 10 clients, 50 MB file, 4 threads.
set -u
cd "$(dirname "$0")"
ROOT=$(pwd)
N=${1:-10}
MB=${2:-50}
THREADS=${3:-4}

make -s all || exit 1
mkdir -p bench_run && cd bench_run
rm -rf out_* uploads *.log
[ -f file.txt ] && [ "$(stat -c %s file.txt)" -eq $((MB * 1048576)) ] || head -c $((MB * 1048576)) /dev/urandom > file.txt

TICK=$(getconf CLK_TCK)
PAGE_KB=$(( $(getconf PAGESIZE) / 1024 ))

run() {  # $1 = label, $2.. = server command
    local label=$1; shift
    "$@" > "server_${label}.log" 2>&1 &
    local spid=$!
    sleep 0.5
    # CPU ticks (utime + stime) before the burst
    read -r u0 s0 < <(awk '{print $14, $15}' /proc/$spid/stat)

    local t0=$(date +%s.%N)
    local pids=()
    for i in $(seq 1 "$N"); do
        "$ROOT/$CLIENT" file.txt "out_${label}_$i" > "client_${label}_$i.log" 2>&1 &
        pids+=($!)
    done
    wait "${pids[@]}"
    local t1=$(date +%s.%N)

    read -r u1 s1 < <(awk '{print $14, $15}' /proc/$spid/stat)
    local hwm=$(awk '/VmHWM/ {print $2}' /proc/$spid/status)
    local thr=$(awk '/Threads/ {print $2}' /proc/$spid/status)
    kill -INT $spid; wait $spid 2>/dev/null

    local wall=$(echo "$t1 - $t0" | bc -l)
    local cpu_s=$(echo "($u1 + $s1 - $u0 - $s0) / $TICK" | bc -l)
    local cpu_pct=$(echo "100 * $cpu_s / $wall" | bc -l)
    local ok=$(grep -h "^RESULT ok=1" client_${label}_*.log | wc -l)
    local stats=$(grep -h "^RESULT ok=1" client_${label}_*.log | sed -E 's/.*wait_ms=([0-9.]+).*total_ms=([0-9.]+)/\1 \2/' |
        awk '{w+=$1; t+=$2; if($1>mw)mw=$1; if($2>mt)mt=$2} END {printf "%.0f %.0f %.0f", w/NR, t/NR, mt}')
    read -r avg_wait avg_total max_total <<< "$stats"
    printf "| %-22s | %7.2f | %8s | %8s | %8s | %6.1f%% | %7.2f | %8s | %4s | %5s |\n" \
        "$label" "$wall" "$avg_wait" "$avg_total" "$max_total" "$cpu_pct" "$cpu_s" "$((hwm / 1024)).$(( (hwm % 1024) * 10 / 1024 ))" "$thr" "$ok/$N"
}

echo
echo "Clients: $N concurrent | File: ${MB} MB | CPU cores: $(nproc)"
echo
echo "| Server                 | Total s | Avg wait | Avg resp | Max resp | CPU %   | CPU s   | Peak MB  | Thr  | OK    |"
echo "|------------------------|---------|----------|----------|----------|---------|---------|----------|------|-------|"
CLIENT=version1/client             run "v1_sequential"       "$ROOT/version1/server"
sleep 1
CLIENT=version2_threadpool/client  run "v2_pool_${THREADS}_threads" "$ROOT/version2_threadpool/server" "$THREADS"
echo
echo "Wait/resp columns are per-client milliseconds. Logs are in bench_run/."
