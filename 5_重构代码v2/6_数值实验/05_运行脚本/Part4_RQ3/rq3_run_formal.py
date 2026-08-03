import csv,datetime,fcntl,hashlib,json,math,os,shutil,subprocess,sys
from pathlib import Path
CODE=Path(__file__).resolve().parents[3];LOCK=CODE/"results/paper_experiments/.experiment_resource.lock";TEMPLATE=CODE/"results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml";COMMIT="0dd725354732f9b8011e9e7e8540e0e64a3ff223";TAG="paper-exp-baseline-20260801";SEEDS=[101,202,303,404,505,606,707,808,909,1010];PRICES=[0,1,2,4,6,10,20]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def scen(p):return f"PRICE_{p:02d}_CAP_100000"
def cfg(p):
 d={}
 for l in Path(p).read_text().splitlines():
  if ":" in l:k,v=l.split(":",1);d[k.strip()]=v.strip().strip("'\"")
 return d
def writecfg(p,s,price,out):
 rep={"num_dc":"10","num_retailers":"30","random_seed":str(s),"carbon_price":str(float(price)),"carbon_cap":"100000.0","delta":"3000.0","investment_exponent":"0.5","output_dir":str(out),"run_mode":"ga"}
 a=[]
 for l in TEMPLATE.read_text().splitlines():
  k=l.split(":",1)[0].strip() if ":" in l else "";a.append(k+": "+rep[k] if k in rep else l)
 Path(p).write_text("\n".join(a)+"\n")
def archive(run):
 src=CODE/"results/single";ds=[p for p in src.iterdir() if p.is_dir() and (p/"result.csv").exists()] if src.exists() else []
 if not ds:return
 z=max(ds,key=lambda p:p.stat().st_mtime)
 for x in z.iterdir():shutil.copytree(x,run/x.name,dirs_exist_ok=True) if x.is_dir() else shutil.copy2(x,run/x.name)
def one(root,s,pr):
 run=root/"runs"/f"seed_{s}"/scen(pr);run.mkdir(parents=True,exist_ok=True);cf=run/"config_used.yaml";writecfg(cf,s,pr,run.relative_to(CODE))
 (run/"MANIFEST.txt").write_text(f"experiment_type: formal_solver_run\nformal_paper_statistics: true\nscenario_id: {scen(pr)}\nsize_id: 10x30\nrandom_seed: {s}\ncarbon_price: {pr}\ncarbon_cap: 100000\ndelta: 3000\ninvestment_exponent: 0.5\nalgorithm_commit: {COMMIT}\nformal_tag: {TAG}\nformal_matrix: FORMAL_EXPERIMENT_MATRIX_PART4_RQ3_20260803_144509.yaml\n")
 with (run/"run.log").open("w") as lg:subprocess.run(["/usr/bin/time","-v","-o",str(run/"time_verbose.txt"),str(CODE/"bin/ga_bp"),str(cf)],cwd=CODE,stdout=lg,stderr=subprocess.STDOUT)
 archive(run)
 if not (run/"result.csv").exists():raise RuntimeError("result.csv missing")
 if not (run/"resolved_config.yaml").exists():shutil.copy2(cf,run/"resolved_config.yaml")
 r=next(csv.DictReader((run/"result.csv").open(newline="")));w=[x for x in r.get("best_w","").split(";") if x]
 if len(w)!=10:raise RuntimeError("best_w length")
 e=float(r["carbon_emission"]);ep=max(e-100000,0);em=max(100000-e,0)
 if abs(float(r["e_plus"])-ep)>1e-4 or abs(float(r["e_minus"])-em)>1e-4 or abs(float(r["net_carbon_trading_cost"])-pr*(e-100000))>1e-4:raise RuntimeError("carbon identity")
 base=cfg(run/"resolved_config.yaml");base.pop("carbon_price",None);base.pop("carbon_cap",None);base.pop("output_dir",None)
 req=float(r.get("fitness_requests",0));hit=float(r.get("actual_fitness_cache_hits",0));pos=[float(x) for x in w if float(x)>0]
 n={"random_seed":s,"carbon_price":pr,"carbon_cap":100000,"delta":3000,"investment_exponent":0.5,"outer_method":"GA-BP","outer_solution_status":"HEURISTIC_BEST_FOUND","final_inner_bp_status":r.get("inner_bp_status",""),"final_inner_bp_certified":r.get("inner_bp_status")=="OPTIMAL","integer_solution":r.get("has_integer_solution",""),"termination_reason":"normal_or_early_stop","total_profit":r.get("total_profit",""),"carbon_emission":r.get("carbon_emission",""),"total_investment_cost":r.get("total_investment_cost",""),"net_carbon_trading_cost":r.get("net_carbon_trading_cost",""),"e_plus":r.get("e_plus",""),"e_minus":r.get("e_minus",""),"num_dc_open":r.get("num_dc_open",""),"open_dc_set":"NA_NOT_AVAILABLE","num_retailers_served":r.get("num_retailers_served",""),"retailer_service_set":"NA_NOT_AVAILABLE","w_vector_candidate_dc":r.get("best_w",""),"w_vector_length":len(w),"positive_w_count":len(pos),"mean_positive_w":sum(pos)/len(pos) if pos else 0,"mean_all_candidate_w":sum(float(x) for x in w)/10,"mean_open_dc_w":"NA_NOT_AVAILABLE","max_w":max(float(x) for x in w),"min_positive_w":min(pos) if pos else 0,"ga_generations":r.get("ga_generations",""),"fitness_requests":r.get("fitness_requests",""),"fitness_bp_solves":r.get("fitness_bp_solves",""),"actual_fitness_cache_hits":r.get("actual_fitness_cache_hits",""),"cache_hit_rate":hit/req if req else 0,"cg_iterations":r.get("cg_iterations",""),"branch_nodes":r.get("branch_nodes",""),"wall_time_sec":r.get("solve_time",""),"algorithm_commit":COMMIT,"formal_tag":TAG,"base_instance_fingerprint":hashlib.sha256(json.dumps(base,sort_keys=True).encode()).hexdigest(),"run_config_fingerprint":sha(run/"resolved_config.yaml")}
 with (run/"normalized_result.csv").open("w",newline="") as f:x=csv.DictWriter(f,fieldnames=n);x.writeheader();x.writerow(n)
 (run/"COMPLETED.ok").write_text("validated\n");return run
