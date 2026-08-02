#!/usr/bin/env python3
import csv,datetime,fcntl,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
C=Path(__file__).resolve().parents[3]; T=C/'results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml'; SEEDS=[101,202,303,404,505,606,707,808,909,1010]
def ts():return datetime.datetime.now().astimezone().isoformat(timespec='seconds')
def rewrite(dst,scen,seed,outrel):
 repl={'num_dc':'10','num_retailers':'30','random_seed':str(seed),'output_dir':outrel,'run_mode':'fixed_w' if scen=='NO_INVESTMENT' else 'ga','min_w':'0.0','max_w':'0.0' if scen=='NO_INVESTMENT' else '1.0'}; out=[]; seen=set()
 for line in T.read_text().splitlines():
  m=re.match(r'^(\s*)([A-Za-z0-9_]+)(\s*:\s*)(.*)$',line)
  if m and m.group(2) in repl: out.append(m.group(1)+m.group(2)+m.group(3)+repl[m.group(2)]); seen.add(m.group(2))
  else: out.append(line)
 if scen=='NO_INVESTMENT': out.append('fixed_w: ['+', '.join(['0.0']*10)+']')
 dst.write_text('\n'.join(out)+'\n')
def fp(cfg):
 a=[]
 for l in cfg.read_text().splitlines():
  if any(l.startswith(k+':') for k in ('run_mode','min_w','max_w','output_dir','fixed_w')):continue
  a.append(l)
 return hashlib.sha256(('\n'.join(a)+'\n').encode()).hexdigest()
def time_fields(p):
 d={};
 if p.exists():
  for l in p.read_text(errors='ignore').splitlines():
   if 'User time (seconds):' in l:d['user_time_sec']=float(l.split(':',1)[1].strip())
   elif 'System time (seconds):' in l:d['system_time_sec']=float(l.split(':',1)[1].strip())
   elif 'Elapsed (wall clock) time' in l:
    s=l.split('):',1)[1].strip(); parts=s.split(':');
    if len(parts)==3:d['wall_time_sec']=float(parts[0])*3600+float(parts[1])*60+float(parts[2])
    elif len(parts)==2:d['wall_time_sec']=float(parts[0])*60+float(parts[1])
    else:d['wall_time_sec']=float(parts[0])
 return d
def parse_result(run,scen,seed,cfg):
 rows=list(csv.DictReader((run/'result.csv').open(newline=''))); raw=rows[-1]; report=(run/'report.txt').read_text(errors='ignore') if (run/'report.txt').exists() else ''
 ws=[]; opened=[]; current=None
 for line in report.splitlines():
  m=re.search(r'DC\s+(\d+):',line)
  if m: current=int(m.group(1))
  if current is not None and '是否开设: 是' in line: opened.append(current)
  m=re.search(r'w_(\d+):\s*([-+0-9.eE]+)',line)
  if m:
   while len(ws)<=int(m.group(1)):ws.append(0.0)
   ws[int(m.group(1))]=float(m.group(2))
 if not ws and raw.get('best_w'): ws=[float(x) for x in raw['best_w'].split(';') if x!='']
 status=raw.get('inner_bp_status',''); integer=raw.get('has_integer_solution','') in ('1','true','True'); certified=(status=='OPTIMAL' and integer)
 outer='FIXED_W_BP' if scen=='NO_INVESTMENT' else 'GA_BP'; outer_status='NOT_APPLICABLE' if scen=='NO_INVESTMENT' else 'GA_BEST_FOUND'
 d={'size_id':'10x30','random_seed':seed,'scenario_id':scen,'outer_method':outer,'outer_solution_status':outer_status,'raw_solver_status':status,'final_inner_bp_status':status,'final_inner_bp_certified':str(certified).lower(),'integer_solution':int(integer),'termination_reason':'normal_or_early_stop','total_profit':raw.get('total_profit',''),'carbon_emission':raw.get('carbon_emission',''),'carbon_cap':raw.get('carbon_cap',''),'e_plus':raw.get('e_plus',''),'e_minus':raw.get('e_minus',''),'net_carbon_trading_cost':raw.get('net_carbon_trading_cost',''),'total_investment_cost':raw.get('total_investment_cost',''),'num_dc_open':raw.get('num_dc_open',''),'open_dc_set':';'.join(map(str,sorted(set(opened)))) if opened else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','w_vector':';'.join(f'{x:.12g}' for x in ws) if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','positive_w_count':sum(x>1e-9 for x in ws) if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','mean_positive_w':sum(x for x in ws if x>1e-9)/sum(x>1e-9 for x in ws) if any(x>1e-9 for x in ws) else (0.0 if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE'),'mean_all_w':sum(ws)/len(ws) if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','max_w':max(ws) if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','min_positive_w':min((x for x in ws if x>1e-9),default=0.0) if ws else 'UNAVAILABLE_FROM_FROZEN_INTERFACE','ga_generations':raw.get('ga_generations','NOT_APPLICABLE') if scen!='NO_INVESTMENT' else 'NOT_APPLICABLE','actual_bp_calls':raw.get('fitness_bp_solves','NOT_APPLICABLE') if scen!='NO_INVESTMENT' else 'NOT_APPLICABLE','cache_hit_rate':(float(raw.get('actual_fitness_cache_hits','0'))/float(raw.get('fitness_requests','1'))) if scen!='NO_INVESTMENT' and raw.get('fitness_requests') else ('NOT_APPLICABLE' if scen=='NO_INVESTMENT' else ''),'cg_iterations':raw.get('cg_iterations',''),'branch_nodes':raw.get('branch_nodes',''),'algorithm_commit':'0dd725354732f9b8011e9e7e8540e0e64a3ff223','formal_tag':'paper-exp-baseline-20260801','config_sha256':hashlib.sha256(cfg.read_bytes()).hexdigest(),'instance_fingerprint':fp(cfg)}; d.update(time_fields(run/'time_verbose.txt')); return d,raw
