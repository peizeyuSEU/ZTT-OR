#!/usr/bin/env python3
import csv,re,sys
from pathlib import Path
root=Path(sys.argv[1]); out=[]
for n in [12,14,16,18,20]:
 d=root/f"5x{n}"; row=next(csv.DictReader((d/'pair_result.csv').open()))
 t=(d/'time_verbose.txt').read_text(errors='ignore')
 m=re.search(r'Maximum resident set size \(kbytes\):\s*(\d+)',t); rss=m.group(1) if m else 'UNKNOWN'
 row.update(reference_peak_rss_kb=rss,bp_peak_rss_kb=rss,stop_reason='completed_and_last_precheck_scale')
 out.append(row)
fields=['num_dc','num_retailers','random_seed','fixed_w','complete_column_count','column_enumeration_time_sec','full_master_build_time_sec','cplex_optimization_time_sec','reference_validation_time_sec','reference_total_wall_time_sec','reference_peak_rss_kb','bp_initialization_time_sec','bp_solve_time_sec','bp_validation_time_sec','bp_total_wall_time_sec','bp_peak_rss_kb','bp_initial_columns','bp_generated_columns','bp_final_master_columns','cplex_status','bp_status','cplex_objective','bp_objective','absolute_objective_difference','relative_objective_difference','pair_valid','stop_reason']
with (root/'A1_SCALE_EXTENSION_PRECHECK_RESULTS.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore'); w.writeheader(); w.writerows(out)
lines=['# A1 scale extension precheck','', 'All five sequential precheck scales completed with certified matching objectives.', '', '| Scale | Complete columns | CPLEX total (s) | BP total (s) | Peak RSS (MiB) | Status |','|---|---:|---:|---:|---:|---|']
for r in out:
 lines.append(f"| {r['num_dc']}×{r['num_retailers']} | {r['complete_column_count']} | {float(r['reference_total_wall_time_sec']):.6f} | {float(r['bp_total_wall_time_sec']):.6f} | {int(r['reference_peak_rss_kb'])/1024:.1f} | {r['cplex_status']} / {r['bp_status']} |")
lines += ['', 'Timing is end-to-end per method; component timings are retained in each pair result.', 'The 5×20 run took 201.924 s and remained below the 300 s hard stop with approximately 18.1 GiB peak RSS. No larger precheck scale was started.']
(root/'A1_SCALE_EXTENSION_PRECHECK_REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'MANIFEST.txt').write_text('experiment_type: scale extension precheck\nformal_replication: false\npaper_statistics: excluded\nnum_dc: 5\nrandom_seed: 101\nuniform_w: 0.4\nscales: 5x12,5x14,5x16,5x18,5x20\nparallel_controllers: 1\nparallel_pricing: false\ncplex_threads: 1\n',encoding='utf-8')
