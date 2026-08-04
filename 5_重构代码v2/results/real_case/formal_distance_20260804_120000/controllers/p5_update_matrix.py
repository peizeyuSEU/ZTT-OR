import csv,sys
from pathlib import Path
R=Path(sys.argv[1]); p=R/'P5_FORMAL_RUN_MATRIX.csv'; rows=list(csv.DictReader(p.open()))
for r in rows:
 d=int(r['intracity_distance_km']);s=int(r['ga_seed']); o=R/f'D{d:02d}'/(f'seed_{s}_attempt_03' if d==10 and s==101 else f'seed_{s}')
 r['status']='COMPLETED' if (o/'COMPLETED.ok').exists() else 'FAILED'; r['expected_output_dir']=str(o)
with p.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
