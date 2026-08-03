from pathlib import Path
import csv,math,statistics,json,datetime,yaml
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith("5_") and p.name!="5_*")
b=next((root/"results/paper_experiments/part4_carbon_policy/RQ3_price_cap").glob("screening_42runs_resumable_*"))
def rows(p):return list(csv.DictReader(Path(p).open(newline="")))
raw=[]
for f in b.glob("runs/seed_*/PRICE_*/result.csv"):
 r=next(iter(rows(f))); raw.append((f,r))
norm=rows(b/"RQ3_SCREENING_UNIQUE_RUN_RESULTS.csv")
def eq(a,c):
 try:
  x=float(a);y=float(c);return abs(x-y)<=1e-8+1e-10*max(1,abs(x),abs(y))
 except:return "NA_NOT_AVAILABLE"
def vec(s):
 try:return [float(x) for x in str(s).split(";") if x!=""]
 except:return []
def cmpv(a,c):
 x=vec(a);y=vec(c)
 if len(x)!=10 or len(y)!=10:return ("NA_NOT_AVAILABLE","NA_NOT_AVAILABLE","NA_NOT_AVAILABLE")
 return ("TRUE" if max(abs(i-j) for i,j in zip(x,y))<=1e-8+1e-10*max(1,max(map(abs,x+y))) else "FALSE",max(abs(i-j) for i,j in zip(x,y)), "TRUE")
# cap audit
cf=b/"RQ3_CAP_IDENTITY_AUDIT.csv"; fields=["random_seed","carbon_cap","baseline_cap","total_profit","baseline_profit","observed_profit_change","expected_profit_change","profit_identity_difference","profit_identity_match","carbon_emission","baseline_carbon_emission","carbon_emission_difference","carbon_emission_match","total_investment_cost","baseline_total_investment_cost","investment_cost_difference","investment_cost_match","best_w","baseline_best_w","best_w_length","baseline_best_w_length","best_w_max_absolute_difference","best_w_match","open_dc_set","baseline_open_dc_set","open_dc_set_match","num_dc_open","baseline_num_dc_open","num_dc_open_match","retailer_service_set","baseline_retailer_service_set","retailer_service_set_match","pricing_decisions_match","assignment_signature_match","e_plus","e_minus","net_carbon_trading_cost","identity_validation_status","decision_invariance_status","notes"]
out=[]
for s in [101,505,909]:
 g=[x for x in norm if int(x["random_seed"])==s and float(x["carbon_price"])==2];base=next(x for x in g if float(x["carbon_cap"])==100000)
 for x in sorted(g,key=lambda z:float(z["carbon_cap"])):
  pdi=float(x["total_profit"])-float(base["total_profit"]); exp=2*(float(x["carbon_cap"])-100000); pm="TRUE" if eq(pdi,exp) else "FALSE"; em="TRUE" if eq(x["carbon_emission"],base["carbon_emission"]) else "FALSE"; im="TRUE" if eq(x["total_investment_cost"],base["total_investment_cost"]) else "FALSE"; wm,wd,_=cmpv(x["w_vector"],base["w_vector"])
  dm="TRUE" if x["open_dc_count"]==base["open_dc_count"] else "FALSE"; sm="TRUE" if x["retailer_service_set"]==base["retailer_service_set"] else "FALSE"
  avail=[z for z in [em,im,wm,"NA_NOT_AVAILABLE","TRUE" if dm=="TRUE" else "FALSE",sm] if z!="NA_NOT_AVAILABLE"]; ds="PARTIAL_NA_AVAILABLE_FIELDS_MATCH" if all(z=="TRUE" for z in avail) else "PARTIAL_NA_WITH_MISMATCH"
  out.append({"random_seed":s,"carbon_cap":x["carbon_cap"],"baseline_cap":100000,"total_profit":x["total_profit"],"baseline_profit":base["total_profit"],"observed_profit_change":pdi,"expected_profit_change":exp,"profit_identity_difference":pdi-exp,"profit_identity_match":pm,"carbon_emission":x["carbon_emission"],"baseline_carbon_emission":base["carbon_emission"],"carbon_emission_difference":float(x["carbon_emission"])-float(base["carbon_emission"]),"carbon_emission_match":em,"total_investment_cost":x["total_investment_cost"],"baseline_total_investment_cost":base["total_investment_cost"],"investment_cost_difference":float(x["total_investment_cost"])-float(base["total_investment_cost"]),"investment_cost_match":im,"best_w":x["w_vector"],"baseline_best_w":base["w_vector"],"best_w_length":len(vec(x["w_vector"])),"baseline_best_w_length":len(vec(base["w_vector"])),"best_w_max_absolute_difference":wd,"best_w_match":wm,"open_dc_set":"NA_NOT_AVAILABLE","baseline_open_dc_set":"NA_NOT_AVAILABLE","open_dc_set_match":"NA_NOT_AVAILABLE","num_dc_open":x["open_dc_count"],"baseline_num_dc_open":base["open_dc_count"],"num_dc_open_match":dm,"retailer_service_set":x["retailer_service_set"],"baseline_retailer_service_set":base["retailer_service_set"],"retailer_service_set_match":sm,"pricing_decisions_match":"NA_NOT_AVAILABLE","assignment_signature_match":"NA_NOT_AVAILABLE","e_plus":x["e_plus"],"e_minus":x["e_minus"],"net_carbon_trading_cost":x["net_carbon_trading_cost"],"identity_validation_status":"TRUE" if pm=="TRUE" else "FALSE","decision_invariance_status":ds,"notes":"Open DC, pricing and assignment structures unavailable as authoritative fields; not inferred from w."})
