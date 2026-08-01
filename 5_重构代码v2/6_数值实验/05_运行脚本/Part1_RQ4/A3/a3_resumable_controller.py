#!/usr/bin/env python3
import csv,datetime,fcntl,json,os,re,shutil,subprocess,sys,time
from pathlib import Path
from a3_validate_run import validate

CODE_ROOT=Path(__file__).resolve().parents[4]
TEMPLATE=CODE_ROOT/'results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916/config_used.yaml'
FIELDS=[]; batch=None
def stamp(): return datetime.datetime.now().astimezone().isoformat(timespec='seconds')
def write_rows(p,rows):
    t=p.with_suffix('.tmp')
    with t.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS); w.writeheader(); w.writerows(rows)
    os.replace(t,p)
def setcfg(src,dst,nd,nr,seed,outrel):
    lines=src.read_text().splitlines(); repl={'num_dc':str(nd),'num_retailers':str(nr),'random_seed':str(seed),'output_dir':outrel}; seen=set(); out=[]
    for line in lines:
        m=re.match(r'^(\s*)([A-Za-z0-9_]+)(\s*:\s*)(.*)$',line)
        if m and m.group(2) in repl: out.append(m.group(1)+m.group(2)+m.group(3)+repl[m.group(2)]); seen.add(m.group(2))
        else: out.append(line)
    if set(repl)-seen: raise RuntimeError('template missing '+','.join(sorted(set(repl)-seen)))
    dst.write_text('\n'.join(out)+'\n')
def update_state(rows,status='RUNNING'):
    c={s:sum(r['status']==s for r in rows) for s in ['PENDING','RUNNING','COMPLETED','FAILED','INTERRUPTED']}
    (batch/'A3_BATCH_STATE.json').write_text(json.dumps({'batch_status':status,**{k.lower()+'_count':v for k,v in c.items()}},indent=2)+'\n')
def main():
    global batch,FIELDS
    if len(sys.argv)<2: raise SystemExit('usage: a3_resumable_controller.py BATCH_DIR [--dry-run]')
    batch=Path(sys.argv[1]).resolve(); dry='--dry-run' in sys.argv; q=batch/'A3_RUN_QUEUE.csv'
    with q.open(newline='') as f:
        reader=csv.DictReader(f); FIELDS=reader.fieldnames; rows=list(reader)
    lockpath=CODE_ROOT/'results/paper_experiments/.experiment_resource.lock'; lockpath.parent.mkdir(parents=True,exist_ok=True)
    with lockpath.open('w') as resource:
        try: fcntl.flock(resource,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: print('SERVER_BUSY_BY_OTHER_EXPERIMENT'); return 2
        runlock=batch/'RUNNING.lock'
        if runlock.exists(): print('BATCH_CONTROLLER_ALREADY_RUNNING'); return 2
        runlock.write_text(str(os.getpid()))
        try:
            for r in rows:
                if r['status']=='RUNNING': r['status']='INTERRUPTED'; r['termination_reason']='controller_restart_or_stale_run'; r['finished_at']=stamp()
            write_rows(q,rows); update_state(rows)
            for r in rows:
                if r['status']!='PENDING': continue
                if (batch/'STOP_AFTER_CURRENT').exists(): (batch/'PAUSED.ok').write_text('pause requested\n'); update_state(rows,'PAUSED'); return 0
                run=Path(r['output_directory']); run.mkdir(parents=True,exist_ok=True); seed=int(r['random_seed'])
                r['status']='RUNNING'; r['attempt']=str(int(r['attempt'])+1); r['started_at']=stamp(); r['controller_session']=str(os.getpid()); r['process_id']=''; write_rows(q,rows); update_state(rows)
                outrel=os.path.relpath(run,CODE_ROOT); cfg=run/'config_used.yaml'; setcfg(TEMPLATE,cfg,int(r['num_dc']),int(r['num_retailers']),seed,outrel)
                (run/'MANIFEST.txt').write_text(f"experiment_type: A3 formal GA-BP task\nsize: {r['size_id']}\nrandom_seed: {seed}\nformal_replication: true\nsource_algorithm_commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223\n")
                if dry: print(f"DRY_RUN order={r['order_id']} {r['size_id']} seed={seed}"); r['status']='PENDING'; write_rows(q,rows); continue
                single_root=CODE_ROOT/'results/single'; before={p.name for p in single_root.iterdir() if p.is_dir()} if single_root.exists() else set(); started=time.time()
                with (run/'run.log').open('w') as log:
                    proc=subprocess.Popen(['/usr/bin/time','-v','-o',str(run/'time_verbose.txt'),str(CODE_ROOT/'bin/ga_bp'),str(cfg)],cwd=CODE_ROOT,stdout=log,stderr=subprocess.STDOUT); r['process_id']=str(proc.pid); write_rows(q,rows); update_state(rows); rc=proc.wait()
                candidates=[p for p in single_root.iterdir() if p.is_dir() and p.name not in before and p.stat().st_mtime>=started-2 and (p/'result.csv').exists()] if single_root.exists() else []
                if candidates:
                    produced=max(candidates,key=lambda p:p.stat().st_mtime)
                    for item in produced.iterdir():
                        target=run/item.name
                        if item.is_dir(): shutil.copytree(item,target,dirs_exist_ok=True)
                        else: shutil.copy2(item,target)
                if not (run/'resolved_config.yaml').exists(): shutil.copy2(cfg,run/'resolved_config.yaml')
                r['exit_code']=str(rc); r['finished_at']=stamp(); ok,errs=validate(run,int(r['num_dc']),int(r['num_retailers']),seed)
                if ok: (run/'COMPLETED.ok').write_text('validated\n'); r['status']='COMPLETED'; r['validation_status']='VALID'; r['completion_marker']='COMPLETED.ok'; r['termination_reason']='normal_or_early_stop'
                else: r['status']='FAILED'; r['validation_status']='INVALID'; r['termination_reason']=';'.join(errs); (run/'FAILED.txt').write_text('\n'.join(errs)+'\n')
                write_rows(q,rows); update_state(rows)
                if r['status']=='FAILED': return 1
            update_state(rows,'DRY_RUN' if dry else 'COMPLETED'); return 0
        finally: runlock.unlink(missing_ok=True)
if __name__=='__main__': raise SystemExit(main())
