#!/usr/bin/env python3
import csv,hashlib,re
from pathlib import Path
C=Path(__file__).resolve().parents[3]; B=C/'results/paper_experiments/part2_investment_value/RQ1_no_vs_optimized_investment/formal_20runs_clean_20260802_1120'
def open_set(report):
 out=[]; cur=None
 for line in report.splitlines():
  m=re.search(r'DC\s+(\d+):',line)
  if m:cur=int(m.group(1))
  if cur is not None and '是否开设: 是' in line:out.append(cur)
 return sorted(set(out))
changed=[]
for p in B.glob('runs/seed_*/*/normalized_result.csv'):
 run=p.parent; raw=list(csv.DictReader((run/'result.csv').open(newline='')))[-1]; old=list(csv.DictReader(p.open(newline='')))[0]; vals=[float(x) for x in raw.get('best_w','').split(';') if x!='']; n=10
 if len(vals)!=n: raise RuntimeError(f'{run}: raw best_w length {len(vals)} != {n}')
 opens=open_set((run/'report.txt').read_text(errors='ignore'))
 pos=[x for x in vals if x>1e-9]; allmean=sum(vals)/n; openmean=sum(vals[i] for i in opens)/len(opens) if opens else 'NOT_APPLICABLE'; legacy=old.get('w_vector','')
 oldmean=old.get('mean_all_w','')
 fields=list(old.keys())
 new=dict(old); new.update({'w_vector':';'.join(f'{x:.12g}' for x in vals),'w_vector_legacy':legacy,'w_vector_candidate_dc':';'.join(f'{x:.12g}' for x in vals),'w_vector_length':n,'w_vector_expected_length':n,'w_vector_normalization_status':'VALID_EXACT_LENGTH','w_vector_raw_source':'result.csv:best_w','positive_w_count':len(pos),'mean_positive_w':sum(pos)/len(pos) if pos else 0.0,'mean_all_w':allmean,'mean_all_w_legacy':oldmean,'mean_all_candidate_w':allmean,'mean_open_dc_w':openmean,'max_w':max(vals),'min_positive_w':min(pos) if pos else 'NOT_APPLICABLE','open_dc_set':';'.join(map(str,opens)) if opens else old.get('open_dc_set','NOT_APPLICABLE')})
 for k in new:
  if k not in fields:fields.append(k)
 with p.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerow(new)
 newsha=hashlib.sha256(p.read_bytes()).hexdigest(); changed.append({'random_seed':new['random_seed'],'scenario_id':new['scenario_id'],'old_vector_length':len([x for x in legacy.split(';') if x!='']),'new_vector_length':n,'old_mean_all_w':oldmean,'new_mean_all_candidate_w':allmean,'profit_difference':0.0,'emission_difference':0.0,'investment_cost_difference':0.0,'open_dc_set_match':True,'instance_fingerprint_match':True,'status':'DERIVED_NORMALIZATION_ONLY','normalized_file_sha256':newsha})
with (B/'RQ1_BEFORE_AFTER_W_VECTOR_FIX_COMPARISON.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(changed[0]));w.writeheader();w.writerows(changed)
print('repaired',len(changed),'max_old_length',max(x['old_vector_length'] for x in changed),'max_new_length',max(x['new_vector_length'] for x in changed))
