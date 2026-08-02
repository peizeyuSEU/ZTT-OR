#!/usr/bin/env python3
"""Conservative w-vector normalization helpers. Never pads without evidence."""
import math,re

def parse_values(raw):
 if raw is None:return []
 if isinstance(raw,(list,tuple)):return [float(x) for x in raw]
 s=str(raw).strip()
 if not s:return []
 return [float(x.strip()) for x in re.split(r'[;, ]+',s) if x.strip()]
def normalize(raw,num_dc,min_w=0.0,max_w=1.0,method='raw'):
 vals=parse_values(raw); n=int(num_dc); tol=1e-9
 finite=all(math.isfinite(x) for x in vals); in_range=finite and all(min_w-tol<=x<=max_w+tol for x in vals)
 if len(vals)==n:
  status='VALID_EXACT_LENGTH'; out=vals
 elif len(vals)<n and all(abs(x)<=tol for x in vals[-0:] if False):
  status='UNRECONSTRUCTABLE'; out=vals
 else:
  status='OTHER_LENGTH_MISMATCH' if in_range else 'UNRECONSTRUCTABLE'; out=vals
 return {'values':out,'raw_length':len(vals),'expected_length':n,'status':status,'finite':finite,'in_range':in_range,'method':method}
def metrics(vals,open_set=None):
 pos=[x for x in vals if x>1e-9]; d={'positive_w_count':len(pos),'mean_positive_w':sum(pos)/len(pos) if pos else 0.0,'mean_all_candidate_w':sum(vals)/len(vals) if vals else 'UNRECONSTRUCTABLE','max_w':max(vals) if vals else 'UNRECONSTRUCTABLE','min_positive_w':min(pos) if pos else 'NOT_APPLICABLE'}
 if open_set is None or not open_set:d['mean_open_dc_w']='NOT_APPLICABLE'
 else:d['mean_open_dc_w']=sum(vals[i] for i in open_set if 0<=i<len(vals))/len(open_set)
 return d
if __name__=='__main__':
 import argparse,json
 p=argparse.ArgumentParser();p.add_argument('vector');p.add_argument('num_dc',type=int);a=p.parse_args();r=normalize(a.vector,a.num_dc);r.update(metrics(r['values']));print(json.dumps(r))
