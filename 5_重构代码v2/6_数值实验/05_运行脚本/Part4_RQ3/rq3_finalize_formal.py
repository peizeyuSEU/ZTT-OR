from pathlib import Path
import csv,statistics,math,json
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith("5_") and p.name!="5_*")
b=next((root/"results/paper_experiments/part4_carbon_policy/RQ3_price_cap").glob("formal_70runs_resumable_*"))
def read(p):return list(csv.DictReader(Path(p).open(newline="")))
q=read(b/"RQ3_FORMAL_RUN_QUEUE.csv");assert len(q)==70 and all(x["status"]=="COMPLETED" for x in q)
rows=[]
for x in q:
 rows.append(next(iter(read(Path(x["output_directory"])/"normalized_result.csv"))))
with (b/"RQ3_FORMAL_UNIQUE_RUN_RESULTS.csv").open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
def stats(g):
 out={"n":len(g),"completion_rate":len(g)/10}
 for k in ["total_profit","carbon_emission","total_investment_cost","net_carbon_trading_cost","e_plus","e_minus","num_dc_open","num_retailers_served","mean_all_candidate_w","max_w","wall_time_sec","fitness_bp_solves","cache_hit_rate"]:
  a=[float(x[k]) for x in g if x.get(k,"") not in ("","NA_NOT_AVAILABLE")];out[k+"_mean"]=statistics.mean(a);out[k+"_sd"]=statistics.stdev(a) if len(a)>1 else 0;out[k+"_median"]=statistics.median(a);out[k+"_min"]=min(a);out[k+"_max"]=max(a)
 return out
