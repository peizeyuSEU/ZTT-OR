from pathlib import Path
import csv,sys
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith('5_') and p.name!='5_*');b=next((root/'results/paper_experiments/part4_carbon_policy/RQ3_price_cap').glob('screening_42runs_resumable_*'))
q=list(csv.DictReader((b/'RQ3_SCREENING_RUN_QUEUE.csv').open()));a=list(csv.DictReader((b/'RQ3_CAP_IDENTITY_AUDIT.csv').open()))
e=[]
if len(q)!=42 or sum(x['status']=='COMPLETED' for x in q)!=42:e.append('screening tasks')
if len(a)!=15:e.append('cap audit rows')
if max(abs(float(x['profit_identity_difference'])) for x in a)!=0:e.append('identity error')
for x in a:
 for k in ['profit_identity_match','carbon_emission_match','investment_cost_match','best_w_match','open_dc_set_match','num_dc_open_match','retailer_service_set_match','pricing_decisions_match','assignment_signature_match']:
  if x[k] not in ('TRUE','FALSE','NA_NOT_AVAILABLE'):e.append('invalid status')
if e:print('VALIDATION_FAILED',e);sys.exit(1)
print('VALIDATION_PASS: 42 screening; 15 cap rows; identity max error 0; status vocabulary valid')
