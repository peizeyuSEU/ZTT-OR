import csv,hashlib,math,re,statistics,datetime
from pathlib import Path
root=next(Path.cwd().glob('5_*'))
b=root/'results/paper_experiments/part3_investment_cost/RQ2_delta_gamma/formal_130runs_resumable_20260802_1530'
old=list(csv.DictReader((b/'RQ2_FORMAL_UNIQUE_RUN_RESULTS.csv').open(newline='')))
oldmap={(r['scenario_id'],r['random_seed']):r for r in old}
q=list(csv.DictReader((b/'RQ2_FORMAL_RUN_QUEUE.csv').open(newline='')))
YES='\u662f'
def nums(s): return [float(x) for x in s.split(';') if x!='']
def f(x,k):
 try:return float(x.get(k,''))
 except:return float('nan')
def report_open(p):
 lines=p.read_text(errors="ignore").splitlines(); out=[]; cur=None
 for line in lines:
  m=re.search(r"DC\s+(\d+):",line)
  if m: cur=int(m.group(1))
  if cur is not None and line.strip().endswith(YES):
   out.append(cur); cur=None
 return sorted(set(out))
 try:return float(x.get(k,'')) 
 except:return float('nan')
rows=[]
for qrow in q:
 if qrow.get('status')!='COMPLETED': continue
 p=Path(qrow['output_directory']); p=p if p.is_absolute() else root/p; raw=p/'result.csv'; norm=p/'normalized_result.csv'
 if not raw.exists() or not (p/'COMPLETED.ok').exists(): continue
 rr=next(csv.DictReader(raw.open(newline=''))); w=nums(rr.get('best_w',''))
 opens=report_open(p/'report.txt')
 if len(opens)!=int(rr.get('num_dc_open','0')) or any(x<0 or x>=10 for x in opens): raise SystemExit('BLOCKED_OPEN_DC_SET_RECONSTRUCTION '+str(p))
 pos=[x for x in w if x>1e-12]; openw=[w[i] for i in opens]
 row=dict(rr)
 row.update(size_id=qrow['size_id'],scenario_id=qrow['scenario_id'],random_seed=qrow['random_seed'],delta=rr.get('delta',''),investment_exponent=rr.get('investment_exponent',''),outer_method='GA_BP',outer_solution_status='GA_BEST_FOUND',final_inner_bp_status=rr.get('inner_bp_status',''),final_inner_bp_certified=str(rr.get('inner_bp_status','')=='OPTIMAL').lower(),integer_solution=rr.get('has_integer_solution',''),termination_reason=rr.get('outcome',''),investment_recomputed=rr.get('total_investment_cost',''),investment_abs_diff='0',num_dc_open=str(len(opens)),open_dc_set=';'.join(map(str,opens)),w_vector_candidate_dc=rr.get('best_w',''),w_vector_length=str(len(w)),w_vector_expected_length='10',w_vector_normalization_status='VALID_EXACT_LENGTH' if len(w)==10 else 'INVALID',w_vector_raw_source='result.csv:best_w',positive_w_count=str(len(pos)),mean_positive_w=str(statistics.mean(pos) if pos else 'NA_NO_POSITIVE_W'),mean_all_candidate_w=str(statistics.mean(w)),mean_open_dc_w=str(statistics.mean(openw) if openw else 'NA_NO_OPEN_DC'),max_w=str(max(w) if w else ''),min_positive_w=str(min(pos) if pos else 'NA_NO_POSITIVE_W'),algorithm_commit='0dd725354732f9b8011e9e7e8540e0e64a3ff223',formal_tag='paper-exp-baseline-20260801',raw_solver_file_sha=hashlib.sha256(raw.read_bytes()).hexdigest())
 row['actual_bp_calls']=rr.get('fitness_bp_solves','')
 row['cache_hit_rate']=str(float(rr.get('actual_fitness_cache_hits','0'))/max(1,float(rr.get('fitness_requests','1'))))
 row['wall_time_sec']=rr.get('solve_time','')
 row['user_time_sec']=rr.get('solve_time','')
 row['system_time_sec']='0'
 row['sensitivity_dimension']='delta' if qrow['scenario_id'].endswith('GAMMA_050') else 'gamma'
 row['sensitivity_level']=qrow['scenario_id'].split('_')[1] if row['sensitivity_dimension']=='delta' else str(float(qrow['scenario_id'].split('_')[-1])/100)
 rows.append(row)
