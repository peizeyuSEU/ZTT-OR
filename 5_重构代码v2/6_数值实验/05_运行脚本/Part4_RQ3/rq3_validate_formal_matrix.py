from pathlib import Path
import yaml,sys
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith('5_') and p.name!='5_*');d=next((x for x in root.glob('6_*/01_*/Part4_RQ3') if '????' not in str(x)));files=sorted(d.glob('FORMAL_EXPERIMENT_MATRIX_PART4_RQ3_*.yaml'));p=files[-1];x=yaml.safe_load(p.read_text());e=[]
if x['metadata']['status']!='FINAL_FROZEN_PENDING_EXECUTION_APPROVAL':e.append('status')
if x['model_parameters']['carbon_price_levels']!=[0,1,2,4,6,10,20]:e.append('prices')
if len(x['seeds'])!=10 or x['solver_runs']['carbon_price_formal_runs']!=70 or x['solver_runs']['allowance_additional_solver_runs']!=0 or x['analytical_observations']['derived_allowance_observations']!=50:e.append('counts')
if x['metadata']['formal_experiment_start_allowed'] is not False:e.append('start flag')
if e:print('VALIDATION_FAILED',e);sys.exit(1)
print('VALIDATION_PASS: formal prices=7; seeds=10; solver runs=70; allowance solver runs=0; derived observations=50')
