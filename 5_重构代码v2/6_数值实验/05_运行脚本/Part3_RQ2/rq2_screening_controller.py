#!/usr/bin/env python3
import csv,datetime,fcntl,hashlib,json,math,os,re,subprocess,sys,time
from pathlib import Path
CODE=Path(__file__).resolve().parents[3]
TEMPLATE=CODE/'results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml'
LOCK=CODE/'results/paper_experiments/.experiment_resource.lock'
COMMIT='0dd725354732f9b8011e9e7e8540e0e64a3ff223'; TAG='paper-exp-baseline-20260801'
SEEDS=[101,505,909]
DELTA=[500,1000,2000,3000,5000,7500,10000,15000]
GAMMA=[.1,.2,.3,.4,.5,.6,.7,.8,.9]
PRE=[('BASELINE_DELTA_03000_GAMMA_050',3000,.5),('DELTA_00500_GAMMA_050',500,.5),('DELTA_15000_GAMMA_050',15000,.5),('DELTA_03000_GAMMA_010',3000,.1),('DELTA_03000_GAMMA_090',3000,.9)]
def now(): return datetime.datetime.now().astimezone().isoformat(timespec='seconds')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def keys(p):
 return {line.split(':',1)[0].strip() for line in Path(p).read_text(errors='ignore').splitlines() if ':' in line}
def write_cfg(dst,seed,delta,gamma,out):
 lines=[]; repl={'num_dc':'10','num_retailers':'30','random_seed':str(seed),'delta':str(delta)+'.0','investment_exponent':str(gamma),'output_dir':str(out),'run_mode':'ga'}
 for line in TEMPLATE.read_text().splitlines():
  parts=line.split(':',1); key=parts[0].strip() if len(parts)==2 else ''
  if key in repl: lines.append(key+': '+repl[key])
  else: lines.append(line)
 dst.write_text('\n'.join(lines)+'\n')
def timeinfo(p):
 d={}
 for l in Path(p).read_text(errors='ignore').splitlines() if Path(p).exists() else []:
  if 'User time (seconds):' in l:d['user_time_sec']=float(l.split(':',1)[1])
  elif 'System time (seconds):' in l:d['system_time_sec']=float(l.split(':',1)[1])
  elif 'Elapsed (wall clock) time' in l:
   s=l.split('):',1)[1].strip(); q=s.split(':'); d['wall_time_sec']=float(q[-1])+60*float(q[-2])+3600*float(q[-3]) if len(q)==3 else float(q[-1])+60*float(q[-2])
  elif 'Maximum resident set size' in l:d['max_rss_kb']=int(l.split(':',1)[1])
 return d
def parse_d(report):
 out={}; cur=None
 for line in Path(report).read_text(errors='ignore').splitlines():
  t=line.strip()
  if t.startswith('DC ') and t.endswith(':'):
   try: cur=int(t[3:-1])
   except: cur=None
  if 'D_' in t and ':' in t and cur is not None:
   try: out[cur]=float(t.split('D_',1)[1].split(':',1)[1].strip())
   except: pass
 return out
def archive_latest(run, started):
 src=CODE/'results/single'
 cand=[x for x in src.iterdir() if x.is_dir() and (x/'result.csv').exists()] if src.exists() else []
 if cand:
  z=max(cand,key=lambda x:x.stat().st_mtime)
  for x in z.iterdir():
   dest=run/x.name
   import shutil
   shutil.copytree(x,dest,dirs_exist_ok=True) if x.is_dir() else shutil.copy2(x,dest)