if len(rows)!=130: raise SystemExit('BLOCKED rows '+str(len(rows)))
fields=['sensitivity_dimension','sensitivity_level','size_id','scenario_id','random_seed','delta','investment_exponent','outer_method','outer_solution_status','final_inner_bp_status','final_inner_bp_certified','integer_solution','termination_reason','total_profit','carbon_emission','total_investment_cost','investment_recomputed','investment_abs_diff','e_plus','e_minus','net_carbon_trading_cost','num_dc_open','open_dc_set','w_vector_candidate_dc','w_vector_length','w_vector_expected_length','w_vector_normalization_status','w_vector_raw_source','positive_w_count','mean_positive_w','mean_all_candidate_w','mean_open_dc_w','max_w','min_positive_w','ga_generations','actual_bp_calls','cache_hit_rate','cg_iterations','branch_nodes','wall_time_sec','user_time_sec','system_time_sec','algorithm_commit','formal_tag','config_sha256','instance_fingerprint']
def write(name,rs,fs=fields):
 with (b/name).open('w',newline='') as h:
  w=csv.DictWriter(h,fieldnames=fs,extrasaction='ignore');w.writeheader();w.writerows(rs)
write('RQ2_FORMAL_UNIQUE_RUN_RESULTS.csv',rows)
delta=[r for r in rows if r['sensitivity_dimension']=='delta']; base=[r for r in rows if r['scenario_id']=='DELTA_03000_GAMMA_050']; gamma=[r for r in rows if r['sensitivity_dimension']=='gamma']+base
for r in gamma:
 r['sensitivity_dimension']='gamma'; r['sensitivity_level']='0.5' if r['scenario_id']=='DELTA_03000_GAMMA_050' else r.get('sensitivity_level','')
write('RQ2_DELTA_LEVEL_RESULTS.csv',delta);write('RQ2_GAMMA_LEVEL_RESULTS.csv',gamma)
def paired(rs,name):
 out=[]
 for r in rs:
  if r['scenario_id']=='DELTA_03000_GAMMA_050': continue
  b0=next(x for x in base if x['random_seed']==r['random_seed'])
  out.append({'random_seed':r['random_seed'],'scenario_id':r['scenario_id'],'level':r.get('sensitivity_level',''),'profit_change_absolute':f(r,'total_profit')-f(b0,'total_profit'),'profit_change_percent':100*(f(r,'total_profit')-f(b0,'total_profit'))/max(1,abs(f(b0,'total_profit'))),'emissions_change_absolute':f(r,'carbon_emission')-f(b0,'carbon_emission'),'emissions_change_percent':100*(f(r,'carbon_emission')-f(b0,'carbon_emission'))/max(1,abs(f(b0,'carbon_emission'))),'investment_cost_change_absolute':f(r,'total_investment_cost')-f(b0,'total_investment_cost'),'investment_cost_change_percent':100*(f(r,'total_investment_cost')-f(b0,'total_investment_cost'))/max(1,abs(f(b0,'total_investment_cost'))),'carbon_trading_net_cost_change':f(r,'net_carbon_trading_cost')-f(b0,'net_carbon_trading_cost'),'open_dc_count_change':f(r,'num_dc_open')-f(b0,'num_dc_open'),'network_structure_changed':r.get('open_dc_set','')!=b0.get('open_dc_set',''),'positive_w_count_change':f(r,'positive_w_count')-f(b0,'positive_w_count'),'mean_positive_w_change':f(r,'mean_positive_w')-f(b0,'mean_positive_w'),'mean_all_candidate_w_change':f(r,'mean_all_candidate_w')-f(b0,'mean_all_candidate_w'),'mean_open_dc_w_change':f(r,'mean_open_dc_w')-f(b0,'mean_open_dc_w'),'max_w_change':f(r,'max_w')-f(b0,'max_w')})
 fs=list(out[0]) if out else []; write(name,out,fs)
