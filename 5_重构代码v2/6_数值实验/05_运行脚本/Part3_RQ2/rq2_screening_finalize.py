#!/usr/bin/env python3
import csv,json,math,statistics,sys,datetime
from pathlib import Path

def read_rows(root):
 out=[]
 for p in sorted(root.glob('seed_*/**/normalized_result.csv')):
  marker=p.parent/'COMPLETED.ok'
  if not marker.exists(): continue
  r=next(csv.DictReader(p.open(newline=''))); out.append(r)
 return out

def num(r,k):
 try:return float(r.get(k,''))
 except:return float('nan')

def main(root):
 root=Path(root); rows=read_rows(root)
 if len(rows)!=48: print('BLOCKED: valid completed rows',len(rows)); return 1
 keys=['size_id','scenario_id','screening_dimension','screening_level','random_seed','delta','investment_exponent','outer_status','inner_bp_status','has_integer_solution','outcome','total_profit','carbon_emission','total_investment_cost','e_plus','e_minus','net_carbon_trading_cost','num_dc_open','ga_generations','fitness_requests','fitness_bp_solves','actual_fitness_cache_hits','cache_hit_rate','cg_iterations','branch_nodes','best_w','w_vector_expected_length','w_vector_normalization_status','algorithm_commit','formal_tag','config_sha256','instance_fingerprint','demand_source','investment_recomputed','investment_abs_diff','user_time_sec','system_time_sec','wall_time_sec','max_rss_kb']
 with (root/'RQ2_SCREENING_UNIQUE_RUN_RESULTS.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
 for dim,name in [('delta','RQ2_SCREENING_DELTA_RESULTS.csv'),('gamma','RQ2_SCREENING_GAMMA_RESULTS.csv')]:
  rr=[r for r in rows if r['screening_dimension']==dim]
  if dim=='gamma': rr += [r for r in rows if r['scenario_id']=='BASELINE_DELTA_03000_GAMMA_050']
  with (root/name).open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rr)
 groups={}
 for r in rows: groups.setdefault((r['screening_dimension'],r['screening_level']),[]).append(r)
 groups.setdefault(('gamma','0.5'),[]).extend([r for r in rows if r['scenario_id']=='BASELINE_DELTA_03000_GAMMA_050'])
 sk=['screening_dimension','level','n','completion_rate','mean_profit','sample_sd_profit','min_profit','max_profit','mean_emission','mean_investment','mean_net_carbon_cost','mean_open_dc','mean_all_w','mean_max_w','mean_wall_time_sec','mean_actual_bp_calls','mean_cache_hit_rate','mean_cg_iterations','mean_branch_nodes']
 sums=[]
 for (dim,lev),rr in sorted(groups.items()):
  def vals(k): return [num(x,k) for x in rr if math.isfinite(num(x,k))]
  def mean(k):
   v=vals(k); return statistics.mean(v) if v else float('nan')
  def sd(k):
   v=vals(k); return statistics.stdev(v) if len(v)>1 else 0.0
  sums.append({'screening_dimension':dim,'level':lev,'n':len(rr),'completion_rate':len(rr)/3,'mean_profit':mean('total_profit'),'sample_sd_profit':sd('total_profit'),'min_profit':min(vals('total_profit')),'max_profit':max(vals('total_profit')),'mean_emission':mean('carbon_emission'),'mean_investment':mean('total_investment_cost'),'mean_net_carbon_cost':mean('net_carbon_trading_cost'),'mean_open_dc':mean('num_dc_open'),'mean_all_w':statistics.mean([sum(float(z) for z in x['best_w'].split(';'))/10 for x in rr]),'mean_max_w':statistics.mean([max(float(z) for z in x['best_w'].split(';')) for x in rr]),'mean_wall_time_sec':mean('wall_time_sec'),'mean_actual_bp_calls':mean('fitness_bp_solves'),'mean_cache_hit_rate':mean('cache_hit_rate'),'mean_cg_iterations':mean('cg_iterations'),'mean_branch_nodes':mean('branch_nodes')})
 with (root/'RQ2_SCREENING_SUMMARY_BY_LEVEL.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=sk);w.writeheader();w.writerows(sums)
 with (root/'RQ2_SCREENING_LEVEL_DIAGNOSTICS.csv').open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['screening_dimension','level','flags','basis'])
  for s in sums:
   flags=[]
   if s['mean_all_w']<=.01 or s['mean_max_w']<=.02 or s['mean_all_w']>=.95: flags.append('DEGENERATE_BOUNDARY_CANDIDATE')
   w.writerow([s['screening_dimension'],s['level'],';'.join(flags),'predeclared boundary rule'])
 with (root/'RQ2_SCREENING_SELECTION_INPUT.csv').open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['screening_dimension','candidate_level','selection_status','reason']);
  for dim,levels in [('delta',[500,1000,2000,3000,5000,7500,10000,15000]),('gamma',[.1,.2,.3,.4,.5,.6,.7,.8,.9])]:
   for x in levels:w.writerow([dim,x,'INPUT_FOR_USER_SELECTION','candidate pool; no formal level frozen'])
 def mat(name,ds,gs):
  p=root/name; p.write_text('metadata:\n  status: CANDIDATE_NOT_FROZEN\n  user_confirmation_required: true\n  experiment_stage: FORMAL_CANDIDATE_ONLY\n  formal_paper_statistics: false\nselection_rule: preserve baseline; cover low/mid/high and turning points; apply degeneracy and redundancy diagnostics; user confirmation required\nselected_delta_levels: '+str(ds)+'\nselected_gamma_levels: '+str(gs)+'\nshared_baseline: delta=3000.0, gamma=0.5\nexpected_unique_runs: '+str(10*(len(ds)+len(gs)-1))+'\nexcluded_candidates: not automatically deleted; see RQ2_FORMAL_CANDIDATE_MATRIX_COMPARISON.md\n')
 ds5=[500,2000,3000,7500,15000]; ds6=[500,1000,3000,5000,10000,15000]; ds7=[500,1000,2000,3000,5000,10000,15000]
 gs5=[.1,.3,.5,.7,.9]; gs6=[.1,.2,.4,.5,.7,.9]; gs7=[.1,.2,.3,.5,.7,.8,.9]
 stamp='20260802_1430'
 mat('FORMAL_CANDIDATE_5_LEVELS_PART3_RQ2_'+stamp+'.yaml',ds5,gs5); mat('FORMAL_CANDIDATE_6_LEVELS_PART3_RQ2_'+stamp+'.yaml',ds6,gs6); mat('FORMAL_CANDIDATE_7_LEVELS_PART3_RQ2_'+stamp+'.yaml',ds7,gs7)
 (root/'RQ2_FORMAL_CANDIDATE_MATRIX_COMPARISON.md').write_text('# RQ2 formal candidate comparison\\n\\nThree candidate matrices are provided for user review only (5+5=90, 6+6=110, 7+7=130 unique runs). No matrix is frozen automatically.\\n\\nFinal decision: USER_CONFIRMATION_REQUIRED.\\n')
 (root/'RQ2_SCREENING_REPORT.md').write_text('# Part 3 / RQ2 exploratory screening report\\n\\nexperiment_stage: EXPLORATORY_SCREENING\\nformal_paper_statistics: false\\nvalid completed runs: 48/48\\nNo formal level or conclusion is frozen; candidate matrices require user confirmation.\\n')
 (root/'SCREENING_FINALIZATION_MANIFEST.txt').write_text('batch_status: PART3_RQ2_SCREENING_COMPLETE\\nformal_matrix_status: FORMAL_MATRIX_NOT_FROZEN\\nfinal_decision: USER_CONFIRMATION_REQUIRED\\nvalid_unique_runs: 48\\n')
 return 0
if __name__=='__main__': raise SystemExit(main(Path(sys.argv[1])))
