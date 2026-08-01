#!/usr/bin/env python3
import csv,sys
from concurrent.futures import ThreadPoolExecutor,as_completed
from pathlib import Path
from statistics import mean,stdev
root=Path(sys.argv[1]); runner=sys.argv[2]; baseline=sys.argv[3]; workers=int(sys.argv[4])
seeds=[101,202,303,404,505,606,707,808,909,1010]; sizes=[("5x14",5,14),("5x16",5,16),("5x18",5,18)]; ws=[("w000",0.0),("w040",0.4),("w080",0.8)]
root.mkdir(parents=True,exist_ok=True)
def run(x):
 sz,nd,nr,wid,w,sid=x; d=root/sz/wid/f"seed_{sid:04d}"; d.mkdir(parents=True,exist_ok=True)
 import subprocess
 with (d/'controller.log').open('w') as f: rc=subprocess.run([runner,baseline,str(nd),str(nr),str(sid),str(w),str(d)],stdout=f,stderr=subprocess.STDOUT).returncode
 if rc or not (d/'pair_result.csv').exists(): raise RuntimeError(f"failed {sz} {wid} {sid}")
 row=next(csv.DictReader((d/'pair_result.csv').open())); row.update(size_id=sz,fixed_w=w); return row
jobs=[(sz,nd,nr,wid,w,sid) for sz,nd,nr in sizes for wid,w in ws for sid in seeds]; rows=[]
with ThreadPoolExecutor(max_workers=workers) as ex:
 fs={ex.submit(run,j):j for j in jobs}
 for f in as_completed(fs): rows.append(f.result())
rows.sort(key=lambda r:(r['size_id'],float(r['fixed_w']),int(r['random_seed']))); fields=list(rows[0])
with (root/'A1_EXTENSION_PAIRED_RESULTS.csv').open('w',newline='') as f: w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
groups=[]
for sz,nd,nr in sizes:
 for wid,wv in ws:
  g=[r for r in rows if r['size_id']==sz and abs(float(r['fixed_w'])-wv)<1e-12]; c=[float(r['reference_total_wall_time_sec']) for r in g]; b=[float(r['bp_total_wall_time_sec']) for r in g]; rel=[float(r['relative_objective_difference'])*100 for r in g]
  groups.append({'size_id':sz,'num_dc':nd,'num_retailers':nr,'fixed_w':wv,'paired_instances':len(g),'complete_column_count':g[0]['complete_column_count'],'cplex_certified':sum(r['cplex_status']=='OPTIMAL' for r in g),'bp_certified':sum(r['bp_status']=='OPTIMAL_CERTIFIED' for r in g),'objective_matches':sum(r['pair_valid']=='true' for r in g),'max_abs_diff':max(float(r['absolute_objective_difference']) for r in g),'max_rel_diff_pct':max(rel),'mean_reference_total_wall_time_sec':mean(c),'sd_reference_total_wall_time_sec':stdev(c),'max_reference_total_wall_time_sec':max(c),'mean_bp_total_wall_time_sec':mean(b),'sd_bp_total_wall_time_sec':stdev(b),'max_bp_total_wall_time_sec':max(b),'mean_time_ratio_cplex_over_bp':mean(x/y for x,y in zip(c,b)),'median_time_ratio_cplex_over_bp':sorted(x/y for x,y in zip(c,b))[len(g)//2],'mean_bp_generated_columns':mean(float(r['bp_generated_columns']) for r in g),'mean_reference_peak_rss_mb':mean(float(r['reference_peak_rss_kb'])/1024 for r in g),'mean_bp_peak_rss_mb':mean(float(r['bp_peak_rss_kb'])/1024 for r in g),'anomalies':sum(r['pair_valid']!='true' for r in g)})
with (root/'A1_EXTENSION_SUMMARY_BY_SIZE.csv').open('w',newline='') as f: w=csv.DictWriter(f,fieldnames=list(groups[0])); w.writeheader(); w.writerows(groups)
(root/'BATCH_MANIFEST.txt').write_text('experiment_type: A1 formal scale extension\nformal_replication: true\nadded_pairs: 90\nparallel_controllers: 1\nparallel_pricing: false\ncplex_threads: 1\n',encoding='utf-8')
print('completed',len(rows))
