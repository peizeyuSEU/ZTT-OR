# Formal GA--BP baseline candidate decisions

Status: candidate only. No formal experiment was started, no existing configuration was changed, and no GitHub commit was created.

## Fixed decisions

- The 30×100 size is **experiment-specific**, not a global baseline parameter. It remains documented only as the previously completed smoke test.
- Provisional investment coefficient: `delta=3000`.
- Sensitivity level: `delta=5000`.
- Investment exponent: `gamma=0.5`.
- GA parameters: population `30`, maximum generations `50`, crossover `0.7`, elitism enabled, and `mutation_rate=-1` (the code sentinel for automatic `1/chromosome_length`).
- Early stopping is enabled with `convergence_generations=10` and `convergence_tolerance=0.001`.

## Smoke configuration versus formal GA candidate

| Item | 30×100 smoke | Formal GA candidate |
|---|---|---|
| Role | Fixed-w validation run | Outer GA over w |
| Dimensions | 30 DC, 100 retailers | Experiment-specific; not selected here |
| Delta | 5000 | Provisional 3000; 5000 sensitivity |
| Fixed w | 0.8 | Not applicable; GA chromosome generates w |
| GA | Not used | 30 / 50 / 0.7 / elitism / mutation -1 |
| Early stop | Disabled | Enabled: 10 generations, 0.001 tolerance |
| Instance and GA seeds | Smoke seed 32501 | Protocol lists 10 instance seeds and 10 GA seeds; not run |
| CG limit | 1000 in smoke | 1000 or 2000 are both source-backed candidates; selection pending |
| Pricing columns | K=1 | K=1 or K=3; selection pending validation |
| Parallel evaluation | Disabled in smoke | Enabled; fixed `num_threads=8` after finite precheck |
| Dual smoothing | 0 in smoke | 0 is the only validated value; alternative is not established |
| Time limit | 600 seconds per BP | 600 seconds per BP; full-GA limit is unknown |

## Multi-column, smoothing, and parallel candidates

The audit does not justify silently choosing accelerated settings. The candidate values and their evidence are therefore explicit:

- Multi-column pricing: `K=1` is the completed smoke baseline. `K=3` appears as the adaptive maximum and in prior acceleration discussion, but must be validated for the formal protocol before selection. It should be described as multi-column pricing with `K=3`, not as a proven exact Top-K implementation unless separately verified.
- Dual smoothing: `alpha=0` is selected. The completed 30×100 result and repeated prior tests found no useful improvement from smoothing.
- Parallel fitness evaluation: `true` with fixed `num_threads=8` is selected after the finite 10×30 precheck. The 30×100 smoke itself remains serial and is not reclassified.
- Parallel pricing: `false` is selected, matching the completed 30×100 smoke. `pricing_threads=4` is recorded as the configured upper bound, but effective pricing parallelism was 1 because parallel pricing was disabled.
- The server has 36 logical CPUs (18 cores × 2 threads). The 30×100 baseline did not use them for parallel GA evaluation or parallel pricing; the precheck selected 8 GA worker processes, leaving a resource margin. The historical launcher has different parallel flags and is not authoritative for this candidate.
- Fitness caching remains restricted to certified optimal inner-BP results. Uncertified feasible outcomes may be used for the current evaluation but must not be treated as exact cache hits.

## Remaining decisions before any run

1. Select the actual `num_dc` and `num_retailers` per experiment; these are deliberately absent from the global candidate.
2. Select K=1 or K=3 before formal runs; smoothing is fixed at 0 and parallel evaluation/pricing are fixed at `true`/`false` with `num_threads=8`.
3. Select `max_cg_iterations` explicitly: 1000 is smoke-derived, while 2000 is the code default.
4. Fix the process/thread allocation and record it in the resolved configuration.
5. Determine the complete GA wall-clock budget through a pre-experiment. The code audit found only the per-BP `600s` limit, not a dedicated full-GA limit.
6. Resolve physical units from the manuscript/data protocol. No parameter may rely on an implicit code default.

Until these decisions are recorded, this YAML is a review artifact and must not be used to launch the ten formal replications.