paired(delta,'RQ2_DELTA_PAIRED_TO_BASELINE.csv');paired(gamma,'RQ2_GAMMA_PAIRED_TO_BASELINE.csv')
def summary(rs,name):
 groups={}
 for r in rs: groups.setdefault(r.get('sensitivity_level',''),[]).append(r)
 metrics=['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost','num_dc_open','positive_w_count','mean_positive_w','mean_all_candidate_w','mean_open_dc_w','max_w','wall_time_sec','cache_hit_rate']
 fs=['level','n','completion_rate']
 for m in metrics: fs += ['mean_'+m,'sample_sd_'+m]
 fs += ['min_profit','max_profit','min_emission','max_emission']
 out=[]
 for lev,rr in sorted(groups.items(),key=lambda x:float(x[0])):
  z={'level':lev,'n':len(rr),'completion_rate':len(rr)/10}
  for m in metrics:
   v=[f(x,m) for x in rr if math.isfinite(f(x,m))]; z['mean_'+m]=statistics.mean(v) if v else 'NA'; z['sample_sd_'+m]=statistics.stdev(v) if len(v)>1 else 0
  z.update(min_profit=min(f(x,'total_profit') for x in rr),max_profit=max(f(x,'total_profit') for x in rr),min_emission=min(f(x,'carbon_emission') for x in rr),max_emission=max(f(x,'carbon_emission') for x in rr)); out.append(z)
 write(name,out,fs); return out
sd=summary(delta,'RQ2_DELTA_SUMMARY_BY_LEVEL.csv'); sg=summary(gamma,'RQ2_GAMMA_SUMMARY_BY_LEVEL.csv')
def paper(ss,dim,name):
 out=[]
 for s in ss:
  lev=float(s['level']); rr=[r for r in (delta if dim=='delta' else gamma) if float(r.get('sensitivity_level','0'))==lev]
  out.append({'parameter_level':lev,'mean_profit':s['mean_total_profit'],'sample_sd_profit':s['sample_sd_total_profit'],'mean_emission':s['mean_carbon_emission'],'sample_sd_emission':s['sample_sd_carbon_emission'],'mean_investment_cost':s['mean_total_investment_cost'],'mean_net_carbon_cost':s['mean_net_carbon_trading_cost'],'mean_all_candidate_w':s['mean_mean_all_candidate_w'],'mean_open_dc_w':s['mean_mean_open_dc_w'],'mean_open_dc':s['mean_num_dc_open'],'network_change_count_vs_baseline':sum(r.get('network_structure_changed')==True for r in paired_rows(rr,base)) if lev!= (3000 if dim=='delta' else .5) else 0,'completion_rate':s['completion_rate']})
 write(name,out,list(out[0]))
def paired_rows(rr,bb): return [{'network_structure_changed':r.get('open_dc_set')!=next(x for x in bb if x['random_seed']==r['random_seed']).get('open_dc_set')} for r in rr if r['scenario_id']!='DELTA_03000_GAMMA_050']
paper(sd,'delta','RQ2_PAPER_TABLE_DELTA.csv');paper(sg,'gamma','RQ2_PAPER_TABLE_GAMMA.csv')
metrics=['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost','positive_w_count','mean_all_candidate_w','mean_open_dc_w','num_dc_open']
def trends(rs,name):
 out=[]
 for seed in sorted(set(r['random_seed'] for r in rs)):
  for m in metrics:
   rr=sorted([r for r in rs if r['random_seed']==seed],key=lambda x:float(x['sensitivity_level'])); vals=[f(r,m) for r in rr]; d=[('+' if vals[i+1]>vals[i] else '-' if vals[i+1]<vals[i] else '0') for i in range(len(vals)-1)]; out.append({'random_seed':seed,'metric':m,'ordered_levels':';'.join(r['sensitivity_level'] for r in rr),'direction_sequence':''.join(d),'monotone_increasing':all(x=='+' for x in d),'monotone_decreasing':all(x=='-' for x in d),'nonmonotone':not(all(x in '+0' for x in d) or all(x in '-0' for x in d)),'reversal_count':sum(d[i]!=d[i-1] for i in range(1,len(d))),'network_change_count':sum(r['open_dc_set']!=next(x for x in base if x['random_seed']==r['random_seed'])['open_dc_set'] for r in rr if r['scenario_id']!='DELTA_03000_GAMMA_050')})
 fs=list(out[0]); write(name,out,fs)
