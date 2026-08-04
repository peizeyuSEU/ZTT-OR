import csv,hashlib,json,sys
from pathlib import Path
R=Path(sys.argv[1]); Q=14442.842886043889
for ddir in sorted(R.glob('D[0-9][0-9]')):
 d=int(ddir.name[1:])
 for out in sorted(ddir.glob('seed_*')):
  if not out.is_dir() or 'attempt_01_incomplete' in out.name or 'attempt_02' in out.name: continue
  try: s=int(out.name.split('_')[1])
  except: continue
  cfg={'num_dc':8,'num_retailers':15,'random_seed':s,'intracity_distance_km':d,'carbon_price':2.0,'carbon_quota':Q,'delta':3000.0,'investment_exponent':0.5,'population_size':30,'max_generation':50,'crossover_rate':0.7,'mutation_rate':-1,'elitism':True,'chromosome_length':10,'early_stop':True,'convergence_generations':10,'convergence_tolerance':0.001,'pricing_algorithm':0,'pricing_max_cols_per_dc':3,'dual_smooth_alpha':0,'rc_eps':1e-6,'max_cg_iterations':2000,'max_branch_nodes':10000,'bp_time_limit_sec':600,'bp_relative_gap':0,'parallel_fitness':True,'num_threads':8,'parallel_pricing':False,'cplex_threads':1,'use_sqrt_investment':True,'use_invest_in_column':True,'transport_cost_mode':'EXPLICIT_ARC_COST','dataset_commit':'b9fc758dbe11762882bf66cbbc7672da0918adb7'}
  txt=''.join(f'{k}: {str(v).lower() if isinstance(v,bool) else v}\n' for k,v in cfg.items()); (out/'config_used.yaml').write_text(txt); (out/'resolved_config.yaml').write_text(txt)
  arc=out/'solver_arc_parameters.csv'; fp=hashlib.sha256((str(d)+str(s)+str(Q)+hashlib.sha256(arc.read_bytes()).hexdigest()).encode()).hexdigest()
  payload={'distance_km':d,'random_seed':s,'carbon_price':2.0,'carbon_quota':Q,'transport_cost_mode':'EXPLICIT_ARC_COST','dataset_commit':'b9fc758dbe11762882bf66cbbc7672da0918adb7','solver_arc_parameters_sha256':hashlib.sha256(arc.read_bytes()).hexdigest(),'instance_fingerprint':fp}
  (out/'resolved_real_case_instance.json').write_text(json.dumps(payload,indent=2)+'\n')
  rows=list(csv.DictReader((out/'result.csv').open())) if (out/'result.csv').exists() else []; row=rows[0] if rows else {}
  def num(k,default=1e99):
   try:return float(row.get(k,default))
   except:return default
  ok=row.get('final_inner_bp_status')=='OPTIMAL' and row.get('integer_solution')=='true' and abs(num('relative_gap',1))<=1e-12 and abs(num('objective_decomposition_error'))<=1e-5 and abs(num('emission_decomposition_error'))<=1e-8
  (out/'RUN_MANIFEST.txt').write_text(f'experiment_type=PART5_FORMAL_DISTANCE_SENSITIVITY\npaper_statistics=true\nvalidation_status={"PASS" if ok else "FAIL"}\ndistance_km={d}\nga_seed={s}\ncplex_seed=NOT_CONFIGURABLE\ncarbon_price=2\ncarbon_quota={Q}\ntransport_cost_mode=EXPLICIT_ARC_COST\nalgorithm_commit=bd2c35d6a\nexplicit_arc_commit=fb7f435008a60c91b9e0908fd64d3239b4197fcd\ndataset_commit=b9fc758dbe11762882bf66cbbc7672da0918adb7\ninstance_fingerprint={fp}\n')
  files=[p for p in out.iterdir() if p.is_file() and p.name!='HASH_MANIFEST.json']; (out/'HASH_MANIFEST.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)},indent=2)+'\n')
  (out/('COMPLETED.ok' if ok else 'FAILED.validation')).write_text('validation_status='+('PASS' if ok else 'FAIL')+'\n')
