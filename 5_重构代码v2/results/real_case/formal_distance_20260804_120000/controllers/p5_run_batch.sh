#!/usr/bin/env bash
set -euo pipefail
P="/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/worktrees/real-case-adapter-intracity-carbon/5_重构代码v2"
D="/home/peizeyu2026/smart_wolf_project/datasets/YRD-Steel-v02/data/processed/v0.2.0-candidate"
B=/tmp/real_case_formal_v3
R="$P/results/real_case/formal_distance_20260804_120000"
Q=14442.842886043889
for s in 101 202 303 404 505 606 707 808 909 1010; do
 for d in 0 5 10 15 20; do
  [[ "$s" == 101 && "$d" == 10 ]] && continue
  o="$R/D$(printf '%02d' "$d")/seed_$s"
  mkdir -p "$o"
  "$B" --data-dir "$D" --output-dir "$R/_dry_tmp/D$(printf '%02d' "$d")" --intracity-distance-km "$d" --random-seed 101 --carbon-price 2 --carbon-quota "$Q" --dry-run >/dev/null
  cp "$R/_dry_tmp/D$(printf '%02d' "$d")/solver_arc_parameters.csv" "$o/solver_arc_parameters.csv"
  /usr/bin/time -v "$B" --data-dir "$D" --output-dir "$o" --intracity-distance-km "$d" --random-seed "$s" --carbon-price 2 --carbon-quota "$Q" --formal-run >"$o/run.stdout" 2>"$o/time_verbose.txt"
  cp "$o/time_verbose.txt" "$o/run.stderr"
  echo "COMPLETED distance=$d seed=$s"
 done
done
