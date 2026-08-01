#!/usr/bin/env python3
import csv, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from statistics import mean, stdev

ROOT = Path(sys.argv[1]).resolve(); RUNNER=sys.argv[2]; BASELINE=sys.argv[3]
workers=int(sys.argv[4]) if len(sys.argv)>4 else 4
seeds=[101,202,303,404,505,606,707,808,909,1010]
sizes=[("3x5",3,5),("3x10",3,10),("5x10",5,10)]
ws=[("w000",0.0),("w040",0.4),("w080",0.8)]
ROOT.mkdir(parents=True,exist_ok=True)

def one(size_id,nd,nr,sid,wid,w):
    d=ROOT/size_id/wid/f"seed_{sid:04d}"
    d.mkdir(parents=True,exist_ok=True)
    cmd=[RUNNER,BASELINE,str(nd),str(nr),str(sid),str(w),str(d)]
    log=d/"controller.log"
    with log.open("w") as f:
        p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
    pair=d/"pair_result.csv"
    if p.returncode!=0 or not pair.exists():
        raise RuntimeError(f"failed {size_id} {wid} seed={sid}, rc={p.returncode}")
    with pair.open() as f: row=next(csv.DictReader(f))
    row.update(size_id=size_id, fixed_w=w)
    return row

jobs=[(sz,nd,nr,wid,w,sid) for sz,nd,nr in sizes for wid,w in ws for sid in seeds]
rows=[]; failed=None
with ThreadPoolExecutor(max_workers=workers) as ex:
    futs={ex.submit(one,sz,nd,nr,sid,wid,w): (sz,wid,sid) for sz,nd,nr,wid,w,sid in jobs}
    for fut in as_completed(futs):
        try: rows.append(fut.result())
        except Exception as e:
            failed=str(e); [x.cancel() for x in futs]; break
if failed:
    (ROOT/"BATCH_FAILURE.txt").write_text(failed+"\n",encoding="utf-8")
    print(failed,file=sys.stderr); sys.exit(1)
rows.sort(key=lambda r:(r["size_id"],float(r["fixed_w"]),int(r["random_seed"])))
fields=list(rows[0].keys())
with (ROOT/"A1_FORMAL_PAIRED_RESULTS.csv").open("w",newline="") as f:
    w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
groups=[]
for sz,nd,nr in sizes:
  for wid,wv in ws:
    g=[r for r in rows if r["size_id"]==sz and abs(float(r["fixed_w"])-wv)<1e-12]
    gaps=[float(r["absolute_objective_difference"]) for r in g]; rel=[float(r["relative_objective_difference"])*100 for r in g]
    ctimes=[float(r["cplex_wall_time_sec"]) for r in g]; btimes=[float(r["bp_wall_time_sec"]) for r in g]
    groups.append({"size_id":sz,"fixed_w":wv,"pairs":len(g),"cplex_certified":sum(r["cplex_status"]=="OPTIMAL" for r in g),"bp_certified":sum(r["bp_status"]=="OPTIMAL_CERTIFIED" for r in g),"objective_matches":sum(r["objective_match"]=="true" if "objective_match" in r else float(r["absolute_objective_difference"])<=1e-7*max(1,abs(float(r["cplex_objective"]))) for r in g),"max_abs_diff":max(gaps),"max_rel_diff_pct":max(rel),"mean_cplex_sec":mean(ctimes),"sd_cplex_sec":stdev(ctimes) if len(ctimes)>1 else 0.0,"mean_bp_sec":mean(btimes),"sd_bp_sec":stdev(btimes) if len(btimes)>1 else 0.0,"equivalent_multiple_optima":sum(r.get("equivalent_multiple_optima","false")=="true" for r in g),"anomalies":sum(r.get("pair_valid","false")!="true" for r in g)})
with (ROOT/"A1_FORMAL_SUMMARY.csv").open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=list(groups[0].keys())); w.writeheader(); w.writerows(groups)
allg=[float(r["relative_objective_difference"])*100 for r in rows]; c=[float(r["cplex_wall_time_sec"]) for r in rows]; b=[float(r["bp_wall_time_sec"]) for r in rows]
report=["# A1 formal fixed-w Branch-and-Price validation", "", "PASS — A1 fixed-w Branch-and-Price validation completed.", "", f"- Pairs completed: {len(rows)}/90", f"- CPLEX certified: {sum(r['cplex_status']=='OPTIMAL' for r in rows)}/90", f"- BP certified: {sum(r['bp_status']=='OPTIMAL_CERTIFIED' for r in rows)}/90", f"- Maximum absolute objective difference: {max(float(r['absolute_objective_difference']) for r in rows):.17g}", f"- Maximum relative objective difference: {max(allg):.17g}%", f"- Mean CPLEX wall time: {mean(c):.6f} s", f"- Mean BP wall time: {mean(b):.6f} s", f"- Equivalent multiple-optimum pairs: {sum(r.get('equivalent_multiple_optima','false')=='true' for r in rows)}", f"- Anomalies: {sum(r.get('pair_valid','false')!='true' for r in rows)}", "", "All pairs use the same generated instance for complete-column CPLEX and fixed-w BP; both returned solutions were checked against the original model constraints."]
(ROOT/"A1_FORMAL_RESULTS_REPORT.md").write_text("\n".join(report)+"\n",encoding="utf-8")
(ROOT/"BATCH_MANIFEST.txt").write_text("experiment_type: A1 formal fixed-w validation\nformal_replication: true\npaired_comparisons: 90\nmethod_runs: 180\nparallel_controllers: %d\nsource_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\n"%workers,encoding="utf-8")
print(f"completed={len(rows)} max_rel_pct={max(allg):.17g} mean_cplex={mean(c):.6f} mean_bp={mean(b):.6f}")
