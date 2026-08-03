import csv,datetime,fcntl,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
CODE=Path(__file__).resolve().parents[3];LOCK=CODE/"results/paper_experiments/.experiment_resource.lock";TEMPLATE=CODE/"results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml";COMMIT="0dd725354732f9b8011e9e7e8540e0e64a3ff223";TAG="paper-exp-baseline-20260801";SEEDS=[101,505,909];PRICES=[0,1,2,4,6,8,10,12,16,20];CAPS=[0,40000,100000,200000,350000]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def scen(p,c):return f"PRICE_{int(p*100):04d}_CAP_{int(c):06d}"
def cfg(p):
 d={}
 for l in Path(p).read_text().splitlines():
  if ":" in l:k,v=l.split(":",1);d[k.strip()]=v.strip().strip("'\"")
 return d
def writecfg(p,s,pr,ca,out):
 rep={"num_dc":"10","num_retailers":"30","random_seed":str(s),"carbon_price":str(float(pr)),"carbon_cap":str(float(ca)),"delta":"3000.0","investment_exponent":"0.5","output_dir":str(out),"run_mode":"ga"};a=[]
 for l in TEMPLATE.read_text().splitlines():
  k=l.split(":",1)[0].strip() if ":" in l else "";a.append(k+": "+rep[k] if k in rep else l)
 Path(p).write_text("\n".join(a)+"\n")
def archive(run):
 src=CODE/"results/single";ds=[p for p in src.iterdir() if p.is_dir() and (p/"result.csv").exists()] if src.exists() else []
 if not ds:return
 z=max(ds,key=lambda p:p.stat().st_mtime)
 for x in z.iterdir():shutil.copytree(x,run/x.name,dirs_exist_ok=True) if x.is_dir() else shutil.copy2(x,run/x.name)
def one(root,s,pr,ca):
 run=root/"runs"/f"seed_{s}"/scen(pr,ca);run.mkdir(parents=True,exist_ok=True);cf=run/"config_used.yaml";writecfg(cf,s,pr,ca,run.relative_to(CODE))
 with (run/"run.log").open("w") as lg:subprocess.run(["/usr/bin/time","-v","-o",str(run/"time_verbose.txt"),str(CODE/"bin/ga_bp"),str(cf)],cwd=CODE,stdout=lg,stderr=subprocess.STDOUT)
 archive(run)
 if not (run/"result.csv").exists():raise RuntimeError("result.csv missing")
 if not (run/"resolved_config.yaml").exists():shutil.copy2(cf,run/"resolved_config.yaml")
 r=next(csv.DictReader((run/"result.csv").open(newline="")));w=[x for x in r.get("best_w","").split(";") if x]
 if len(w)!=10:raise RuntimeError("best_w length")
 e=float(r["carbon_emission"])
 if abs(float(r["e_plus"])-max(e-ca,0))>1e-4 or abs(float(r["e_minus"])-max(ca-e,0))>1e-4 or abs(float(r["net_carbon_trading_cost"])-pr*(e-ca))>1e-4:raise RuntimeError("carbon identity")
 if r.get("inner_bp_status")!="OPTIMAL" or r.get("has_integer_solution") not in ("1","True","true"):raise RuntimeError("BP validation")
 base=cfg(run/"resolved_config.yaml");base.pop("carbon_price",None);base.pop("carbon_cap",None);base.pop("output_dir",None)
 req=float(r.get("fitness_requests",0));hit=float(r.get("actual_fitness_cache_hits",0))
 n={"random_seed":s,"scenario_id":scen(pr,ca),"carbon_price":pr,"carbon_cap":ca,"size_id":"10x30","outer_solution_status":"HEURISTIC_BEST_FOUND","final_inner_bp_status":r.get("inner_bp_status",""),"integer_solution":r.get("has_integer_solution",""),"total_profit":r.get("total_profit",""),"carbon_emission":r.get("carbon_emission",""),"total_investment_cost":r.get("total_investment_cost",""),"e_plus":r.get("e_plus",""),"e_minus":r.get("e_minus",""),"net_carbon_trading_cost":r.get("net_carbon_trading_cost",""),"open_dc_count":r.get("num_dc_open",""),"retailer_service_set":r.get("num_retailers_served",""),"w_vector":r.get("best_w",""),"max_w":max(float(x) for x in w),"actual_bp_calls":r.get("fitness_bp_solves",""),"fitness_requests":r.get("fitness_requests",""),"cache_hit_rate":hit/req if req else 0,"base_instance_fingerprint":hashlib.sha256(json.dumps(base,sort_keys=True).encode()).hexdigest(),"run_config_fingerprint":sha(run/"resolved_config.yaml"),"config_sha256":sha(cf)}
 with (run/"normalized_result.csv").open("w",newline="") as f:x=csv.DictWriter(f,fieldnames=n);x.writeheader();x.writerow(n)
 (run/"COMPLETED.ok").write_text("validated\n");return run
def tasks():
 a=[]
 for s in SEEDS:
  a.append((s,2,100000));a += [(s,p,100000) for p in PRICES if p!=2];a += [(s,2,c) for c in CAPS if c!=100000]
 return a
def main():
 root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True);qp=root/"RQ3_SCREENING_RUN_QUEUE.csv";ts=tasks()
 if not qp.exists():
  with qp.open("w",newline="") as f:
   w=csv.DictWriter(f,fieldnames=["order_id","random_seed","scenario_id","status","validation_status","completion_marker","output_directory","notes"]);w.writeheader()
   for i,(s,p,c) in enumerate(ts,1):w.writerow({"order_id":i,"random_seed":s,"scenario_id":scen(p,c),"status":"PENDING","validation_status":"","completion_marker":"","output_directory":"","notes":""})
 (root/"BATCH_MANIFEST.txt").write_text("experiment_type: screening_only\nformal_experiment: false\nexpected_unique_tasks: 42\nparallel_scenario_runs: 1\nouter_workers_per_run: 8\n")
 with LOCK.open("w") as lf:
  fcntl.flock(lf,fcntl.LOCK_EX);q=list(csv.DictReader(qp.open(newline="")))
  for row in q:
   if row["status"]=="COMPLETED":continue
   s=int(row["random_seed"]);parts=row["scenario_id"].replace("PRICE_","").split("_CAP_");pr=int(parts[0])/100;ca=int(parts[1])
   try:run=one(root,s,pr,ca);row.update(status="COMPLETED",validation_status="PASS",completion_marker="COMPLETED.ok",output_directory=str(run))
   except Exception as ex:row.update(status="FAILED",validation_status="FAIL",notes=str(ex))
   with qp.open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=q[0]);w.writeheader();w.writerows(q)
   (root/"SCREENING_PROGRESS.md").write_text(f"completed={sum(x['status']=='COMPLETED' for x in q)}/42\nfailed={sum(x['status']=='FAILED' for x in q)}\n")
   if row["status"]=="FAILED":return 1
 (root/"SCREENING_FINALIZATION_MANIFEST.txt").write_text("screening_status: FINAL_COMPLETE_AND_FROZEN\nformal_stage_status: PENDING_USER_REVIEW\n");return 0
if __name__=="__main__":raise SystemExit(main())

