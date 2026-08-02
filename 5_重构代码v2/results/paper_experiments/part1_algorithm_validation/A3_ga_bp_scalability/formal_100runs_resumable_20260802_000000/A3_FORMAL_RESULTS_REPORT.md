# A3 formal scalability results (derived post-processing)

Rebuilt from 100 completed raw task outputs without rerunning A3. Raw result.csv, report.txt, configurations, logs, and completion markers were not modified.

- Completed tasks: 100/100 (10 sizes x 10 seeds); failed/missing/duplicate: 0/0/0.
- Outer interpretation: HEURISTIC_BEST_FOUND; no global-optimality claim.
- Final inner BP: OPTIMAL with integer solution in 100/100 runs.
- Cache rate: per-run actual_fitness_cache_hits / fitness_requests; size summaries are arithmetic means of per-run rates (sample SD, ddof=1).
- Fitness-stage uncertified feasible evaluations (aggregate counter): 44; this does not change final inner-BP certification.

## 30x100 runtime diagnostics

Mean 6174.007 s; sample SD 3903.689 s; median 5760.241 s; range 1462.496--13801.882 s; maximum seed 404. Runtime is not strictly monotone across seeds.

Seed-808 interrupted attempt is preserved and excluded; attempt_05 is the sole official completed source.

## Reproducibility scope

This update changes derived tables, diagnostics, the consistency audit, report, manifest, and rebuild/validation scripts only. No solver rerun or raw solver output modification was performed. No final paper narrative or scientific conclusion is asserted here.

## Per-size derived cache and fitness-stage statistics

| size | mean cache-hit rate | mean actual BP solves | mean CG iterations | mean branch nodes |
|---|---:|---:|---:|---:|
| 3x5 | 18.561869% | 302.700 | 1034.700 | 2.200 |
| 3x10 | 18.377062% | 354.100 | 2372.300 | 2.400 |
| 5x10 | 17.850561% | 424.100 | 3423.900 | 15.500 |
| 5x20 | 18.210887% | 435.300 | 9155.900 | 59.300 |
| 5x30 | 18.451149% | 465.000 | 18463.700 | 23.400 |
| 10x30 | 17.602128% | 533.300 | 13224.500 | 55.900 |
| 15x30 | 17.585711% | 551.200 | 12589.000 | 146.300 |
| 15x50 | 18.316513% | 540.500 | 25307.200 | 330.100 |
| 20x50 | 18.998563% | 525.500 | 18458.500 | 357.400 |
| 30x100 | 17.746722% | 588.900 | 146074.300 | 4678.300 |
