import csv,hashlib,json,shutil,subprocess,sys,time
from pathlib import Path
P=Path('/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/worktrees/real-case-adapter-intracity-carbon/5_重构代码v2')
D=Path('/home/peizeyu2026/smart_wolf_project/datasets/YRD-Steel-v02/data/processed/v0.2.0-candidate')
B=Path('/tmp/real_case_formal_hp'); R=P/'results/real_case/formal_distance_high_precision_20260804_130000'; Q=14442.842886043889
SEEDS=[101,202,303,404,505,606,707,808,909,1010]; DISTS=[0,5,10,15,20]
CFG={'num_dc':8,'num_retailers':15,'population_size':30,'max_generation':50,'crossover_rate':0.7,'mutation_rate':-1,'elitism':True,'chromosome_length':10,'min_w':0.0,'max_w':1.0,'early_stop':True,'convergence_generations':10,'convergence_tolerance':0.001,'pricing_algorithm':0,'pricing_max_cols_per_dc':3,'pricing_adaptive_cols':False,'dual_smooth_alpha':0.0,'rc_eps':1e-6,'max_cg_iterations':2000,'max_branch_nodes':10000,'bp_time_limit_sec':600,'bp_relative_gap':0.0,'parallel_fitness':True,'num_threads':8,'parallel_pricing':False,'cplex_threads':1,'use_sqrt_investment':True,'use_invest_in_column':True,'investment_exponent':0.5,'delta':3000.0,'transport_cost_mode':'EXPLICIT_ARC_COST','carbon_price':2.0,'carbon_quota':Q,'dataset_commit':'b9fc758dbe11762882bf66cbbc7672da0918adb7'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def yaml(d):return ''.join(f'{k}: {str(v).lower() if isinstance(v,bool) else v}\n' for k,v in d.items())
def run(d,s):
 out=R/f'D{d:02d}'/f'seed_{s}';out.mkdir(parents=True,exist_ok=True);cfg=dict(CFG,random_seed=s,intracity_distance_km=d)
 txt=yaml(cfg);(out/'config_used.yaml').write_text(txt);(out/'resolved_config.yaml').write_text(txt)
 dry=R/'_dry_tmp'/f'D{d:02d}';shutil.rmtree(dry,ignore_errors=True);dry.mkdir(parents=True)
 subprocess.run([str(B),'--data-dir',str(D),'--output-dir',str(dry),'--intracity-distance-km',str(d),'--random-seed','101','--carbon-price','2','--carbon-quota',str(Q),'--dry-run'],cwd=P,check=True,stdout=subprocess.DEVNULL)
 shutil.copy2(dry/'solver_arc_parameters.csv',out/'solver_arc_parameters.csv')
 cmd=[str(B),'--data-dir',str(D),'--output-dir',str(out),'--intracity-distance-km',str(d),'--random-seed',str(s),'--carbon-price','2','--carbon-quota',str(Q),'--formal-run'];(out/'command.txt').write_text(' '.join(cmd)+'\n');t=time.time()
 with (out/'run.stdout').open('w') as so,(out/'time_verbose.txt').open('w') as te: rc=subprocess.run(['/usr/bin/time','-v']+cmd,cwd=P,stdout=so,stderr=te).returncode
 shutil.copy2(out/'time_verbose.txt',out/'run.stderr');row=next(csv.DictReader((out/'result.csv').open()))
 def num(k):
  try:return float(row[k])
  except:return 1e99
 ok=rc==0 and row['final_inner_bp_status']=='OPTIMAL' and row['integer_solution']=='true' and num('relative_gap')==0 and abs(num('objective_decomposition_error'))<=1e-5 and abs(num('emission_decomposition_error'))<=1e-8
 fp=hashlib.sha256(f'{d}|{s}|{Q}|{sha(out/"solver_arc_parameters.csv")}'.encode()).hexdigest();(out/'resolved_real_case_instance.json').write_text(json.dumps({'distance_km':d,'random_seed':s,'carbon_price':2.0,'carbon_quota':Q,'transport_cost_mode':'EXPLICIT_ARC_COST','dataset_commit':'b9fc758dbe11762882bf66cbbc7672da0918adb7','instance_fingerprint':fp},indent=2)+'\n')
 vals=[x for x in row['open_dc_ids'].split(';') if x];ws=row['best_w'].split(';');
 with (out/'network_decision.csv').open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['dc_id','open','w']);
  for j in range(8):w.writerow([f'C{j+1:02d}', 'true' if f'C{j+1:02d}' in vals else 'false', ws[j] if j<len(ws) else '0'])
 (out/'RUN_MANIFEST.txt').write_text(f'experiment_type=PART5_FORMAL_DISTANCE_SENSITIVITY_HIGH_PRECISION\nresult_version=HIGH_PRECISION_V2\nnumeric_output_precision=max_digits10\nsupersedes_result_version=ROUNDED_OUTPUT_V1\nvalidation_status={"PASS" if ok else "FAIL"}\ndistance_km={d}\nga_seed={s}\ncarbon_price=2\ncarbon_quota={Q}\ntransport_cost_mode=EXPLICIT_ARC_COST\nexecution_code_commit=1566b7fca4d9311e1fe8195834ea46c770b75445\ndataset_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\ninstance_fingerprint={fp}\n')
 files=[p for p in out.iterdir() if p.is_file() and p.name!='HASH_MANIFEST.json'];(out/'HASH_MANIFEST.json').write_text(json.dumps({p.name:sha(p) for p in sorted(files)},indent=2)+'\n');(out/('COMPLETED.ok' if ok else 'FAILED.validation')).write_text('validation_status='+('PASS' if ok else 'FAIL')+'\n')
 print(f'{d}km seed={s}: {"PASS" if ok else "FAIL"}',flush=True);return ok,row
if __name__=='__main__':
 R.mkdir(parents=True,exist_ok=True);results=[]
 for s in SEEDS:
  for d in DISTS:
   ok,row=run(d,s);results.append((d,s,ok))
   if not ok: raise SystemExit('high precision batch stopped after failed run')
 print('COMPLETED',len(results),'/50')
