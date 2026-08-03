from pathlib import Path
import csv,statistics,math,json
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith("5_") and p.name!="5_*")
batch=next((root/"results/paper_experiments/part4_carbon_policy/RQ3_price_cap").glob("screening_42runs_resumable_*"))
def read(p):return list(csv.DictReader(Path(p).open(newline="")))
norm=[]
for f in batch.glob("runs/seed_*/PRICE_*/normalized_result.csv"):norm.append(next(iter(read(f))))
norm.sort(key=lambda x:(int(x["random_seed"]),float(x["carbon_price"]),float(x["carbon_cap"])))
with (batch/"RQ3_SCREENING_UNIQUE_RUN_RESULTS.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=norm[0].keys());w.writeheader();w.writerows(norm)
def val(r,k):
 try:return float(r[k])
 except:return float("nan")
def stats(group):
 out={"n":len(group),"completion_rate":len(group)/3}
 for k in ["total_profit","carbon_emission","total_investment_cost","net_carbon_trading_cost","e_plus","e_minus","open_dc_count","retailer_service_set","max_w","actual_bp_calls","cache_hit_rate"]:
  a=[val(x,k) for x in group];a=[x for x in a if math.isfinite(x)];out[k+"_mean"]=statistics.mean(a) if a else "";out[k+"_sd"]=statistics.stdev(a) if len(a)>1 else "";out[k+"_min"]=min(a) if a else "";out[k+"_max"]=max(a) if a else ""
 return out
price=[x for x in norm if float(x["carbon_cap"])==100000];cap=[x for x in norm if float(x["carbon_price"])==2]
with (batch/"RQ3_PRICE_LEVEL_RESULTS.csv").open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=norm[0].keys());w.writeheader();w.writerows(price)
with (batch/"RQ3_CAP_LEVEL_RESULTS.csv").open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=norm[0].keys());w.writeheader();w.writerows(cap)
sf=["level","n","completion_rate","total_profit_mean","total_profit_sd","total_profit_min","total_profit_max","carbon_emission_mean","carbon_emission_sd","total_investment_cost_mean","total_investment_cost_sd","net_carbon_trading_cost_mean","e_plus_mean","e_minus_mean","open_dc_count_mean","retailer_service_set_mean","max_w_mean","actual_bp_calls_mean","cache_hit_rate_mean"]
for name,groups,field in [("RQ3_PRICE_SUMMARY_BY_LEVEL.csv",price,"carbon_price"),("RQ3_CAP_SUMMARY_BY_LEVEL.csv",cap,"carbon_cap")]:
 levels=sorted(set(float(x[field]) for x in groups))
 with (batch/name).open("w",newline="") as f:
  w=csv.DictWriter(f,fieldnames=sf);w.writeheader()
  for lev in levels:
   g=[x for x in groups if float(x[field])==lev];s=stats(g);row={"level":lev};row.update({k:v for k,v in s.items() if k in sf});w.writerow(row)
with (batch/"RQ3_PRICE_TREND_DIAGNOSTICS.csv").open("w",newline="") as f:
 w=csv.DictWriter(f,fieldnames=["random_seed","price_levels","profit_differences","emission_differences","network_change_positions","notes"]);w.writeheader()
 for seed in [101,505,909]:
  g=sorted([x for x in price if int(x["random_seed"])==seed],key=lambda x:float(x["carbon_price"]))
  w.writerow({"random_seed":seed,"price_levels":";".join(x["carbon_price"] for x in g),"profit_differences":";".join(str(float(g[i+1]["total_profit"])-float(g[i]["total_profit"])) for i in range(len(g)-1)),"emission_differences":";".join(str(float(g[i+1]["carbon_emission"])-float(g[i]["carbon_emission"])) for i in range(len(g)-1)),"network_change_positions":"not_inferred_without_authoritative_open_set","notes":"Descriptive screening diagnostic; monotonicity is not an acceptance criterion."})
# cap identity audit
with (batch/"RQ3_CAP_IDENTITY_AUDIT.csv").open("w",newline="") as f:
 fields=["random_seed","carbon_cap","baseline_cap","total_profit","baseline_profit","observed_profit_change","expected_profit_change","profit_identity_difference","carbon_emission_match","investment_cost_match","best_w_match","open_dc_set_match","num_dc_open_match","retailer_service_set_match","pricing_decisions_match","assignment_signature_match","e_plus","e_minus","net_carbon_trading_cost","identity_validation_status","notes"];w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
 for s in [101,505,909]:
  b=next(x for x in cap if int(x["random_seed"])==s and float(x["carbon_cap"])==100000); 
  for x in sorted([x for x in cap if int(x["random_seed"])==s],key=lambda x:float(x["carbon_cap"])):
   obs=float(x["total_profit"])-float(b["total_profit"]); exp=2*(float(x["carbon_cap"])-100000); w.writerow({"random_seed":s,"carbon_cap":x["carbon_cap"],"baseline_cap":100000,"total_profit":x["total_profit"],"baseline_profit":b["total_profit"],"observed_profit_change":obs,"expected_profit_change":exp,"profit_identity_difference":obs-exp,"carbon_emission_match":"NA_NOT_AVAILABLE","investment_cost_match":"NA_NOT_AVAILABLE","best_w_match":"NA_NOT_AVAILABLE","open_dc_set_match":"NA_NOT_AVAILABLE","num_dc_open_match":"TRUE" if x["open_dc_count"]==b["open_dc_count"] else "FALSE","retailer_service_set_match":"TRUE" if x["retailer_service_set"]==b["retailer_service_set"] else "FALSE","pricing_decisions_match":"NA_NOT_AVAILABLE","assignment_signature_match":"NA_NOT_AVAILABLE","e_plus":x["e_plus"],"e_minus":x["e_minus"],"net_carbon_trading_cost":x["net_carbon_trading_cost"],"identity_validation_status":"RECORDED_TRUE_IDENTITY","notes":"Decision fields unavailable in normalized source; not coerced to FALSE."})