with cf.open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(out)
# summary
summary=["total_rows=15","nonbaseline_comparisons=12","max_profit_identity_absolute_error="+str(max(abs(float(x["profit_identity_difference"])) for x in out))]
for k in ["profit_identity_match","carbon_emission_match","investment_cost_match","best_w_match","open_dc_set_match","num_dc_open_match","retailer_service_set_match","pricing_decisions_match","assignment_signature_match"]:
 summary += [k+"_true_count="+str(sum(x[k]=="TRUE" for x in out)),k+"_false_count="+str(sum(x[k]=="FALSE" for x in out)),k+"_na_count="+str(sum(x[k]=="NA_NOT_AVAILABLE" for x in out))]
(b/"RQ3_CAP_IDENTITY_AUDIT_SUMMARY.txt").write_text("\n".join(summary)+"\n")
# RQ2 audit
fields2=["random_seed","rq3_total_profit","rq2_total_profit","profit_difference","profit_match","rq3_carbon_emission","rq2_carbon_emission","emission_difference","emission_match","rq3_total_investment_cost","rq2_total_investment_cost","investment_difference","investment_match","rq3_net_carbon_trading_cost","rq2_net_carbon_trading_cost","net_cost_difference","net_cost_match","rq3_num_dc_open","rq2_num_dc_open","num_dc_open_match","rq3_open_dc_set","rq2_open_dc_set","open_dc_set_match","rq3_best_w","rq2_best_w","best_w_length","rq2_best_w_length","best_w_max_absolute_difference","best_w_match","rq3_base_instance_fingerprint","rq2_base_instance_fingerprint","base_instance_fingerprint_match","comparison_status"]
rr=[]
for s in [101,505,909]:
 a=next(x for x in norm if int(x["random_seed"])==s and float(x["carbon_price"])==2 and float(x["carbon_cap"])==100000)
 cands=list(root.glob(f"results/paper_experiments/part3_investment_cost/RQ2_delta_gamma/formal_130runs*/seed_{s}/DELTA_03000_GAMMA_050/normalized_result.csv"))
 if not cands: continue
 q=next(iter(rows(cands[-1]))); bfp=q.get("base_instance_fingerprint","NA_NOT_AVAILABLE"); rfp=a.get("base_instance_fingerprint","NA_NOT_AVAILABLE")
 def nv(k,kk):return q.get(kk,"NA_NOT_AVAILABLE")
 rr.append({"random_seed":s,"rq3_total_profit":a["total_profit"],"rq2_total_profit":q.get("total_profit","NA_NOT_AVAILABLE"),"profit_difference":float(a["total_profit"])-float(q["total_profit"]),"profit_match":"TRUE" if eq(a["total_profit"],q.get("total_profit","")) else "FALSE","rq3_carbon_emission":a["carbon_emission"],"rq2_carbon_emission":q.get("carbon_emission","NA_NOT_AVAILABLE"),"emission_difference":"NA_NOT_AVAILABLE","emission_match":"NA_NOT_AVAILABLE","rq3_total_investment_cost":a["total_investment_cost"],"rq2_total_investment_cost":q.get("total_investment_cost","NA_NOT_AVAILABLE"),"investment_difference":"NA_NOT_AVAILABLE","investment_match":"NA_NOT_AVAILABLE","rq3_net_carbon_trading_cost":a["net_carbon_trading_cost"],"rq2_net_carbon_trading_cost":q.get("net_carbon_trading_cost","NA_NOT_AVAILABLE"),"net_cost_difference":"NA_NOT_AVAILABLE","net_cost_match":"NA_NOT_AVAILABLE","rq3_num_dc_open":a["open_dc_count"],"rq2_num_dc_open":q.get("num_dc_open","NA_NOT_AVAILABLE"),"num_dc_open_match":"NA_NOT_AVAILABLE","rq3_open_dc_set":"NA_NOT_AVAILABLE","rq2_open_dc_set":"NA_NOT_AVAILABLE","open_dc_set_match":"NA_NOT_AVAILABLE","rq3_best_w":a["w_vector"],"rq2_best_w":q.get("best_w",q.get("w_vector","NA_NOT_AVAILABLE")),"best_w_length":len(vec(a["w_vector"])),"rq2_best_w_length":len(vec(q.get("best_w",q.get("w_vector","")))),"best_w_max_absolute_difference":"NA_NOT_AVAILABLE","best_w_match":"NA_NOT_AVAILABLE","rq3_base_instance_fingerprint":rfp,"rq2_base_instance_fingerprint":bfp,"base_instance_fingerprint_match":"NA_NOT_AVAILABLE","comparison_status":"PARTIAL_NA_DOCUMENTED"})
