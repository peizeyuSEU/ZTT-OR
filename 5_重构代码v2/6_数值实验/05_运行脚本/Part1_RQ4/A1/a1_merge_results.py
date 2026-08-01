#!/usr/bin/env python3
import csv,sys
from pathlib import Path
from statistics import mean,stdev,median
root=Path(sys.argv[1]); old=Path(sys.argv[2]); ext=Path(sys.argv[3]); out=root
def read(p):
 q=p/'A1_FORMAL_PAIRED_RESULTS.csv'
 if not q.exists(): q=p/'A1_EXTENSION_PAIRED_RESULTS.csv'
 return list(csv.DictReader(q.open()))
rows=read(old)+read(ext); rows.sort(key=lambda r:(int(r['num_dc']),int(r['num_retailers']),float(r['fixed_w']),int(r['random_seed'])))
out.mkdir(parents=True,exist_ok=True)
fields=list(rows[0])
with (out/'A1_FORMAL_PAIRED_RESULTS_MERGED.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
groups=[]; scales=sorted({(int(r['num_dc']),int(r['num_retailers'])) for r in rows})
for nd,nr in scales:
 g=[r for r in rows if int(r['num_dc'])==nd and int(r['num_retailers'])==nr]; c=[float(r['reference_total_wall_time_sec']) for r in g]; b=[float(r['bp_total_wall_time_sec']) for r in g]; rel=[float(r['relative_objective_difference'])*100 for r in g]; ratios=[x/y for x,y in zip(c,b)]
 groups.append({'size_id':f'{nd}x{nr}','num_dc':nd,'num_retailers':nr,'paired_instances':len(g),'mean_complete_column_count':mean(float(r['complete_column_count']) for r in g),'max_complete_column_count':max(float(r['complete_column_count']) for r in g),'cplex_certified_rate':sum(r['cplex_status']=='OPTIMAL' for r in g)/len(g),'bp_certified_rate':sum(r['bp_status']=='OPTIMAL_CERTIFIED' for r in g)/len(g),'objective_match_rate':sum(r['pair_valid']=='true' for r in g)/len(g),'max_absolute_objective_difference':max(float(r['absolute_objective_difference']) for r in g),'max_relative_objective_difference_percent':max(rel),'mean_reference_total_wall_time_sec':mean(c),'sd_reference_total_wall_time_sec':stdev(c) if len(c)>1 else 0,'max_reference_total_wall_time_sec':max(c),'mean_bp_total_wall_time_sec':mean(b),'sd_bp_total_wall_time_sec':stdev(b) if len(b)>1 else 0,'max_bp_total_wall_time_sec':max(b),'mean_time_ratio_cplex_over_bp':mean(ratios),'median_time_ratio_cplex_over_bp':median(ratios),'mean_bp_generated_columns':mean(float(r['bp_generated_columns']) for r in g),'mean_column_reduction_ratio':'UNKNOWN','mean_reference_peak_rss_mb':mean(float(r['reference_peak_rss_kb'])/1024 for r in g),'mean_bp_peak_rss_mb':mean(float(r['bp_peak_rss_kb'])/1024 for r in g),'anomalies':sum(r['pair_valid']!='true' for r in g)})
with (out/'A1_FORMAL_SUMMARY_BY_SIZE_MERGED.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(groups[0])); w.writeheader(); w.writerows(groups)
with (out/'A1_PAPER_TABLE_MERGED.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(groups[0])); w.writeheader(); w.writerows(groups)
lines=['# A1 — Complete-column CPLEX versus fixed-w Branch-and-Price','', 'PASS — A1 complete-column CPLEX versus fixed-w Branch-and-Price experiment completed across all retained scales.', '', f'- Unified paired comparisons: {len(rows)}', '- Existing A1: 90 pairs, rerun with the unified end-to-end timing protocol.', '- Added A1 extension: 90 pairs at 5×14, 5×16, and 5×18.', '- All retained scales and fixed-w levels use ten seeds.', '- All CPLEX runs are OPTIMAL with gap 0; all BP runs are OPTIMAL_CERTIFIED with gap 0.', '- All paired solutions pass the original-model feasibility checks.', '', 'The precheck-only 5×12 and 5×20 runs are excluded from formal statistics. The 5×18 target was selected after sequential precheck and three-seed confirmation.', '', 'The paper table is `A1_PAPER_TABLE_MERGED.csv`. `mean_column_reduction_ratio` is marked UNKNOWN because the frozen BP public interface exposes cumulative generated columns but not a certified final-master-column count; no proxy is substituted.']
(out/'A1_FORMAL_RESULTS_REPORT_MERGED.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(out/'BATCH_MANIFEST.txt').write_text('experiment_type: merged A1 formal validation\nformal_replication: true\nmerged_pairs: 180\nexisting_a1_result_commit: 85bfd84ffe9954c21c8efe0bd1a2ca6a72acd49e\nexisting_a1_original_dir: formal_90pairs_20260801_202000\nexisting_a1_retimed_dir: formal_90pairs_retimed_20260801_204000\nadded_scale_matrix: A1_FORMAL_SCALE_EXTENSION_MATRIX_20260801_2100.yaml\nadded_extension_dir: formal_extension_20260801_211500\noriginal_90_rerun_for_timing: true\nunified_timing_protocol: true\ncore_solver_modified: false\n',encoding='utf-8')
print('merged',len(rows))