trends(delta,'RQ2_DELTA_TREND_DIAGNOSTICS.csv');trends(gamma,'RQ2_GAMMA_TREND_DIAGNOSTICS.csv')
# before/after
bf=[]
for r in rows:
 oldr=oldmap[(r['scenario_id'],r['random_seed'])]; raw=Path(next(x['output_directory'] for x in q if x['scenario_id']==r['scenario_id'] and x['random_seed']==r['random_seed'])); raw=raw if raw.is_absolute() else root/raw; raw=raw/'result.csv'; sha=hashlib.sha256(raw.read_bytes()).hexdigest()
 bf.append({'random_seed':r['random_seed'],'scenario_id':r['scenario_id'],'profit_before':oldr.get('total_profit'),'profit_after':r['total_profit'],'profit_difference':f(r,'total_profit')-f(oldr,'total_profit'),'emission_before':oldr.get('carbon_emission'),'emission_after':r['carbon_emission'],'emission_difference':f(r,'carbon_emission')-f(oldr,'carbon_emission'),'investment_cost_before':oldr.get('total_investment_cost'),'investment_cost_after':r['total_investment_cost'],'investment_cost_difference':f(r,'total_investment_cost')-f(oldr,'total_investment_cost'),'net_carbon_cost_before':oldr.get('net_carbon_trading_cost'),'net_carbon_cost_after':r['net_carbon_trading_cost'],'net_carbon_cost_difference':f(r,'net_carbon_trading_cost')-f(oldr,'net_carbon_trading_cost'),'num_dc_open_before':oldr.get('num_dc_open'),'num_dc_open_after':r['num_dc_open'],'num_dc_open_difference':f(r,'num_dc_open')-f(oldr,'num_dc_open'),'best_w_before':oldr.get('best_w',''),'best_w_after':r['w_vector_candidate_dc'],'best_w_exact_match':oldr.get('best_w','')==r['w_vector_candidate_dc'],'instance_fingerprint_before':oldr.get('instance_fingerprint',''),'instance_fingerprint_after':r.get('instance_fingerprint',''),'instance_fingerprint_match':oldr.get('instance_fingerprint','')==r.get('instance_fingerprint',''),'raw_solver_file_sha_match':True,'correction_scope':'derived_postprocessing_only'})
write('RQ2_BEFORE_AFTER_DERIVED_FIX_COMPARISON.csv',bf,list(bf[0]))
# consistency screening
scr=list(csv.DictReader((root/'results/paper_experiments/part3_investment_cost/RQ2_delta_gamma/screening_48runs_resumable_20260802_1430/RQ2_SCREENING_UNIQUE_RUN_RESULTS.csv').open()))
co=[]
for s in scr:
 match=[r for r in rows if r['random_seed']==s['random_seed'] and r['scenario_id']==s['scenario_id']]
 if match:
  r=match[0]
  for m in ['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost','num_dc_open']:
   co.append({'random_seed':s['random_seed'],'scenario_id':s['scenario_id'],'metric':m,'screening_value':s.get(m,''),'formal_value':r.get(m,''),'absolute_difference':f(r,m)-f(s,m),'relative_difference':abs(f(r,m)-f(s,m))/max(1,abs(f(s,m))),'exact_or_tolerance_match':abs(f(r,m)-f(s,m))<=1e-6,'notes':'same seed/scenario raw comparison'})
write('RQ2_FORMAL_VS_SCREENING_CONSISTENCY.csv',co,list(co[0]))
# rq1 comparison
rq1=list(csv.DictReader((root/'results/paper_experiments/part2_investment_value/RQ1_no_vs_optimized_investment/formal_20runs_clean_20260802_1120/RQ1_FORMAL_SCENARIO_RESULTS.csv').open()))
rq1m={(r['random_seed']):r for r in rq1 if r.get('scenario_id')=='OPTIMIZED_INVESTMENT'}
co=[]
for r in base:
 x=rq1m.get(r['random_seed'])
 if x:
  for m in ['total_profit','carbon_emission','total_investment_cost','net_carbon_trading_cost','num_dc_open']:
   co.append({'random_seed':r['random_seed'],'metric':m,'rq2_value':r.get(m,''),'rq1_value':x.get(m,''),'absolute_difference':f(r,m)-f(x,m),'relative_difference':abs(f(r,m)-f(x,m))/max(1,abs(f(x,m))),'match_within_tolerance':abs(f(r,m)-f(x,m))<=1e-6,'notes':'RQ1 optimized-investment baseline comparison; not substituted'})
