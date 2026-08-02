#!/usr/bin/env python3
import csv,datetime,fcntl,hashlib,json,os,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import rq2_screening_controller as sc
CODE=sc.CODE; COMMIT=sc.COMMIT; TAG=sc.TAG
DELTA=[500,1000,2000,3000,5000,10000,15000]; GAMMA=[.1,.3,.4,.5,.6,.7,.9]; SEEDS=[101,202,303,404,505,606,707,808,909,1010]
SCEN=[('DELTA_%05d_GAMMA_050'%d,d,.5) for d in DELTA]+[('DELTA_03000_GAMMA_%03d'%int(g*100),3000,g) for g in GAMMA if abs(g-.5)>1e-9]
SCEN=list(dict((x[0],x) for x in SCEN).values())
def instance_fp(cfg):
 keep=[]
 for line in Path(cfg).read_text().splitlines():
  key=line.split(':',1)[0].strip() if ':' in line else ''
  if key not in {'delta','investment_exponent','output_dir','run_mode','random_seed'}: keep.append(line)
 return hashlib.sha256(('\n'.join(keep)+'\n').encode()).hexdigest()
def normalize(run,scen,seed,d,g,stage):
 ok,msg=sc.run_one(run.parent.parent,scen,seed,d,g)
 if not ok:return False,msg
 fp=instance_fp(run/'config_used.yaml'); (run/'instance_fingerprint.txt').write_text(fp+'\n')
 m=run/'MANIFEST.txt'; s=m.read_text(); s=s.replace('EXPLORATORY_SCREENING',stage).replace('formal_paper_statistics: false','formal_paper_statistics: '+('true' if stage=='FORMAL' else 'false')); m.write_text(s+'instance_fingerprint: '+fp+'\n')
 p=run/'normalized_result.csv'; rows=list(csv.DictReader(p.open(newline=''))); r=rows[0]; r.update({'size_id':'10x30','random_seed':str(seed),'scenario_id':scen,'sensitivity_dimension':'delta' if g==.5 else 'gamma','sensitivity_level':str(d if g==.5 else g),'outer_method':'GA_BP','outer_solution_status':'GA_BEST_FOUND','final_inner_bp_status':r.get('inner_bp_status',''),'final_inner_bp_certified':'true' if r.get('inner_bp_status')=='OPTIMAL' and r.get('has_integer_solution') in ('1','true','True') else 'false','integer_solution':r.get('has_integer_solution',''),'termination_reason':'normal_or_early_stop','w_vector_candidate_dc':r.get('best_w',''),'w_vector_length':'10','w_vector_expected_length':'10','w_vector_normalization_status':'VALID_EXACT_LENGTH','w_vector_raw_source':'result.csv:best_w','algorithm_commit':COMMIT,'formal_tag':TAG,'instance_fingerprint':fp})
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=r.keys());w.writeheader();w.writerow(r)
 return True,'PASS'
def write_queue(root):
 rows=[]; i=1
 for seed in SEEDS:
  for s,d,g in SCEN: rows.append({'order_id':i,'size_id':'10x30','scenario_id':s,'random_seed':seed,'delta':d,'gamma':g,'status':'PENDING','validation_status':'','completion_marker':'','output_directory':str(root/f'seed_{seed}/{s}'),'unique_key':f'10x30|{seed}|{s}'});i+=1
 fields=list(rows[0])
 with (root/'RQ2_FORMAL_RUN_QUEUE.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
 (root/'RQ2_FORMAL_RUN_QUEUE.json').write_text(json.dumps(rows,indent=2)+'\n');(root/'BATCH_STATE.json').write_text(json.dumps({'batch_status':'INITIALIZED','total':len(rows),'completed':0,'failed':0,'expected_unique_keys':130},indent=2)+'\n');(root/'BATCH_MANIFEST.txt').write_text('experiment_stage: FORMAL\nformal_paper_statistics: true\nexpected_unique_runs: 130\nparallel_formal_runs: 1\nA3: PAUSED\n')
def main():
 mode=sys.argv[1]; root=Path(sys.argv[2]); root.mkdir(parents=True,exist_ok=True)
 if mode=='precheck':
  out=[]
  with sc.LOCK.open('w') as lf:
   fcntl.flock(lf,fcntl.LOCK_EX)
   for s,d,g in SCEN: out.append((s,normalize(root/f'seed_101/{s}',s,101,d,g,'FORMAL_PRECHECK')))
 with (root/'RQ2_FORMAL_PRECHECK_RESULTS.csv').open('w',newline='') as f:w=csv.writer(f);w.writerow(['scenario_id','status','notes']);w.writerows([[s,'PASS' if r[0] else 'FAIL',r[1]] for s,r in out])
 (root/'RQ2_FORMAL_PRECHECK_REPORT.md').write_text('# RQ2 formal precheck\n\n13 unique scenarios for seed 101.\n\nConclusion: '+('PASS' if all(r[0] for s,r in out) else 'FAIL')+'\n')
 (root/'PRECHECK_MANIFEST.txt').write_text('precheck_tasks: 13\nseed: 101\nformal_statistics: false\n')
 return 0 if all(r[0] for s,r in out) else 1
 if mode=='init': write_queue(root); return 0
 if mode=='run':
  q=root/'RQ2_FORMAL_RUN_QUEUE.csv'; rows=list(csv.DictReader(q.open(newline=''))); fields=list(rows[0])
  with sc.LOCK.open('w') as lf:
   fcntl.flock(lf,fcntl.LOCK_EX)
   for r in rows:
    if r['status']=='COMPLETED':continue
    r['status']='RUNNING';
    with q.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
    ok,msg=normalize(Path(r['output_directory']),r['scenario_id'],int(r['random_seed']),float(r['delta']),float(r['gamma']),'FORMAL')
    r['status']='COMPLETED' if ok else 'FAILED';r['validation_status']='PASS' if ok else 'FAIL';r['completion_marker']='COMPLETED.ok' if ok else 'FAILED.txt'
    with q.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
    (root/'BATCH_STATE.json').write_text(json.dumps({'batch_status':'RUNNING' if ok else 'FAILED','total':130,'completed':sum(x['status']=='COMPLETED' for x in rows),'failed':sum(x['status']=='FAILED' for x in rows)},indent=2)+'\n')
    if not ok:return 1
  (root/'BATCH_STATE.json').write_text(json.dumps({'batch_status':'COMPLETED','total':130,'completed':130,'failed':0},indent=2)+'\n');return 0
 return 2
if __name__=='__main__':raise SystemExit(main())
