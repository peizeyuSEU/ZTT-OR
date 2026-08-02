import csv,math
from pathlib import Path
def main():
 root=next(Path.cwd().glob('5_*')); b=root/'results/paper_experiments/part3_investment_cost/RQ2_delta_gamma/formal_130runs_resumable_20260802_1530'
 rows=list(csv.DictReader((b/'RQ2_FORMAL_UNIQUE_RUN_RESULTS.csv').open()))
 assert len(rows)==130 and len({(r['scenario_id'],r['random_seed']) for r in rows})==130
 assert len(list(csv.DictReader((b/'RQ2_DELTA_LEVEL_RESULTS.csv').open())))==70
 assert len(list(csv.DictReader((b/'RQ2_GAMMA_LEVEL_RESULTS.csv').open())))==70
 assert len(list(csv.DictReader((b/'RQ2_DELTA_PAIRED_TO_BASELINE.csv').open())))==60
 assert len(list(csv.DictReader((b/'RQ2_GAMMA_PAIRED_TO_BASELINE.csv').open())))==60
 assert sorted(float(r['level']) for r in csv.DictReader((b/'RQ2_GAMMA_SUMMARY_BY_LEVEL.csv').open()))==[.1,.3,.4,.5,.6,.7,.9]
 assert all(r.get('w_vector_length')=='10' and r.get('w_vector_normalization_status')=='VALID_EXACT_LENGTH' for r in rows)
 assert all(r.get('open_dc_set','')!='' for r in rows)
 assert all(r.get('final_inner_bp_status')=='OPTIMAL' for r in rows)
 print('PASS RQ2 derived validation')
if __name__=='__main__': main()
