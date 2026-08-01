# Final baseline smoke acceptance — 2026-08-01

## Conclusion

**PASS — formal baseline configuration is accepted**

This was one pre-formal full-configuration smoke test, not a formal replication and not part of any ten-replication summary.

## Run identity

- Result directory: /home/peizeyu2026/smart_wolf_project/ZTT-OR/20260715 ZTT-OR/5_重构代码v2/results/paper_experiments/final_baseline_smoke_10x30_seed101_20260801_150916
- Code commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223
- Git tag: paper-exp-baseline-20260801
- Branch: master
- Size: 10×30
- Unified random_seed: 101, used by both instance generation and GA RNG because the frozen implementation exposes one seed field.
- Start: 2026-08-01T15:10:29+08:00
- End: 2026-08-01T15:10:46+08:00

## Parsed configuration

The archived config_used.yaml and resolved_config.yaml explicitly contain all run fields. The startup diagnostics and result CSV confirmed: delta=3000, gamma=0.5, population 30, maximum generation 50, early stop 10/0.001, pricing algorithm 0, K=3, smoothing 0, CG limit 2000, node limit 10000, BP limit 600s, gap 0, GA fitness parallelism enabled with 8 workers, pricing parallelism disabled, CPLEX one thread, investment in column profit enabled, square-root investment enabled, direct-distance transport disabled, CG early stop disabled, and legacy mode disabled.

## Results

- Overall outcome: HEURISTIC_BEST_FOUND (outer GA is heuristic).
- Final inner-BP status: OPTIMAL.
- Integer solution: yes.
- Final objective: 2371052.3565378.
- GA generations completed: 15.
- Termination: early stopping after 10 consecutive generations without improvement.
- Wall time: 16.86 s.
- User CPU: 109.42 s.
- System CPU: 1.20 s.
- CPU utilization: 655%.
- Maximum parent RSS: 15460 KB; this does not aggregate forked children.
- Fitness requests: 450.
- Actual BP calls: 362.
- Certified optimal BP calls: 362.
- Cache hits: 88; cache hit rate 88/450 = 0.1955556.
- Uncertified feasible BP: 0.
- No-integer BP: 0.
- Fitness failures: 0.
- Total CG iterations: 9920; average 9920/362 = 27.4033 per BP.
- Branch nodes: 5; processed 372; pruned 3; remaining active 0.

## Acceptance checks

All four GA worker settings were active in startup diagnostics: parallel_fitness=true, configured/effective num_threads=8. Pricing diagnostics reported parallel_pricing=false; CPLEX reported one thread. The result CSV recorded pricing_max_cols_per_dc=3, max_cg_iterations=2000, and all BP results were certified optimal.

No ERROR, SOLVER_ERROR, RMP_NOT_OPTIMAL, CG_ITERATION_LIMIT, TIME_LIMIT, fork failure, thread failure, CPLEX license failure, file collision, or child-process failure was observed. The final result, report, convergence file, logs, manifest, timing file, and both configuration files are present.

The complete run log is retained in the result directory. This smoke result must not be inserted into the formal ten-replication statistical tables.
