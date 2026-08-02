#!/usr/bin/env python3
import csv,json,shutil,subprocess,time,datetime,statistics
from pathlib import Path
C=Path(__file__).resolve().parents[3]; G=C.parent; seeds=[101,202,303,404,505,606,707,808,909,1010]
def readrow(run):
 p=run/'result.csv'
 if not p.exists(): return {}
 with p.open(newline='') as f: return list(csv.DictReader(f))[-1]
def num(x):
 try:return float(x)
 except:return None
def wall(run):
 p=run/'time_verbose.txt'
 if not p.exists():return None
 for l in p.read_text(errors='ignore').splitlines():
  if 'Elapsed (wall clock) time' in l:return l.split(':',1)[-1].strip()
 return None
def finalize(b):
 b=Path(b); rows=list(csv.DictReader((b/'RQ1_RUN_QUEUE.csv').open())); valid=[r for r in rows if r['status']=='COMPLETED' and r['validation_status']=='PASS' and r['completion_marker']=='COMPLETED.ok']
 if len(valid)!=20: return False
 out=[]
 for q in valid:
  rr=readrow(Path(q['output_directory'])); out.append({'size_id':q['size_id'],'random_seed':q['random_seed'],'scenario_id':q['scenario_id'],'solution_status':rr.get('inner_bp_status',''),'integer_solution':rr.get('has_integer_solution',''),'total_profit':rr.get('total_profit',''),'carbon_emission':rr.get('carbon_emission',''),'total_investment_cost':rr.get('total_investment_cost',''),'net_carbon_trading_cost':rr.get('net_carbon_trading_cost',''),'e_plus':rr.get('e_plus',''),'e_minus':rr.get('e_minus',''),'num_dc_open':rr.get('num_dc_open',''),'ga_generations':rr.get('ga_generations',''),'actual_bp_calls':rr.get('fitness_bp_solves',''),'cache_hit_rate':rr.get('actual_fitness_cache_hits',''),'cg_iterations':rr.get('cg_iterations',''),'branch_nodes':rr.get('branch_nodes',''),'wall_time':wall(Path(q['output_directory'])),'config_sha256':__import__('hashlib').sha256((Path(q['output_directory'])/'config_used.yaml').read_bytes()).hexdigest(),'instance_fingerprint':(Path(q['output_directory'])/'instance_fingerprint.txt').read_text().strip()})
 out.sort(key=lambda x:(int(x['random_seed']),x['scenario_id']))
 with (b/'RQ1_FORMAL_SCENARIO_RESULTS.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=out[0]);w.writeheader();w.writerows(out)
 by={(x['random_seed'],x['scenario_id']):x for x in out}; paired=[]
 for s in seeds:
  n=by[(str(s),'NO_INVESTMENT')]; o=by[(str(s),'OPTIMIZED_INVESTMENT')]; np=num(n['total_profit']);op=num(o['total_profit']);ne=num(n['carbon_emission']);oe=num(o['carbon_emission']);
  paired.append({'random_seed':s,'no_investment_profit':n['total_profit'],'optimized_investment_profit':o['total_profit'],'profit_change_absolute':op-np,'profit_change_percent':(op-np)/abs(np)*100 if np else '','no_investment_emissions':n['carbon_emission'],'optimized_investment_emissions':o['carbon_emission'],'emissions_change_absolute':oe-ne,'emissions_reduction_percent':(ne-oe)/ne*100 if ne else '','optimized_investment_cost':o['total_investment_cost'],'no_investment_wall_time':n['wall_time'],'optimized_investment_wall_time':o['wall_time'],'optimized_mean_positive_w':'UNAVAILABLE_FROM_FROZEN_INTERFACE','network_structure_changed':n['num_dc_open']!=o['num_dc_open']})
 with (b/'RQ1_FORMAL_PAIRED_RESULTS.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=paired[0]);w.writeheader();w.writerows(paired)
 vals=[num(x['profit_change_absolute']) for x in paired if num(x['profit_change_absolute']) is not None]; em=[num(x['emissions_reduction_percent']) for x in paired if num(x['emissions_reduction_percent']) is not None]
 summary=[{'metric':'profit_change_absolute_mean','value':statistics.mean(vals) if vals else ''},{'metric':'profit_change_absolute_sd','value':statistics.stdev(vals) if len(vals)>1 else ''},{'metric':'emissions_reduction_percent_mean','value':statistics.mean(em) if em else ''},{'metric':'emissions_reduction_percent_sd','value':statistics.stdev(em) if len(em)>1 else ''}]
 with (b/'RQ1_FORMAL_SUMMARY.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=['metric','value']);w.writeheader();w.writerows(summary)
 with (b/'RQ1_PAPER_TABLE.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=['comparison','mean_profit_change','sd_profit_change','mean_emissions_reduction_percent','sd_emissions_reduction_percent']);w.writeheader();w.writerow({'comparison':'Optimized investment vs no investment','mean_profit_change':summary[0]['value'],'sd_profit_change':summary[1]['value'],'mean_emissions_reduction_percent':summary[2]['value'],'sd_emissions_reduction_percent':summary[3]['value']})
 (b/'RQ1_FORMAL_RESULTS_REPORT.md').write_text(f'# RQ1 Formal Results Report\n\n- Valid completed paired runs: {len(paired)}/10\n- Objective: total_profit\n- Optimized scenario: heuristic best-found; no global-optimality claim.\n- Unavailable fields are explicitly marked; no unsupported costs inferred.\n- Mean absolute profit change: {summary[0]["value"]}\n- Mean emissions reduction percent: {summary[2]["value"]}\n')
 (b/'BATCH_MANIFEST.txt').write_text(f'experiment_type: Part2 RQ1 formal paired batch\nformal_replications: 20\nvalid_completed: 20\nsize: 10x30\nseeds: {seeds}\nalgorithm_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\nformal_tag: paper-exp-baseline-20260801\n')
 T=G/'5_重构代码v2/6_数值实验/03_正式结果/Part2_RQ1'/b.name; T.mkdir(parents=True,exist_ok=True); shutil.copytree(b,T,dirs_exist_ok=True); subprocess.run(['git','add','--',str(T.relative_to(G))],cwd=G,check=True); subprocess.run(['git','commit','-m','Add Part 2 RQ1 formal results'],cwd=G,check=True); subprocess.run(['git','push','origin','master'],cwd=G,check=True); return True
if __name__=='__main__':
 b=Path(__import__('sys').argv[1]);
 while True:
  q=list(csv.DictReader((b/'RQ1_RUN_QUEUE.csv').open())); c=sum(x['status']=='COMPLETED' and x['validation_status']=='PASS' and x['completion_marker']=='COMPLETED.ok' for x in q); f=sum(x['status']=='FAILED' for x in q)
  if f: raise SystemExit('RQ1 failed task; no aggregation')
  if c==20: print('FINALIZED',finalize(b)); break
  print('waiting',c,'/20',flush=True); time.sleep(30)