def archive(run,start):
 s=C/'results/single'; cand=[]
 if s.exists(): cand=[p for p in s.iterdir() if p.is_dir() and p.stat().st_mtime>=start-2 and (p/'result.csv').exists()]
 if cand:
  p=max(cand,key=lambda x:x.stat().st_mtime)
  for x in p.iterdir(): shutil.copytree(x,run/x.name,dirs_exist_ok=True) if x.is_dir() else shutil.copy2(x,run/x.name)
def one(run,scen,seed):
 run.mkdir(parents=True,exist_ok=True); cfg=run/'config_used.yaml'; rewrite(cfg,scen,seed,os.path.relpath(run,C)); fingerprint=fp(cfg); (run/'instance_fingerprint.txt').write_text(fingerprint+'\n'); (run/'MANIFEST.txt').write_text(f'experiment_type: Part2 RQ1 clean formal rerun\nsize_id: 10x30\nrandom_seed: {seed}\nscenario_id: {scen}\nalgorithm_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\nformal_tag: paper-exp-baseline-20260801\nconfig_sha256: {hashlib.sha256(cfg.read_bytes()).hexdigest()}\ninstance_fingerprint: {fingerprint}\n')
 start=time.time()
 with (run/'run.log').open('w') as log: proc=subprocess.Popen(['/usr/bin/time','-v','-o',str(run/'time_verbose.txt'),str(C/'bin/ga_bp'),str(cfg)],cwd=C,stdout=log,stderr=subprocess.STDOUT); proc.wait(); rc=proc.returncode
 archive(run,start); shutil.copy2(cfg,run/'resolved_config.yaml') if not (run/'resolved_config.yaml').exists() else None
 ok=False; errs=[]
 try:
  norm,raw=parse_result(run,scen,seed,cfg); ok=norm['final_inner_bp_certified']=='true';
  if scen=='NO_INVESTMENT' and abs(float(norm['total_investment_cost']))>1e-7:ok=False;errs.append('nonzero no-investment cost')
  (run/'normalized_result.csv').write_text(','.join(norm.keys())+'\n'+','.join(str(norm[k]) for k in norm)+'\n')
 except Exception as e:errs.append(str(e))
 if ok:(run/'COMPLETED.ok').write_text('validated\n')
 else:(run/'FAILED.txt').write_text('\n'.join(errs)+'\n')
 return ok,errs

def precheck(b):
 b=Path(b); results=[]
 for s in ('NO_INVESTMENT','OPTIMIZED_INVESTMENT'):results.append((s,one(b/'seed_101'/s,s,101)))
 same=(b/'seed_101/NO_INVESTMENT/instance_fingerprint.txt').read_text()==(b/'seed_101/OPTIMIZED_INVESTMENT/instance_fingerprint.txt').read_text(); ok=all(x[1][0] for x in results) and same
 (b/'CLEAN_PRECHECK_SUMMARY.md').write_text('# Clean RQ1 precheck\n\n'+ '\n'.join(f'- {s}: {"PASS" if x[0] else "FAIL"} {x[1]}' for s,x in results)+f'\n- same_instance_fingerprint: {same}\n- conclusion: {"PASS" if ok else "FAIL"}\n'); print('PASS' if ok else 'FAIL'); return 0 if ok else 1
def init(b):
 b=Path(b); rows=[];i=1
 for seed in SEEDS:
  for s in ('NO_INVESTMENT','OPTIMIZED_INVESTMENT'):rows.append({'order_id':i,'size_id':'10x30','random_seed':seed,'scenario_id':s,'status':'PENDING','attempt':0,'started_at':'','finished_at':'','validation_status':'','completion_marker':'','output_directory':str(b/'runs'/f'seed_{seed}'/s),'termination_reason':''});i+=1
 b.mkdir(parents=True,exist_ok=True)
 with (b/'RQ1_RUN_QUEUE.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
 (b/'RQ1_BATCH_STATE.json').write_text(json.dumps({'batch_status':'INITIALIZED','total':20},indent=2)+'\n')
def run(b):
 b=Path(b); q=b/'RQ1_RUN_QUEUE.csv'; rows=list(csv.DictReader(q.open())); fields=rows[0].keys(); lock=C/'results/paper_experiments/.experiment_resource.lock'
 with lock.open('w') as lf:
  fcntl.flock(lf,fcntl.LOCK_EX)
  for r in rows:
   if r['status']!='PENDING':continue
   r['status']='RUNNING';r['attempt']=str(int(r['attempt'])+1);r['started_at']=ts()
   with q.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
   ok,errs=one(Path(r['output_directory']),r['scenario_id'],int(r['random_seed']));r['status']='COMPLETED' if ok else 'FAILED';r['validation_status']='PASS' if ok else 'FAIL';r['completion_marker']='COMPLETED.ok' if ok else '';r['finished_at']=ts();r['termination_reason']='normal' if ok else ';'.join(errs)
   with q.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
   if not ok:return 1
 (b/'RQ1_BATCH_STATE.json').write_text(json.dumps({'batch_status':'COMPLETED','total':20,'completed':20,'failed':0},indent=2)+'\n');return 0
if __name__=='__main__':
 m=sys.argv[1];p=sys.argv[2];sys.exit(precheck(p) if m=='precheck' else (init(p) or 0) if m=='init' else run(p))