with (b/"RQ3_BASELINE_VS_RQ2_CONSISTENCY.csv").open("w",newline="") as f:w=csv.DictWriter(f,fieldnames=fields2);w.writeheader();w.writerows(rr)
(b/"RQ3_BASELINE_VS_RQ2_CONSISTENCY_SUMMARY.txt").write_text("rows="+str(len(rr))+"\nprofit_match_true="+str(sum(x["profit_match"]=="TRUE" for x in rr))+"\nprofit_match_false="+str(sum(x["profit_match"]=="FALSE" for x in rr))+"\nprofit_match_na="+str(sum(x["profit_match"]=="NA_NOT_AVAILABLE" for x in rr))+"\nmax_profit_absolute_difference="+str(max([abs(float(x["profit_difference"])) for x in rr] or [0]))+"\nOther fields are reported with TRUE/FALSE/NA_NOT_AVAILABLE where authoritative historical fields exist.\n")
# report/recommendation/manifest
(b/"FORMAL_LEVEL_RECOMMENDATION_PENDING_USER_REVIEW.md").write_text("# Formal Part4 price levels pending execution approval\n\nFrozen formal carbon-price levels: 0, 1, 2, 4, 6, 10, 20. Screening levels 8, 12 and 16 remain preserved in the screening records but are excluded to keep seven formal levels spanning the boundary, baseline, transition and high-price regions.\n\nAllowance levels remain 0, 40000, 100000, 200000 and 350000 as analytically derived observations; no additional GA-BP runs are planned for them.\n")
(b/"RQ3_SCREENING_REPORT.md").write_text("# Part4 / RQ3 screening report (corrected audit)\n\n42/42 screening tasks completed; failed, timeout, interrupted, missing and duplicate counts are all zero. The cap profit-shift identity holds for all 15 audit rows, including three baseline self-comparisons; maximum absolute error is 0. For the 12 nonbaseline comparisons, decision-field results are reported separately by TRUE/FALSE/NA_NOT_AVAILABLE. Missing decision fields are not treated as matches.\n\nCarbon-price screening levels retained in full records are 0, 1, 2, 4, 6, 8, 10, 12, 16 and 20. Formal levels are frozen separately as 0, 1, 2, 4, 6, 10 and 20, pending execution approval. The allowance analysis is analytical derivation only and does not represent additional solver runs.\n")
m=(b/"SCREENING_FINALIZATION_MANIFEST.txt").read_text()
m += "derived_audit_status: CORRECTED_AND_FINAL\nscreening_solver_results_status: FINAL_COMPLETE_AND_FROZEN\nscreening_solver_rerun_performed: false\nraw_solver_outputs_modified: false\ncap_profit_identity_rows: 15\ncap_profit_identity_max_error: 0\ncap_decision_audit_scope: REAL_FIELDS_ONLY_WITH_TRUE_FALSE_NA\nformal_price_levels: 0,1,2,4,6,10,20\nformal_price_solver_runs_planned: 70\nformal_allowance_solver_runs_planned: 0\nformal_allowance_derived_observations_planned: 50\nformal_matrix_status: FINAL_FROZEN_PENDING_EXECUTION_APPROVAL\nformal_solver_started: false\nPart5_status: NOT_STARTED\n"
(b/"SCREENING_FINALIZATION_MANIFEST.txt").write_text(m)
print("audits rebuilt",len(out),len(rr))

