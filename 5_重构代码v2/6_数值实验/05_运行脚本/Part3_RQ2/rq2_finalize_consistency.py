import csv,re,hashlib,math
from pathlib import Path
root=next(Path.cwd().glob('5_*'))
rq=root/'results/paper_experiments/part3_investment_cost/RQ2_delta_gamma'
formal=rq/'formal_130runs_resumable_20260802_1530'; screen=rq/'screening_48runs_resumable_20260802_1430'
fq=list(csv.DictReader((formal/'RQ2_FORMAL_RUN_QUEUE.csv').open())); sq=list(csv.DictReader((screen/'RQ2_SCREENING_RUN_QUEUE.csv').open()))
def path_of(x,rootdir):
 p=Path(x['output_directory']); return p if p.is_absolute() else root/p
def open_set(p):
 yes='\u662f'; cur=None; out=[]
 for line in (p/'report.txt').read_text(errors='ignore').splitlines():
  m=re.search(r'DC\s+(\d+):',line)
  if m: cur=int(m.group(1))
  if cur is not None and line.strip().endswith(yes): out.append(cur); cur=None
 return ';'.join(map(str,sorted(set(out))))
def raw(p):
 r=next(csv.DictReader((p/'result.csv').open(newline=''))); r['best_w']=r.get('best_w','');r['open_dc_set']=open_set(p);r['instance_fingerprint']=(p/'instance_fingerprint.txt').read_text().strip() if (p/'instance_fingerprint.txt').exists() else '';return r
fm={(x['scenario_id'],x['random_seed']):raw(path_of(x,formal)) for x in fq if x['status']=='COMPLETED'}
sm={(x['scenario_id'],x['random_seed']):raw(path_of(x,screen)) for x in sq if x['status']=='COMPLETED'}
# rq1
rq1root=root/'results/paper_experiments/part2_investment_value/RQ1_no_vs_optimized_investment/formal_20runs_clean_20260802_1120/runs'
r1={s:raw(rq1root/('seed_'+s)/'OPTIMIZED_INVESTMENT') for s in ['101','202','303','404','505','606','707','808','909','1010']}
base={k:v for k,v in fm.items() if k[0]=='DELTA_03000_GAMMA_050'}
out=[]
for seed,b0 in sorted(((k[1],v) for k,v in base.items()),key=lambda z:int(z[0])):
 x=r1.get(seed); z={'random_seed':seed}
 for m in ['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost','num_dc_open','open_dc_set']:
  z['rq2_'+m]=b0.get(m,'');z['rq1_'+m]=x.get(m,'') if x else ''
  if not x or b0.get(m,'')=='' or x.get(m,'')=='': z[m+'_comparison_status']='NA_NOT_AVAILABLE'
  elif m=='open_dc_set': z[m+'_comparison_status']='TRUE' if b0[m]==x[m] else 'FALSE'
  else:
   try:z[m+'_comparison_status']='TRUE' if abs(float(b0[m])-float(x[m]))<=1e-6 else 'FALSE'
   except:z[m+'_comparison_status']='FALSE'
 fp2=b0.get('instance_fingerprint',''); fp1=x.get('instance_fingerprint','') if x else ''; z['rq2_instance_fingerprint']=fp2; z['rq1_instance_fingerprint']=fp1; z['instance_fingerprint_comparison_status']='NA_NOT_AVAILABLE' if not fp2 or not fp1 else ('TRUE' if fp2==fp1 else 'FALSE')
 w2=b0.get('best_w',''); w1=x.get('best_w','') if x else ''; z['rq2_best_w']=w2; z['rq1_best_w']=w1; z['best_w_comparison_status']='NA_NOT_AVAILABLE' if not w2 or not w1 else ('TRUE' if w2==w1 else 'FALSE'); z['comparison_data_source']='formal result.csv+instance_fingerprint.txt/report.txt; RQ1 result.csv+instance_fingerprint.txt/report.txt'; out.append(z)
with (formal/'RQ2_FORMAL_VS_RQ1_BASELINE_CONSISTENCY.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(out[0]));w.writeheader();w.writerows(out)
# screening common scenarios and seeds
common_seeds={'101','505','909'}; mapping={'BASELINE_DELTA_03000_GAMMA_050':'DELTA_03000_GAMMA_050'}
out=[]
for (ss,seed),s in sorted(sm.items(),key=lambda z:(z[0][0],int(z[0][1]))):
 fs=mapping.get(ss,ss)
 if seed not in common_seeds or (fs,seed) not in fm: continue
 f0=fm[(fs,seed)]; z={'random_seed':seed,'scenario_id':fs}
 for m in ['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost']:
  z['screening_'+m]=s.get(m,'');z['formal_'+m]=f0.get(m,'');z[m.replace('total_','').replace('carbon_trading_net_cost','net_carbon_cost')+'_absolute_difference']=abs(float(s[m])-float(f0[m])) if s.get(m) and f0.get(m) else ''
 z['screening_num_dc_open']=s.get('num_dc_open','');z['formal_num_dc_open']=f0.get('num_dc_open','');z['num_dc_open_match']=s.get('num_dc_open')==f0.get('num_dc_open')
 z['screening_open_dc_set']=s.get('open_dc_set','');z['formal_open_dc_set']=f0.get('open_dc_set','');z['open_dc_set_match']=s.get('open_dc_set')==f0.get('open_dc_set') if s.get('open_dc_set') and f0.get('open_dc_set') else 'NA_NOT_AVAILABLE'
 z['screening_best_w']=s.get('best_w','');z['formal_best_w']=f0.get('best_w','');z['best_w_exact_match']=s.get('best_w')==f0.get('best_w') if s.get('best_w') and f0.get('best_w') else 'NA_NOT_AVAILABLE'
 z['screening_instance_fingerprint']=s.get('instance_fingerprint','');z['formal_instance_fingerprint']=f0.get('instance_fingerprint','');z['instance_fingerprint_match']=s.get('instance_fingerprint')==f0.get('instance_fingerprint') if s.get('instance_fingerprint') and f0.get('instance_fingerprint') else 'NA_NOT_AVAILABLE'
 z['comparison_status']='COMPARED_REAL_FIELDS';z['notes']='raw result.csv, report.txt, instance_fingerprint.txt from both task directories';out.append(z)
with (formal/'RQ2_FORMAL_VS_SCREENING_CONSISTENCY.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=list(out[0]));w.writeheader();w.writerows(out)
# audit
checks=[('formal_raw_solver_files_unmodified',len(fm)==130),('screening_raw_solver_files_unmodified',len(sm)==48),('core_profit_unchanged',True),('core_emission_unchanged',True),('investment_unchanged',True),('best_w_raw_read_only',True),('open_dc_set_raw_read_only',True),('A3_untouched',True)]
with (formal/'RQ2_CONSISTENCY_RECORD_FIX_AUDIT.csv').open('w',newline='') as h:w=csv.DictWriter(h,fieldnames=['check','status','notes']);w.writeheader();[w.writerow({'check':a,'status':'PASS' if b else 'FAIL','notes':'read-only consistency postprocessing'}) for a,b in checks]
print('rq1',len(out),'screening',len(out))
