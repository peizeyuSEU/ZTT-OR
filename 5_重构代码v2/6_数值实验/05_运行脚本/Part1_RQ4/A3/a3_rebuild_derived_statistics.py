from pathlib import Path
import csv
ROOT=next(Path.cwd().glob("5_*"))
RES=ROOT/"results/paper_experiments/part1_algorithm_validation/A3_ga_bp_scalability/formal_100runs_resumable_20260802_000000"
def main():
 q=list(csv.DictReader((RES/"A3_RUN_QUEUE.csv").open()))
 if len(q)!=100: raise ValueError("queue must contain 100 tasks")
 rows=list(csv.DictReader((RES/"A3_FORMAL_RUN_RESULTS.csv").open()))
 for r in rows:
  req=int(float(r["fitness_requests"])); hits=int(float(r["actual_fitness_cache_hits"]))
  r["cache_hit_rate"]="NA_ZERO_REQUESTS" if req==0 else hits/req
  r["cache_hit_percent"]="" if req==0 else 100*hits/req
 with (RES/"A3_FORMAL_RUN_RESULTS.csv").open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=rows[0].keys()); w.writeheader(); w.writerows(rows)
 print("A3 derived cache columns rebuilt")
if __name__=="__main__": main()

