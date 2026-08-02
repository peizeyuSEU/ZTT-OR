#!/usr/bin/env python3
import csv,math,sys
from pathlib import Path

def validate_file(p):
 rows=list(csv.DictReader(Path(p).open(newline=''))); errs=[]
 for i,r in enumerate(rows,1):
  try:n=int(r.get('expected_vector_length') or r.get('w_vector_expected_length') or r.get('num_dc')); vals=[float(x) for x in (r.get('w_vector_candidate_dc') or r.get('w_vector','')).split(';') if x!='']
  except Exception as e: errs.append((i,str(e)));continue
  if len(vals)!=n:errs.append((i,f'length {len(vals)} != {n}'))
  if not all(math.isfinite(x) and -1e-9<=x<=1+1e-9 for x in vals):errs.append((i,'nonfinite or out of range'))
 return errs
if __name__=='__main__':
 e=validate_file(sys.argv[1]); print('PASS' if not e else '\n'.join(map(str,e))); raise SystemExit(bool(e))
