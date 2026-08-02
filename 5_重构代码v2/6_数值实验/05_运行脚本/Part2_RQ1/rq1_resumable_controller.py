#!/usr/bin/env python3
import csv,datetime,fcntl,hashlib,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
CODE_ROOT=Path(__file__).resolve().parents[3]
TEMPLATE=CODE_ROOT/'results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml'
SEEDS=[101,202,303,404,505,606,707,808,909,1010]
COMMON={'num_dc':'10','num_retailers':'30','random_seed':None,'output_dir':None,'run_mode':None,'min_w':None,'max_w':None}
def now(): return datetime.datetime.now().astimezone().isoformat(timespec='seconds')
def stamp(): return datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
def rewrite(src,dst,scenario,seed,outrel):
    repl={'num_dc':'10','num_retailers':'30','random_seed':str(seed),'output_dir':outrel,'run_mode':'fixed_w' if scenario=='NO_INVESTMENT' else 'ga','min_w':'0.0','max_w':'0.0' if scenario=='NO_INVESTMENT' else '1.0'}
    out=[]; seen=set()
    for line in src.read_text().splitlines():
        m=re.match(r'^(\s*)([A-Za-z0-9_]+)(\s*:\s*)(.*)$',line)
        if m and m.group(2) in repl:
            out.append(m.group(1)+m.group(2)+m.group(3)+repl[m.group(2)]); seen.add(m.group(2))
        else: out.append(line)
    if scenario=='NO_INVESTMENT': out.append('fixed_w: ['+', '.join(['0.0']*10)+']')
    dst.write_text('\n'.join(out)+'\n')
def fingerprint(cfg):
    lines=[]
    for l in cfg.read_text().splitlines():
        if any(l.startswith(k+':') for k in ['run_mode','min_w','max_w','output_dir','fixed_w']): continue
        lines.append(l)
    return hashlib.sha256(('\n'.join(lines)+'\n').encode()).hexdigest()
def archive(run,started):
    single=CODE_ROOT/'results/single'; cand=[]
    if single.exists():
        for p in single.iterdir():
            if p.is_dir() and p.stat().st_mtime>=started-2 and (p/'result.csv').exists(): cand.append(p)
    if cand:
        p=max(cand,key=lambda x:x.stat().st_mtime)
        for x in p.iterdir():
            y=run/x.name
            if x.is_dir(): shutil.copytree(x,y,dirs_exist_ok=True)
            else: shutil.copy2(x,y)
def parse_result(run):
    p=run/'result.csv'
    if not p.exists(): return None
    with p.open(newline='') as f: rows=list(csv.DictReader(f))
    return rows[-1] if rows else None
def validate(run,scenario):
    r=parse_result(run); errs=[]
    if r is None: return False,['missing result.csv']
    if r.get('has_integer_solution','') not in ('1','true','True'): errs.append('no integer solution')
    if r.get('inner_bp_status','')!='OPTIMAL': errs.append('inner BP not OPTIMAL')
    try: float(r.get('total_profit',''))
    except: errs.append('total_profit missing/nonfinite')
    if scenario=='NO_INVESTMENT':
        try:
            if abs(float(r.get('total_investment_cost','nan'))) > 1e-7: errs.append('NO_INVESTMENT cost nonzero')
        except: errs.append('investment cost missing')
    return not errs,errs
def run_one(run,scenario,seed):
    run.mkdir(parents=True,exist_ok=True); cfg=run/'config_used.yaml'; outrel=os.path.relpath(run,CODE_ROOT); rewrite(TEMPLATE,cfg,scenario,seed,outrel); (run/'instance_fingerprint.txt').write_text(fingerprint(cfg)+'\n')
    (run/'MANIFEST.txt').write_text(f'experiment_type: Part2 RQ1 {scenario}\nformal_replication: true\nsize: 10x30\nrandom_seed: {seed}\nscenario_id: {scenario}\nalgorithm_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\nformal_tag: paper-exp-baseline-20260801\nconfig_sha256: {hashlib.sha256(cfg.read_bytes()).hexdigest()}\ninstance_fingerprint: {fingerprint(cfg)}\n')
    before={p.name for p in (CODE_ROOT/'results/single').iterdir() if p.is_dir()} if (CODE_ROOT/'results/single').exists() else set(); started=time.time()
    with (run/'run.log').open('w') as log:
        proc=subprocess.Popen(['/usr/bin/time','-v','-o',str(run/'time_verbose.txt'),str(CODE_ROOT/'bin/ga_bp'),str(cfg)],cwd=CODE_ROOT,stdout=log,stderr=subprocess.STDOUT); (run/'process.pid').write_text(str(proc.pid)); rc=proc.wait()
    archive(run,started)
    if not (run/'resolved_config.yaml').exists(): shutil.copy2(cfg,run/'resolved_config.yaml')
    ok,errs=validate(run,scenario); (run/'validation.txt').write_text(('PASS\n' if ok else 'FAIL\n')+'\n'.join(errs)+'\n')
    if ok: (run/'COMPLETED.ok').write_text('validated\n')
    else: (run/'FAILED.txt').write_text('\n'.join(errs)+'\n')
    return ok,errs,rc

