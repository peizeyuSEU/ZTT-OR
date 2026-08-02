from pathlib import Path
import csv,sys
ROOT=next(Path.cwd().glob("5_*"))
RES=ROOT/"results/paper_experiments/part1_algorithm_validation/A3_ga_bp_scalability/formal_100runs_resumable_20260802_000000"
def read(n): return list(csv.DictReader((RES/n).open()))
e=[]; runs=read("A3_FORMAL_RUN_RESULTS.csv")
if len(runs)!=100:e.append("run rows")
keys=[(r["size_id"],r["random_seed"]) for r in runs]
if len(set(keys))!=100:e.append("duplicate keys")
summ=read("A3_FORMAL_SUMMARY_BY_SIZE.csv")
if len(summ)!=10 or any(int(float(r["n"]))!=10 for r in summ):e.append("summary")
for r in runs:
 try:
  rate=float(r["cache_hit_rate"])
  if not 0<=rate<=1:e.append("rate")
 except: 
  if r["cache_hit_rate"]!="NA_ZERO_REQUESTS":e.append("rate marker")
 if int(float(r["fitness_requests"]))!=int(float(r["fitness_bp_solves"]))+int(float(r["actual_fitness_cache_hits"])):e.append("request accounting")
 if r["inner_bp_status"]!="OPTIMAL":e.append("inner status")
 if str(r["has_integer_solution"]) not in ("1","True","true"):e.append("integer")
ba=read("A3_BEFORE_AFTER_DERIVED_FIX_COMPARISON.csv")
if len(ba)!=100:e.append("audit rows")
if any(r["profit_difference"] not in ("0","0.0") or r["emission_difference"] not in ("0","0.0") or r["investment_difference"] not in ("0","0.0") for r in ba):e.append("core diffs")
if sum(r["best_w_exact_match"]=="True" for r in ba)!=100:e.append("w")
ia=read("A3_INTERRUPTED_RETRY_AUDIT.csv")
if not ia or ia[0]["clean_rerun_attempt"]!="attempt_05" or ia[0]["interrupted_attempt_in_formal_statistics"].lower()!="false":e.append("seed808")
if e:
 print("VALIDATION_FAILED",sorted(set(e)));sys.exit(1)
print("VALIDATION_PASS: runs=100 unique; summaries=10x10; inner_optimal=100; integer=100; core_diffs=0; seed808=attempt05")