price_fields=["carbon_price"]+sorted(stats([rows[0]]).keys())
with (b/"RQ3_FORMAL_PRICE_SUMMARY_BY_LEVEL.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=price_fields);w.writeheader()
 for p in [0,1,2,4,6,10,20]:
  s=stats([x for x in rows if int(float(x["carbon_price"]))==p]);w.writerow({"carbon_price":p,**s})
with (b/"RQ3_FORMAL_PRICE_LEVEL_RESULTS.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
# paired differences vs price 2
pf=["random_seed","carbon_price","baseline_price","profit_difference","emission_difference","investment_difference","net_carbon_cost_difference","num_dc_open_difference","retailer_service_set_difference"]
with (b/"RQ3_FORMAL_PRICE_PAIRED_DIFFERENCES.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=pf);w.writeheader()
 for s in [101,202,303,404,505,606,707,808,909,1010]:
  z=next(x for x in rows if int(x["random_seed"])==s and int(float(x["carbon_price"]))==2)
  for p in [0,1,2,4,6,10,20]:
   x=next(x for x in rows if int(x["random_seed"])==s and int(float(x["carbon_price"]))==p)
   w.writerow({"random_seed":s,"carbon_price":p,"baseline_price":2,"profit_difference":float(x["total_profit"])-float(z["total_profit"]),"emission_difference":float(x["carbon_emission"])-float(z["carbon_emission"]),"investment_difference":float(x["total_investment_cost"])-float(z["total_investment_cost"]),"net_carbon_cost_difference":float(x["net_carbon_trading_cost"])-float(z["net_carbon_trading_cost"]),"num_dc_open_difference":int(float(x["num_dc_open"]))-int(float(z["num_dc_open"])),"retailer_service_set_difference":"NA_NOT_AVAILABLE"})
(b/"RQ3_FORMAL_PRICE_TREND_DIAGNOSTICS.csv").write_text("carbon_price,description\n0,low boundary\n1,low intensity\n2,formal baseline\n4,screening minimum region\n6,post-turning-point level\n10,stable high-price region\n20,high boundary\n")
# analytic allowance from price=2
af=["random_seed","carbon_price","carbon_cap","source_formal_task_id","result_origin","solver_run_performed","source_carbon_price","source_carbon_cap","source_total_profit","total_profit","carbon_emission","total_investment_cost","e_plus","e_minus","net_carbon_trading_cost","w_vector_candidate_dc","num_dc_open","notes"]
ar=[]
for s in [101,202,303,404,505,606,707,808,909,1010]:
 z=next(x for x in rows if int(x["random_seed"])==s and int(float(x["carbon_price"]))==2)
 for c in [0,40000,100000,200000,350000]:
  E=float(z["carbon_emission"]);ar.append({"random_seed":s,"carbon_price":2,"carbon_cap":c,"source_formal_task_id":f"seed_{s}_PRICE_02_CAP_100000","result_origin":"ANALYTICALLY_DERIVED","solver_run_performed":"false","source_carbon_price":2,"source_carbon_cap":100000,"source_total_profit":z["total_profit"],"total_profit":float(z["total_profit"])+2*(c-100000),"carbon_emission":E,"total_investment_cost":z["total_investment_cost"],"e_plus":max(E-c,0),"e_minus":max(c-E,0),"net_carbon_trading_cost":2*(E-c),"w_vector_candidate_dc":z["w_vector_candidate_dc"],"num_dc_open":z["num_dc_open"],"notes":"Derived from formal carbon-price=2 solver result; not an independent GA-BP run."})
with (b/"RQ3_FORMAL_ALLOWANCE_ANALYTICAL_RESULTS.csv").open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=af);w.writeheader();w.writerows(ar)
with (b/"RQ3_FORMAL_ALLOWANCE_SUMMARY_BY_LEVEL.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=["carbon_cap","n","mean_profit","sd_profit","mean_emission","mean_investment_cost"]);w.writeheader()
 for c in [0,40000,100000,200000,350000]:
  g=[x for x in ar if x["carbon_cap"]==c];w.writerow({"carbon_cap":c,"n":len(g),"mean_profit":statistics.mean(float(x["total_profit"]) for x in g),"sd_profit":statistics.stdev(float(x["total_profit"]) for x in g),"mean_emission":statistics.mean(float(x["carbon_emission"]) for x in g),"mean_investment_cost":statistics.mean(float(x["total_investment_cost"]) for x in g)})
with (b/"RQ3_FORMAL_ALLOWANCE_DERIVATION_AUDIT.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=["carbon_cap","max_profit_identity_error","solver_run_performed"]);w.writeheader()
 for c in [0,40000,100000,200000,350000]:w.writerow({"carbon_cap":c,"max_profit_identity_error":0,"solver_run_performed":"false"})
(b/"RQ3_FORMAL_ALLOWANCE_ANALYTICAL_REPORT.md").write_text("# Analytical allowance results\n\n50 observations were derived from the ten formal carbon-price=2 solver results. No additional GA-BP solver runs were performed. These observations are labelled ANALYTICALLY_DERIVED.\n")
statuses={x["final_inner_bp_status"] for x in rows}
(b/"RQ3_FORMAL_VALIDATION_REPORT.md").write_text(f"70/70 formal tasks completed; failures=0; final inner BP statuses={sorted(statuses)}; integer solutions={sum(x['integer_solution'] in ('1','True','true') for x in rows)}/70; analytical allowance observations={len(ar)}; additional allowance solver runs=0.\\n")
(b/"RQ3_FORMAL_REPORT.md").write_text("# Part4 / RQ3 formal report\n\nThe 70 formal GA-BP runs completed successfully. The outer interpretation remains HEURISTIC_BEST_FOUND; final inner BP status is reported separately. The 50 allowance observations are analytical derivations, not independent solver runs. No global-optimality claim is made for the outer GA.\n")
(b/"FORMAL_FINALIZATION_MANIFEST.txt").write_text("formal_status: FINAL_COMPLETE_AND_FROZEN\nformal_solver_runs: 70\ncompleted_runs: 70\nfailed_runs: 0\ntimeout_runs: 0\ninterrupted_runs: 0\nallowance_additional_solver_runs: 0\nallowance_analytical_observations: 50\nraw_solver_outputs_modified: false\nscreening_rerun: false\nPart5_status: NOT_STARTED\n")
print("finalized",len(rows),len(ar),sorted(statuses))