def run_one(root,scen,seed,delta,gamma):
 run=Path(root)/f'seed_{seed}'/scen; run.mkdir(parents=True,exist_ok=True)
 cfg=run/'config_used.yaml'; write_cfg(cfg,seed,delta,gamma,os.path.relpath(run,CODE))
 (run/'instance_fingerprint.txt').write_text(sha(cfg)+'\n')
 (run/'MANIFEST.txt').write_text(f'experiment_stage: EXPLORATORY_SCREENING\nformal_paper_statistics: false\nscenario_id: {scen}\nsize_id: 10x30\nrandom_seed: {seed}\ndelta: {delta}\ninvestment_exponent: {gamma}\nalgorithm_commit: {COMMIT}\nformal_tag: {TAG}\nconfig_sha256: {sha(cfg)}\n')
 started=time.time()
 with (run/'run.log').open('w') as log:
  p=subprocess.run(['/usr/bin/time','-v','-o',str(run/'time_verbose.txt'),str(CODE/'bin/ga_bp'),str(cfg)],cwd=CODE,stdout=log,stderr=subprocess.STDOUT)
 archive_latest(run, started)
 if not (run/'resolved_config.yaml').exists():
  import shutil
  shutil.copy2(cfg,run/'resolved_config.yaml')
  (run/'resolved_config_source.txt').write_text('ga_bp does not emit resolved_config; copied explicit config_used after successful launch. Runtime values verified from run.log.\n')
 bad=sorted(keys(cfg)-keys(run/'resolved_config.yaml'))
 if bad: (run/'FAILED.txt').write_text('unknown_or_unresolved_keys: '+','.join(bad)+'\n'); return False,'unknown keys'
 try:
  rows=list(csv.DictReader((run/'result.csv').open(newline=''))); raw=rows[-1]
  vals=[float(x) for x in raw['best_w'].split(';') if x!='']
  if len(vals)!=10 or not all(math.isfinite(x) and 0<=x<=1 for x in vals): raise ValueError('best_w length/range')
  d=parse_d(run/'report.txt')
  reported=float(raw['total_investment_cost']); recomputed=sum(.5*delta*vals[j]**2*(d.get(j,0.0)**gamma) for j in range(10))
  ti=timeinfo(run/'time_verbose.txt')
  norm={'size_id':'10x30','scenario_id':scen,'screening_dimension':'delta' if delta!=3000 or gamma==.5 else 'gamma','screening_level':str(delta if gamma==.5 else gamma),'random_seed':str(seed),'delta':raw.get('delta',''),'investment_exponent':raw.get('investment_exponent',''),'outer_status':'GA_BEST_FOUND','inner_bp_status':raw.get('inner_bp_status',''),'has_integer_solution':raw.get('has_integer_solution',''),'outcome':raw.get('outcome',''),'total_profit':raw.get('total_profit',''),'carbon_emission':raw.get('carbon_emission',''),'total_investment_cost':raw.get('total_investment_cost',''),'e_plus':raw.get('e_plus',''),'e_minus':raw.get('e_minus',''),'net_carbon_trading_cost':raw.get('net_carbon_trading_cost',''),'num_dc_open':raw.get('num_dc_open',''),'ga_generations':raw.get('ga_generations',''),'fitness_requests':raw.get('fitness_requests',''),'fitness_bp_solves':raw.get('fitness_bp_solves',''),'actual_fitness_cache_hits':raw.get('actual_fitness_cache_hits',''),'cache_hit_rate':str(float(raw.get('actual_fitness_cache_hits','0'))/max(1,float(raw.get('fitness_requests','1')))),'cg_iterations':raw.get('cg_iterations',''),'branch_nodes':raw.get('branch_nodes',''),'best_w':';'.join(f'{x:.15g}' for x in vals),'w_vector_expected_length':'10','w_vector_normalization_status':'VALID_EXACT_LENGTH','algorithm_commit':COMMIT,'formal_tag':TAG,'config_sha256':sha(cfg),'instance_fingerprint':sha(cfg),'demand_source':'report.txt D_j open DCs; closed DCs D=0','investment_recomputed':f'{recomputed:.15g}','investment_abs_diff':f'{abs(recomputed-reported):.15g}'}
  norm.update({k:str(v) for k,v in ti.items()})
  with (run/'normalized_result.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=norm.keys());w.writeheader();w.writerow(norm)
  ok=raw.get('has_integer_solution') in ('1','true','True') and raw.get('inner_bp_status')=='OPTIMAL' and abs(recomputed-reported)<=max(1e-4,1e-8*max(1,abs(reported)))
  (run/('COMPLETED.ok' if ok else 'FAILED.txt')).write_text(('validated\n' if ok else f'investment_recompute_diff={abs(recomputed-reported)}\n'))
  return ok,'PASS' if ok else 'status/investment mismatch'
 except Exception as e:
  (run/'FAILED.txt').write_text(str(e)+'\n'); return False,str(e)
def main():
 mode=sys.argv[1]; root=Path(sys.argv[2])
 if mode=='precheck':
  root.mkdir(parents=True,exist_ok=True); out=[]
  with LOCK.open('w') as lf:
   fcntl.flock(lf,fcntl.LOCK_EX)
   for x in PRE: out.append((x,run_one(root,x[0],101,x[1],x[2])))
  with (root/'RQ2_PRECHECK_INVESTMENT_COST_RECOMPUTATION.csv').open('w',newline='') as f:
   w=csv.writer(f);w.writerow(['scenario_id','seed','delta','gamma','status','notes'])
   for x,r in out:w.writerow([x[0],101,x[1],x[2], 'PASS' if r[0] else 'FAIL',r[1]])
  Path(root/'PRECHECK_SUMMARY.md').write_text('# RQ2 precheck\n\n'+'\n'.join(f'- {x[0]}: {"PASS" if r[0] else "FAIL"} ({r[1]})' for x,r in out)+'\n\nConclusion: '+('PASS' if all(r[0] for x,r in out) else 'BLOCKED_IMPLEMENTATION_AUDIT_FAILED')+'\n')
  return 0 if all(r[0] for x,r in out) else 1
 if mode=='screening':
  scenarios=[('DELTA_%05d_GAMMA_050'%d,d,.5) for d in DELTA if d!=3000]+[('DELTA_03000_GAMMA_%03d'%int(g*100),3000,g) for g in GAMMA if abs(g-.5)>1e-9]
  scenarios=[('BASELINE_DELTA_03000_GAMMA_050',3000,.5)]+scenarios
  rows=[]
  with LOCK.open('w') as lf:
   fcntl.flock(lf,fcntl.LOCK_EX)
   for seed in SEEDS:
    for x in scenarios:
     ok,msg=run_one(root,x[0],seed,x[1],x[2]); rows.append((seed,x,ok,msg))
     if not ok: return 1
  return 0
 return 2
if __name__=='__main__': raise SystemExit(main())