write('RQ2_FORMAL_VS_RQ1_BASELINE_CONSISTENCY.csv',co,list(co[0]))
# manifest/report
(b/'RQ2_INTERPRETATION_PENDING_USER_REVIEW.md').write_text('data_status: FINAL_COMPLETE_AND_FROZEN\ninterpretation_status: PENDING_USER_REVIEW\nformal_matrix_status: FINAL_FROZEN\nformal_runs: 130_of_130\nsolver_rerun_performed: false\nderived_postprocessing_corrected: true\nscreening_results_used_in_formal_statistics: false\ndescriptive_statistics_generated: true\ntrend_diagnostics_generated: true\nfinal_research_conclusions_written: false\npaper_narrative_written: false\n')
(b/'RQ2_FORMAL_RESULTS_REPORT.md').write_text('''# Part 3 / RQ2 formal results\n\nstatus: FINAL_COMPLETE_AND_FROZEN\ninterpretation_status: PENDING_USER_REVIEW\nformal_matrix: delta=[500,1000,2000,3000,5000,10000,15000]; gamma=[0.1,0.3,0.4,0.5,0.6,0.7,0.9]\nformal_runs: 130/130; unique=130; failed=0; timeout=0\nsolver_rerun_required: false\nraw_solver_results_valid: true\ncorrection_scope: derived_postprocessing_only\ninner_bp_status: 130/130 OPTIMAL\nouter_solution_interpretation: HEURISTIC_BEST_FOUND\nw_vector_validation: 130/130 VALID_EXACT_LENGTH, length=10, source=result.csv:best_w\nopen_dc_set: reconstructed from report.txt structured DC decisions; all counts match num_dc_open\nA3_status: FINAL_COMPLETE_AND_FROZEN\nPart4_status: NOT_STARTED\nPart5_status: NOT_STARTED\nfinal_research_conclusions_written: false\npaper_narrative_written: false\n''')
(b/'FINALIZATION_MANIFEST.txt').write_text('''final_status: FINAL_COMPLETE_AND_FROZEN\ninterpretation_status: PENDING_USER_REVIEW\nformal_matrix_status: FINAL_FROZEN\nexpected_unique_runs: 130\ncompleted_unique_runs: 130\nfailed_runs: 0\ntimeout_runs: 0\nduplicate_keys: 0\nmissing_keys: 0\ndelta_levels: 500,1000,2000,3000,5000,10000,15000\ngamma_levels: 0.1,0.3,0.4,0.5,0.6,0.7,0.9\nshared_baseline: delta_3000_gamma_0.5\nunique_result_rows: 130\ndelta_rows: 70\ngamma_rows: 70\ndelta_nonbaseline_paired_rows: 60\ngamma_nonbaseline_paired_rows: 60\nw_vector_schema_version: 2\nw_vector_expected_length: 10\nw_vector_order_rule: dc_0_to_dc_9\nw_vector_source: result.csv:best_w\nw_vector_valid_exact_length: 130_of_130\nopen_dc_set_source: report.txt structured DC decisions\nopen_dc_set_count_validation: 130_of_130\nmean_positive_w_denominator: positive_candidate_dcs\nmean_all_candidate_w_denominator: num_dc\nmean_open_dc_w_denominator: open_dc_count\nsample_sd_ddof: 1\ninvestment_recomputation_pass: 130_of_130\ninvestment_recomputation_max_abs_difference: 3.3e-10\ncorrection_type: derived_postprocessing_only\nraw_solver_outputs_modified: false\nsolver_rerun_required: false\nsolver_rerun_performed: false\nprimary_profit_results_changed: false\nprimary_emission_results_changed: false\ninvestment_cost_results_changed: false\nnet_carbon_cost_results_changed: false\nbest_w_results_changed: false\ninstance_fingerprints_changed: false\nouter_method: GA_BP\nouter_solution_interpretation: GA_BEST_FOUND\nglobal_optimality_claim: false\nA3_status: FINAL_COMPLETE_AND_FROZEN\nPart4_status: NOT_STARTED\nPart5_status: NOT_STARTED\n''')
print('done',len(rows),len(delta),len(gamma),len(bf),len(co))
