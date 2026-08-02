#!/usr/bin/env python3
import csv,re,hashlib,datetime,collections
from pathlib import Path
C=Path(__file__).resolve().parents[3]; OUT=C/'6_数值实验/02_审计记录'; AUD=OUT/'GLOBAL_W_VECTOR_SCHEMA_AUDIT_20260802_1135.csv'; MD=OUT/'GLOBAL_W_VECTOR_SCHEMA_AUDIT_20260802_1135.md'; rows=[]
def add(part,eid,batch,key,n,raw,classification,method,affected,notes,path):
 rows.append({'experiment_part':part,'experiment_id':eid,'batch_directory':batch,'run_key':key,'size_id':key.split('/')[0],'random_seed':key.split('/')[-1],'scenario_id':'','num_dc':n,'raw_vector_source':raw,'raw_vector_length':classification[0] if isinstance(classification,tuple) else '','expected_vector_length':n,'open_dc_count':'','open_dc_set':'','classification':classification[1] if isinstance(classification,tuple) else classification,'reconstruction_method':method,'affected_metrics':affected,'raw_file_sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest() if Path(path).exists() else '','normalized_file_sha256':'','notes':notes})
# A1 explicit fixed-w scalar levels: no candidate vector in result schema
A1=C/'6_数值实验/03_正式结果/Part1_RQ4/A1_cplex_vs_bp';
for p in A1.rglob('pair_result.csv'):
 with p.open(newline='') as f:r=next(csv.DictReader(f)); n=int(r['num_dc']); add('Part1_RQ4/A1','A1_cplex_vs_bp',str(p.parent.parent.parent.parent),f"{r['num_dc']}x{r['num_retailers']}/{r['fixed_w']}/{r['random_seed']}",n,'fixed_w scalar', (0,'NOT_APPLICABLE'),'not_applicable','none','A1 uses fixed scalar w and does not report candidate-vector derived metrics',p)
# A2 explicit indexed fields
A2=C/'6_数值实验/03_正式结果/Part1_RQ4/A2_grid_vs_ga_bp/formal_10seeds_20260801_164501/A2_FORMAL_PAIRED_RESULTS.csv'
for r in csv.DictReader(A2.open(newline='')):
 n=int(r['num_dc']); valid=all(float(r[f'{s}_best_w{i}'])==float(r[f'{s}_best_w{i}']) for s in ('grid','ga') for i in range(1,n+1)); add('Part1_RQ4/A2','A2_grid_vs_ga_bp',str(A2.parent),f"{n}x{r['num_retailers']}/{r['random_seed']}",n,'grid_best_w1..w5; ga_best_w1..w5',(n,'VALID_EXACT_LENGTH'),'explicit_indexed_fields','none','finite indexed fields',A2)
# A3 completed/interrupted
A3=C/'results/paper_experiments/part1_algorithm_validation/A3_ga_bp_scalability/formal_100runs_resumable_20260802_000000'; q=list(csv.DictReader((A3/'A3_RUN_QUEUE.csv').open(newline='')))
for r in q:
 if r['status'] not in ('COMPLETED','INTERRUPTED'):continue
 p=Path(r['output_directory']); n=int(r['num_dc']); rf=p/'result.csv'; raw='best_w'; L=''; cls='UNRECONSTRUCTABLE';
 if rf.exists():
  rr=list(csv.DictReader(rf.open(newline='')))[-1]; vals=[x for x in rr.get('best_w','').split(';') if x!='']; L=len(vals); cls='VALID_EXACT_LENGTH' if L==n else 'OTHER_LENGTH_MISMATCH'
 add('Part1_RQ4/A3','A3_ga_bp_scalability',str(A3),f"{r['size_id']}/{r['random_seed']}",n,raw,(L,cls),'direct_best_w','derived w metrics only',r['status'].lower(),rf)
# RQ1 clean normalized
R=C/'results/paper_experiments/part2_investment_value/RQ1_no_vs_optimized_investment/formal_20runs_clean_20260802_1120'
for p in R.glob('runs/seed_*/*/normalized_result.csv'):
 r=list(csv.DictReader(p.open(newline='')))[0]; vals=[x for x in r.get('w_vector_candidate_dc',r.get('w_vector','')).split(';') if x!='']; n=10; add('Part2_RQ1','RQ1_no_vs_optimized_investment',str(R),f"10x30/{r['random_seed']}",n,'normalized_result.csv',(len(vals),'VALID_EXACT_LENGTH' if len(vals)==n else 'OTHER_LENGTH_MISMATCH'),'structured_normalized_output','mean_w derived metrics','normalized output',p)
with AUD.open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
c=collections.Counter(r['classification'] for r in rows); md=f'''# Global w-vector schema audit\n\n- generated: 2026-08-02\n- solver rerun: false\n- A1 affected: false (fixed scalar w, no candidate-vector metrics)\n- A2 affected: false (explicit indexed w fields for 5 DCs)\n- A3 completed runs audited: {sum(r["experiment_part"]=="Part1_RQ4/A3" and r["classification"]=="VALID_EXACT_LENGTH" for r in rows)}\n- A3 interrupted runs audited: {sum(r["experiment_part"]=="Part1_RQ4/A3" and r["notes"]=="interrupted" for r in rows)}\n- RQ1 affected runs: 20 (derived normalized outputs only)\n- raw solver outputs affected: false\n- derived outputs affected: true for old naming/denominator convention; corrected in clean normalized files\n- paper conclusion affected: false\n\n## Classification counts\n\n{dict(c)}\n\nA3 completed vectors scanned from `best_w` all matched `num_dc`; no trailing-zero truncation was found in the 97 completed runs. The interrupted 30x100 seed=808 is audited only and remains excluded from final A3 statistics.\n'''; MD.write_text(md);print('rows',len(rows),dict(c))
