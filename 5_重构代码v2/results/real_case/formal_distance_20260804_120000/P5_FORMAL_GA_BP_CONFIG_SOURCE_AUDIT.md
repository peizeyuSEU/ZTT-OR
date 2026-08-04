# P5 formal GA-BP configuration source audit

authoritative_source_file=/home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/5_重构代码v2/results/paper_experiments/part4_carbon_policy/RQ3_price_cap/formal_70runs_resumable_20260803_152114/runs/seed_101/PRICE_02_CAP_100000/resolved_config.yaml
source_commit=204d6c2357e19fb60f2e8175f79f2072bbc5e349

| parameter | source value | P5 value | reason |
|---|---|---|---|
| population_size | 30 | 30 | shared formal GA-BP parameter |
| max_generation | 50 | 50 | shared formal GA-BP parameter |
| crossover_rate | 0.7 | 0.7 | shared formal GA-BP parameter |
| mutation_rate | -1 | -1 | shared formal GA-BP parameter |
| elitism | true | true | shared formal GA-BP parameter |
| early_stop | true | true | shared formal GA-BP parameter |
| convergence_generations | 10 | 10 | shared formal GA-BP parameter |
| parallel_fitness | true | true | shared formal GA-BP parameter |
| num_threads | 8 | 8 | shared formal GA-BP parameter |
| chromosome_length | 10 | 10 | shared formal GA-BP parameter |
| pricing_algorithm | 0 | 0 | shared formal GA-BP parameter |
| pricing_max_cols_per_dc | 3 | 3 | shared formal GA-BP parameter |
| pricing_adaptive_cols | false | false | shared formal GA-BP parameter |
| max_cg_iterations | 2000 | 2000 | shared formal GA-BP parameter |
| cg_early_stop | false | false | shared formal GA-BP parameter |
| max_branch_nodes | 10000 | 10000 | shared formal GA-BP parameter |
| bp_time_limit_sec | 600.0 | 600.0 | shared formal GA-BP parameter |
| bp_relative_gap | 0.0 | 0.0 | shared formal GA-BP parameter |
| rc_eps | 1.0e-6 | 1.0e-6 | shared formal GA-BP parameter |
| parallel_pricing | false | false | shared formal GA-BP parameter |
| pricing_threads | 4 | 4 | shared formal GA-BP parameter |
| core_budget | 0 | 0 | shared formal GA-BP parameter |
| cplex_threads | 1 | 1 | shared formal GA-BP parameter |
| root_heuristic | true | true | shared formal GA-BP parameter |
| root_rmp_mip_heuristic | false | false | shared formal GA-BP parameter |
| root_rmp_mip_time_limit_sec | 5.0 | 5.0 | shared formal GA-BP parameter |
| investment_exponent | 0.5 | 0.5 | shared formal GA-BP parameter |
| use_sqrt_investment | true | true | shared formal GA-BP parameter |
| use_invest_in_column | true | true | shared formal GA-BP parameter |
| min_w | 0.0 | 0.0 | shared formal GA-BP parameter |
| max_w | 1.0 | 1.0 | shared formal GA-BP parameter |

P5 overrides: distance, data/output path, carbon price=2, carbon quota=10km E0, explicit arc-cost mode, random seed.
