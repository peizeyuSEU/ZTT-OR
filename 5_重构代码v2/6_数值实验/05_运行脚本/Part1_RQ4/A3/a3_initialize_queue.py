#!/usr/bin/env python3
import csv, json, sys
from pathlib import Path

SIZES = [("3x5",3,5),("3x10",3,10),("5x10",5,10),("5x20",5,20),
         ("5x30",5,30),("10x30",10,30),("15x30",15,30),("15x50",15,50),
         ("20x50",20,50),("30x100",30,100)]
SEEDS = [101,202,303,404,505,606,707,808,909,1010]
FIELDS = ["order_id","size_id","num_dc","num_retailers","random_seed","status","attempt",
          "started_at","finished_at","controller_session","process_id","exit_code",
          "output_directory","validation_status","completion_marker","termination_reason","notes"]

def main():
    if len(sys.argv) != 2: raise SystemExit("usage: a3_initialize_queue.py BATCH_DIR")
    batch = Path(sys.argv[1]).resolve()
    if batch.exists() and any(batch.iterdir()): raise SystemExit(f"batch directory is not empty: {batch}")
    (batch/"progress").mkdir(parents=True, exist_ok=True); (batch/"runs").mkdir()
    rows=[]; order=1
    for sid,nd,nr in SIZES:
        for seed in SEEDS:
            rows.append({"order_id":order,"size_id":sid,"num_dc":nd,"num_retailers":nr,"random_seed":seed,
                         "status":"PENDING","attempt":0,"started_at":"","finished_at":"",
                         "controller_session":"","process_id":"","exit_code":"","output_directory":str(batch/f"runs/{sid}/seed_{seed}"),
                         "validation_status":"","completion_marker":"","termination_reason":"","notes":""})
            order += 1
    with (batch/"A3_RUN_QUEUE.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=FIELDS); w.writeheader(); w.writerows(rows)
    (batch/"A3_BATCH_STATE.json").write_text(json.dumps({"batch_status":"INITIALIZED","completed_count":0,"pending_count":100,"running_count":0,"failed_count":0,"interrupted_count":0},indent=2)+"\n")
    (batch/"BATCH_MANIFEST.txt").write_text("experiment: Part 1 / RQ4 / A3 GA-BP scalability\nformal_replication: true\nqueue_tasks: 100\nformal_scales: 3x5,3x10,5x10,5x20,5x30,10x30,15x30,15x50,20x50,30x100\nseeds: 101,202,303,404,505,606,707,808,909,1010\nchromosome_length: 10\npopulation_size: 30\nmax_generation: 50\nouter_workers: 8\ncplex_threads: 1\nparallel_pricing: false\nsource_algorithm_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\nprecheck_30x100_not_in_formal_statistics: true\n")
    print(f"initialized 100 tasks at {batch}")
if __name__ == "__main__": main()