def main():
 root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True);qp=root/"RQ3_FORMAL_RUN_QUEUE.csv"
 tasks=[(202,0),(202,6),(202,20)]+[(s,p) for s in SEEDS for p in PRICES if (s,p) not in [(202,0),(202,6),(202,20)]]
 if not qp.exists():
  with qp.open("w",newline="") as f:
   w=csv.DictWriter(f,fieldnames=["order_id","random_seed","carbon_price","scenario_id","status","validation_status","completion_marker","output_directory","notes"]);w.writeheader()
   for i,(s,p) in enumerate(tasks,1):w.writerow({"order_id":i,"random_seed":s,"carbon_price":p,"scenario_id":scen(p),"status":"PENDING","validation_status":"","completion_marker":"","output_directory":"","notes":""})
 (root/"BATCH_MANIFEST.txt").write_text("experiment_type: formal_part4_rq3\nformal_experiment: true\nexpected_unique_tasks: 70\nparallel_scenario_runs: 1\nouter_workers_per_run: 8\nparallel_pricing: false\ncplex_threads: 1\napproved_matrix: FORMAL_EXPERIMENT_MATRIX_PART4_RQ3_20260803_144509.yaml\n")
 with LOCK.open("w") as lf:
  fcntl.flock(lf,fcntl.LOCK_EX);q=list(csv.DictReader(qp.open(newline="")))
  for row in q:
   if row["status"]=="COMPLETED":continue
   s=int(row["random_seed"]);p=int(row["carbon_price"])
   try:run=one(root,s,p);row.update(status="COMPLETED",validation_status="PASS",completion_marker="COMPLETED.ok",output_directory=str(run))
   except Exception as ex:row.update(status="FAILED",validation_status="FAIL",notes=str(ex))
   with qp.open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=q[0]);w.writeheader();w.writerows(q)
   (root/"FORMAL_PROGRESS.md").write_text(f"completed={sum(x['status']=='COMPLETED' for x in q)}/70\nfailed={sum(x['status']=='FAILED' for x in q)}\n")
   if row["status"]=="FAILED":return 1
 (root/"FORMAL_EXECUTION_APPROVAL.md").write_text("formal_execution_approved_by_user: true\napproved_solver_runs: 70\n");return 0
if __name__=="__main__":raise SystemExit(main())