def precheck(d):
    d=Path(d); d.mkdir(parents=True,exist_ok=True); results=[]
    for scen in ['NO_INVESTMENT','OPTIMIZED_INVESTMENT']:
        run=d/'seed_101'/scen; ok,errs,rc=run_one(run,scen,101); results.append((scen,ok,errs,rc))
    same=(d/'seed_101'/'NO_INVESTMENT'/'instance_fingerprint.txt').read_text()==(d/'seed_101'/'OPTIMIZED_INVESTMENT'/'instance_fingerprint.txt').read_text()
    passed=all(x[1] for x in results) and same
    (d/'PRECHECK_SUMMARY.md').write_text('# RQ1 paired precheck\n\n'+'\n'.join(f'- {s}: {"PASS" if ok else "FAIL"}; rc={rc}; {errs}' for s,ok,errs,rc in results)+f'\n- same_instance_fingerprint: {same}\n- conclusion: {"PASS" if passed else "FAIL"}\n')
    print('PRECHECK', 'PASS' if passed else 'FAIL'); return 0 if passed else 1
def init_batch(b):
    b=Path(b); b.mkdir(parents=True,exist_ok=True); rows=[]; i=1
    for seed in SEEDS:
        for scen in ['NO_INVESTMENT','OPTIMIZED_INVESTMENT']:
            run=b/'runs'/f'seed_{seed}'/scen; rows.append({'order_id':str(i),'size_id':'10x30','random_seed':str(seed),'scenario_id':scen,'status':'PENDING','attempt':'0','started_at':'','finished_at':'','process_id':'','output_directory':str(run),'validation_status':'','completion_marker':'','termination_reason':'','notes':''}); i+=1
    with (b/'RQ1_RUN_QUEUE.csv').open('w',newline='') as f: w=csv.DictWriter(f,fieldnames=rows[0]); w.writeheader(); w.writerows(rows)
    (b/'RQ1_BATCH_STATE.json').write_text(json.dumps({'batch_status':'INITIALIZED','total':20,'completed':0,'failed':0,'pending':20},indent=2)+'\n'); print('initialized',b)
def run_batch(b):
    b=Path(b); q=b/'RQ1_RUN_QUEUE.csv'; rows=list(csv.DictReader(q.open())); fields=list(rows[0]); lock=CODE_ROOT/'results/paper_experiments/.experiment_resource.lock'; lock.parent.mkdir(parents=True,exist_ok=True)
    with lock.open('w') as lf:
        fcntl.flock(lf,fcntl.LOCK_EX)
        for row in rows:
            if row['status']!='PENDING': continue
            scen=row['scenario_id']; seed=int(row['random_seed']); run=Path(row['output_directory']); row.update(status='RUNNING',attempt=str(int(row['attempt'])+1),started_at=now());
            with q.open('w',newline='') as f: w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
            ok,errs,rc=run_one(run,scen,seed); row.update(status='COMPLETED' if ok else 'FAILED',finished_at=now(),validation_status='PASS' if ok else 'FAIL',completion_marker='COMPLETED.ok' if ok else '',termination_reason='normal' if ok else ';'.join(errs),notes='')
            with q.open('w',newline='') as f: w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
            counts={s:sum(x['status']==s for x in rows) for s in ['PENDING','RUNNING','COMPLETED','FAILED']}; (b/'RQ1_BATCH_STATE.json').write_text(json.dumps({'batch_status':'RUNNING' if ok else 'FAILED',**counts},indent=2)+'\n')
            if not ok: return 1
    (b/'RQ1_BATCH_STATE.json').write_text(json.dumps({'batch_status':'COMPLETED','total':20,'completed':20,'failed':0,'pending':0},indent=2)+'\n'); return 0
if __name__=='__main__':
    if len(sys.argv)<3: raise SystemExit('usage: precheck|init|run PATH')
    m=sys.argv[1]; p=sys.argv[2]; raise SystemExit(precheck(p) if m=='precheck' else (init_batch(p) or 0) if m=='init' else run_batch(p))