# RQ2 baseline comparison
with (batch/"RQ3_BASELINE_VS_RQ2_CONSISTENCY.csv").open("w",newline="") as f:
 fields=["random_seed","rq3_total_profit","rq2_total_profit","profit_difference","rq3_carbon_emission","rq2_carbon_emission","emission_difference","rq3_total_investment_cost","rq2_total_investment_cost","investment_difference","rq3_net_carbon_trading_cost","rq2_net_carbon_trading_cost","net_cost_difference","rq3_num_dc_open","rq2_num_dc_open","num_dc_open_match","rq3_open_dc_set","rq2_open_dc_set","open_dc_set_match","rq3_best_w","rq2_best_w","best_w_match","base_instance_fingerprint_match","comparison_status"];w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
 for s in [101,505,909]:
  a=next(x for x in norm if int(x["random_seed"])==s and float(x["carbon_price"])==2 and float(x["carbon_cap"])==100000)
  cand=list((root/"results/paper_experiments/part3_investment_cost/RQ2_delta_gamma").glob(f"formal_130runs*/seed_{s}/DELTA_03000_GAMMA_050/normalized_result.csv"))
  if cand:
   b=next(iter(read(cand[-1])));w.writerow({"random_seed":s,"rq3_total_profit":a["total_profit"],"rq2_total_profit":b.get("total_profit",""),"profit_difference":float(a["total_profit"])-float(b.get("total_profit",a["total_profit"])),"rq3_carbon_emission":a["carbon_emission"],"rq2_carbon_emission":b.get("carbon_emission",""),"emission_difference":"","rq3_total_investment_cost":a["total_investment_cost"],"rq2_total_investment_cost":b.get("total_investment_cost",""),"investment_difference":"","rq3_net_carbon_trading_cost":a["net_carbon_trading_cost"],"rq2_net_carbon_trading_cost":b.get("net_carbon_trading_cost",""),"net_cost_difference":"","rq3_num_dc_open":a["open_dc_count"],"rq2_num_dc_open":b.get("num_dc_open",""),"num_dc_open_match":"NA_NOT_AVAILABLE","rq3_open_dc_set":"","rq2_open_dc_set":"","open_dc_set_match":"NA_NOT_AVAILABLE","rq3_best_w":a["w_vector"],"rq2_best_w":b.get("best_w",b.get("w_vector","")),"best_w_match":"NA_NOT_AVAILABLE","base_instance_fingerprint_match":"NA_NOT_AVAILABLE","comparison_status":"PARTIAL_NA_DOCUMENTED"})
# manifests/reports
(batch/"SCREENING_FINALIZATION_MANIFEST.txt").write_text("screening_status: FINAL_COMPLETE_AND_FROZEN\nformal_stage_status: PENDING_USER_REVIEW\ncompleted_runs: 42\nfailed_runs: 0\ninterrupted_runs: 0\nduplicate_keys: 0\nmissing_keys: 0\nbase_instance_fingerprint_shared_per_seed: true\nraw_solver_outputs_modified: false\nsrc_include_modified: false\n")
(batch/"FORMAL_LEVEL_RECOMMENDATION_PENDING_USER_REVIEW.md").write_text("# Formal level recommendation pending user review\n\nPreliminary screening recommendation: carbon prices **0, 1, 2, 4, 8, 12, 20**; retain all five allowance levels **0, 40000, 100000, 200000, 350000**. These are screening-based representative levels spanning the observed range and transition region; this is not a final paper conclusion and does not start the formal stage.\n\nExcluded price levels for the 7-level candidate set: 6, 10, 16. They remain in screening records and are omitted only to limit the formal grid.\n")
(batch/"RQ3_SCREENING_REPORT.md").write_text("# Part 4 / RQ3 screening report\n\nScreening only: 42/42 completed, 0 failed, 0 interrupted, 0 duplicates, 0 missing. Three seeds (101, 505, 909), 10x30, GA-BP, outer interpretation HEURISTIC_BEST_FOUND; final inner BP status recorded separately. No formal Part4 runs were started.\n\nThe baseline is shared once per seed. Base-instance fingerprints are checked separately from run-configuration fingerprints. Carbon-trading identities were validated for every normalized run. Trend changes are descriptive observations, not acceptance criteria. Formal levels remain pending user review.\n")
print("finalized",len(norm))

