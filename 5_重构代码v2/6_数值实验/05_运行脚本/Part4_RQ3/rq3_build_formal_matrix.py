from pathlib import Path
import datetime,yaml
root=next(p for p in Path.cwd().iterdir() if p.is_dir() and p.name.startswith("5_") and p.name!="5_*"); stamp=datetime.datetime.now().strftime("%Y%m%d_%H%M%S"); d=root/"6_????"/"01_????"/"Part4_RQ3";d.mkdir(parents=True,exist_ok=True)
prices=[0,1,2,4,6,10,20];seeds=[101,202,303,404,505,606,707,808,909,1010];caps=[0,40000,100000,200000,350000]
data={"metadata":{"document_name":"FORMAL_EXPERIMENT_MATRIX_PART4_RQ3","status":"FINAL_FROZEN_PENDING_EXECUTION_APPROVAL","generated_at":stamp,"research_question":"RQ3","experiment_part":"Part4","algorithm":"GA-BP","formal_algorithm_commit":"0dd725354732f9b8011e9e7e8540e0e64a3ff223","formal_algorithm_tag":"paper-exp-baseline-20260801","formal_experiment_start_allowed":False},"problem":{"size_id":"10x30","num_dc":10,"num_retailers":30},"model_parameters":{"delta":3000.0,"investment_exponent":0.5,"carbon_cap":100000.0,"carbon_price_levels":prices,"allowance_levels":caps},"ga_parameters":{"population_size":30,"max_generation":50,"crossover_rate":0.7,"mutation_rate":-1,"elitism":True,"early_stop":True,"convergence_generations":10,"convergence_tolerance":0.001,"parallel_fitness":True,"num_threads":8,"chromosome_length":10},"bp_parameters":{"pricing_algorithm":0,"pricing_max_cols_per_dc":3,"dual_smooth_alpha":0.0,"rc_eps":1e-6,"max_cg_iterations":2000,"max_branch_nodes":10000,"bp_time_limit_sec":600.0,"bp_relative_gap":0.0,"parallel_pricing":False,"cplex_threads":1},"seeds":seeds,"task_key":"(random_seed, carbon_price)","solver_runs":{"carbon_price_formal_runs":70,"allowance_additional_solver_runs":0,"total_new_ga_bp_runs":70},"analytical_observations":{"allowance_levels":5,"formal_seeds":10,"derived_allowance_observations":50},"execution_policy":{"serial_scenario_runs":1,"resource_lock_required":True,"do_not_start_without_user_approval":True,"formal_allowance_results_are_analytical":True}}
path=d/f"FORMAL_EXPERIMENT_MATRIX_PART4_RQ3_{stamp}.yaml";path.write_text(yaml.safe_dump(data,sort_keys=False,allow_unicode=True))
md=d/f"FORMAL_EXPERIMENT_MATRIX_PART4_RQ3_{stamp}.md";md.write_text(f"""# Part4 / RQ3 formal experiment matrix

Status: FINAL_FROZEN_PENDING_EXECUTION_APPROVAL

This matrix defines 70 formal GA-BP solver runs: 7 carbon-price levels (0, 1, 2, 4, 6, 10, 20) times 10 frozen seeds. The task key is (random_seed, carbon_price).

The five allowance levels (0, 40000, 100000, 200000, 350000) add zero solver runs. After the ten carbon-price=2 baseline runs are complete, 50 allowance observations will be analytically derived using the verified cap-profit identity. They must be labelled analytical derivations, not independent GA-BP runs.

Frozen common settings: size 10x30, delta=3000, gamma=0.5, GA population 30, maximum 50 generations, outer fitness parallelism 8, pricing serial, CPLEX one thread. Formal execution is not started by creation of this matrix and requires user approval.

Screening levels 8, 12 and 16 remain preserved in the completed screening records and are not deleted; they are excluded only from the seven-level formal matrix.
""")
print(path,md)

