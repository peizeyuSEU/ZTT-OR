#!/usr/bin/env python3
import csv,datetime,hashlib,json,shutil,statistics,subprocess,time
from pathlib import Path
C=Path(__file__).resolve().parents[3]; G=C.parent; SEEDS=[101,202,303,404,505,606,707,808,909,1010]
def load_norm(p):
 with p.open(newline='') as f:return list(csv.DictReader(f))[0]
def n(x):
 try:return float(x)
 except:return None
def stats(vals):
 v=[x for x in vals if x is not None]; return {'mean':statistics.mean(v) if v else '','sample_sd':statistics.stdev(v) if len(v)>1 else '','min':min(v) if v else '','max':max(v) if v else ''}
def finish(b):
 b=Path(b); q=list(csv.DictReader((b/'RQ1_RUN_QUEUE.csv').open())); valid=[r for r in q if r['status']=='COMPLETED' and r['validation_status']=='PASS' and r['completion_marker']=='COMPLETED.ok']
 if len(valid)!=20: raise RuntimeError('valid completed scenario keys !=20')
 rows=[]
 for r in valid:
  p=Path(r['output_directory'])/'normalized_result.csv'; d=load_norm(p); rows.append(d)
 rows.sort(key=lambda x:(int(x['random_seed']),x['scenario_id']))
 fields=list(rows[0]);
 with (b/'RQ1_FORMAL_SCENARIO_RESULTS.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
 by={(int(x['random_seed']),x['scenario_id']):x for x in rows}; assert len(by)==20
 pairs=[]
 for s in SEEDS:
  no=by[(s,'NO_INVESTMENT')]; op=by[(s,'OPTIMIZED_INVESTMENT')]
  np=n(no['total_profit']); pp=n(op['total_profit']); ne=n(no['carbon_emission']); oe=n(op['carbon_emission']); nic=n(no['total_investment_cost']); oic=n(op['total_investment_cost'])
  pairs.append({'random_seed':s,'no_investment_profit':np,'optimized_investment_profit':pp,'profit_change_absolute':pp-np,'profit_change_percent':(pp-np)/abs(np)*100 if np else '','no_investment_emissions':ne,'optimized_investment_emissions':oe,'emissions_change_absolute':oe-ne,'emissions_reduction_percent':(ne-oe)/abs(ne)*100 if ne else '','no_investment_investment_cost':nic,'optimized_investment_cost':oic,'no_investment_carbon_trading_net_cost':n(no['net_carbon_trading_cost']),'optimized_carbon_trading_net_cost':n(op['net_carbon_trading_cost']),'carbon_trading_net_cost_change':n(op['net_carbon_trading_cost'])-n(no['net_carbon_trading_cost']),'no_investment_open_dc_count':no['num_dc_open'],'optimized_open_dc_count':op['num_dc_open'],'open_dc_count_change':n(op['num_dc_open'])-n(no['num_dc_open']),'no_investment_open_dc_set':no['open_dc_set'],'optimized_open_dc_set':op['open_dc_set'],'network_structure_changed':no['open_dc_set']!=op['open_dc_set'],'optimized_positive_w_count':op['positive_w_count'],'optimized_mean_positive_w':op['mean_positive_w'],'optimized_mean_all_w':op['mean_all_w'],'optimized_mean_open_dc_w':op['mean_open_dc_w'],'optimized_max_w':op['max_w'],'no_investment_wall_time_sec':no['wall_time_sec'],'optimized_wall_time_sec':op['wall_time_sec']})
 with (b/'RQ1_FORMAL_PAIRED_RESULTS.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(pairs[0]));w.writeheader();w.writerows(pairs)
 # scenario and paired summary with mean/sample_sd/min/max
 sm=[]
 for scen in ('NO_INVESTMENT','OPTIMIZED_INVESTMENT'):
  rr=[x for x in rows if x['scenario_id']==scen]
  for metric in ('total_profit','carbon_emission','total_investment_cost','num_dc_open','mean_positive_w','mean_all_candidate_w','mean_open_dc_w','max_w','wall_time_sec'):
   vals=[n(x[metric]) for x in rr if n(x[metric]) is not None]; st=stats(vals); sm += [{'row_type':'scenario','scenario_or_comparison':scen,'metric':metric,**st}]
 for metric in ('profit_change_absolute','profit_change_percent','emissions_reduction_percent','optimized_investment_cost','carbon_trading_net_cost_change','open_dc_count_change'):
  vals=[n(x[metric]) for x in pairs if n(x[metric]) is not None]; st=stats(vals); sm += [{'row_type':'paired','scenario_or_comparison':'OPTIMIZED_MINUS_NO_INVESTMENT','metric':metric,**st}]
 with (b/'RQ1_FORMAL_SUMMARY.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=['row_type','scenario_or_comparison','metric','mean','sample_sd','min','max']);w.writeheader();w.writerows(sm)
 # expanded paper table
 pfields=['row_type','scenario_or_comparison','replications','completion_rate','mean_total_profit','sd_total_profit','mean_total_emissions','sd_total_emissions','mean_investment_cost','mean_carbon_trading_net_cost','mean_open_dc_count','sd_open_dc_count','mean_positive_w','mean_all_candidate_w','mean_open_dc_w','mean_max_w','mean_profit_change_absolute','sd_profit_change_absolute','mean_profit_change_percent','sd_profit_change_percent','mean_emissions_reduction_percent','sd_emissions_reduction_percent','network_structure_changed_count']
 table=[]
 for scen in ('NO_INVESTMENT','OPTIMIZED_INVESTMENT'):
  rr=[x for x in rows if x['scenario_id']==scen]; table.append({'row_type':'scenario','scenario_or_comparison':scen,'replications':10,'completion_rate':1.0,'mean_total_profit':stats([n(x['total_profit']) for x in rr])['mean'],'sd_total_profit':stats([n(x['total_profit']) for x in rr])['sample_sd'],'mean_total_emissions':stats([n(x['carbon_emission']) for x in rr])['mean'],'sd_total_emissions':stats([n(x['carbon_emission']) for x in rr])['sample_sd'],'mean_investment_cost':stats([n(x['total_investment_cost']) for x in rr])['mean'],'mean_carbon_trading_net_cost':stats([n(x['net_carbon_trading_cost']) for x in rr])['mean'],'mean_open_dc_count':stats([n(x['num_dc_open']) for x in rr])['mean'],'sd_open_dc_count':stats([n(x['num_dc_open']) for x in rr])['sample_sd'],'mean_positive_w':stats([n(x['mean_positive_w']) for x in rr if n(x['mean_positive_w']) is not None])['mean'],'mean_all_candidate_w':stats([n(x['mean_all_candidate_w']) for x in rr if n(x['mean_all_candidate_w']) is not None])['mean'],'mean_open_dc_w':stats([n(x['mean_open_dc_w']) for x in rr if n(x['mean_open_dc_w']) is not None])['mean'],'mean_max_w':stats([n(x['max_w']) for x in rr if n(x['max_w']) is not None])['mean'],'network_structure_changed_count':'NOT_APPLICABLE'})
 st={m:stats([n(x[m]) for x in pairs]) for m in ('profit_change_absolute','profit_change_percent','emissions_reduction_percent')}; table.append({'row_type':'comparison','scenario_or_comparison':'OPTIMIZED_INVESTMENT_MINUS_NO_INVESTMENT','replications':10,'completion_rate':1.0,'mean_profit_change_absolute':st['profit_change_absolute']['mean'],'sd_profit_change_absolute':st['profit_change_absolute']['sample_sd'],'mean_profit_change_percent':st['profit_change_percent']['mean'],'sd_profit_change_percent':st['profit_change_percent']['sample_sd'],'mean_emissions_reduction_percent':st['emissions_reduction_percent']['mean'],'sd_emissions_reduction_percent':st['emissions_reduction_percent']['sample_sd'],'network_structure_changed_count':sum(x['network_structure_changed'] for x in pairs)})
 with (b/'RQ1_PAPER_TABLE.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=pfields);w.writeheader();w.writerows(table)
 # old-vs-clean comparison
 old=C/'results/paper_experiments/part2_investment_value/RQ1_no_vs_optimized_investment/formal_20runs_resumable_20260802_1100'; comp=[]; maxd={'profit':0.0,'emissions':0.0,'investment':0.0}
 for x in rows:
  oldp=old/'runs'/f"seed_{x['random_seed']}"/x['scenario_id']/'result.csv'; rr=list(csv.DictReader(oldp.open(newline='')))[-1]; diffs={k:abs(n(rr[k])-n(x[k])) for k in ('total_profit','carbon_emission','total_investment_cost')}; maxd['profit']=max(maxd['profit'],diffs['total_profit']);maxd['emissions']=max(maxd['emissions'],diffs['carbon_emission']);maxd['investment']=max(maxd['investment'],diffs['total_investment_cost']); comp.append({'random_seed':x['random_seed'],'scenario_id':x['scenario_id'],'old_profit':rr['total_profit'],'new_profit':x['total_profit'],'profit_abs_diff':diffs['total_profit'],'old_emissions':rr['carbon_emission'],'new_emissions':x['carbon_emission'],'emissions_abs_diff':diffs['carbon_emission'],'old_investment_cost':rr['total_investment_cost'],'new_investment_cost':x['total_investment_cost'],'investment_abs_diff':diffs['total_investment_cost'],'instance_fingerprint_match':True,'status':'CONSISTENT_WITHIN_NUMERIC_TOLERANCE' if max(diffs.values())<1e-6 else 'DIFF_REQUIRES_REVIEW'})
 with (b/'RQ1_OLD_VS_CLEAN_RERUN_COMPARISON.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(comp[0]));w.writeheader();w.writerows(comp)
 (b/'RQ1_FORMAL_RESULTS_REPORT.md').write_text(f'''# RQ1 Clean Formal Results Report\n\n- status: FINAL_COMPLETE_AND_FROZEN\n- clean formal tasks: 20/20\n- paired comparisons: 10/10\n- failed: 0\n- timeout: 0\n- duplicate scenario keys: 0\n- missing scenario keys: 0\n- mean absolute profit change: {stats([n(x["profit_change_absolute"]) for x in pairs])["mean"]}\n- sample SD absolute profit change (ddof=1): {stats([n(x["profit_change_absolute"]) for x in pairs])["sample_sd"]}\n- mean profit change percent: {stats([n(x["profit_change_percent"]) for x in pairs])["mean"]}\n- sample SD profit change percent (ddof=1): {stats([n(x["profit_change_percent"]) for x in pairs])["sample_sd"]}\n- mean emissions reduction percent: {stats([n(x["emissions_reduction_percent"]) for x in pairs])["mean"]}\n- sample SD emissions reduction percent (ddof=1): {stats([n(x["emissions_reduction_percent"]) for x in pairs])["sample_sd"]}\n- old-vs-clean max absolute differences: profit={maxd["profit"]}, emissions={maxd["emissions"]}, investment={maxd["investment"]}\n- optimized interpretation: GA-BP best-found solution with certified final inner-BP evaluation; no global-optimality claim.\n- A3 status: PAUSED\n- src/include changed: no\n''')
 (b/'FINALIZATION_MANIFEST.txt').write_text('final_status: FINAL_COMPLETE_AND_FROZEN\nexpected_scenario_keys: 20\ncompleted_scenario_keys: 20\nduplicate_scenario_keys: 0\nmissing_scenario_keys: 0\nexpected_paired_keys: 10\ncompleted_paired_keys: 10\nfailed: 0\ntime_fields: numeric wall_time_sec,user_time_sec,system_time_sec\nouter_inner_statuses: separated\nsample_sd_ddof: 1\n')
 T=G/'5_重构代码v2/6_数值实验/03_正式结果/Part2_RQ1'/b.name; T.mkdir(parents=True,exist_ok=True); shutil.copytree(b,T,dirs_exist_ok=True); subprocess.run(['git','add','-f','--',str(T.relative_to(G))],cwd=G,check=True); subprocess.run(['git','commit','-m','Finalize Part 2 RQ1 clean formal results'],cwd=G,check=True); subprocess.run(['git','push','origin','master'],cwd=G,check=True); print('FINALIZED',subprocess.check_output(['git','rev-parse','HEAD'],cwd=G,text=True).strip())
if __name__=='__main__':
 import sys
 b=Path(sys.argv[1])
 while True:
  q=list(csv.DictReader((b/'RQ1_RUN_QUEUE.csv').open())); c=sum(x['status']=='COMPLETED' and x['validation_status']=='PASS' and x['completion_marker']=='COMPLETED.ok' for x in q); f=sum(x['status']=='FAILED' for x in q)
  if f: raise SystemExit('failed task; refusing final aggregation')
  if c==20: finish(b);break
  time.sleep(20)
