# Part 1 A2 Grid Search–BP feasibility precheck

## Scope

This is a runtime-feasibility precheck, not a formal paper replication and not part of the paper statistics. It uses the frozen algorithm code at commit `0dd725354732f9b8011e9e7e8540e0e64a3ff223`. The annotated tag object is `ee021f66c1ffebe373a1e5038a0bcbd1d863e622`; dereferencing the tag gives the same code commit `0dd725354732f9b8011e9e7e8540e0e64a3ff223`.

The grid is correctly defined as:

```text
5 candidate DCs × 3-bit encoding
8 levels per DC: k/7, k=0,...,7
8^5 = 2^15 = 32,768 combinations
```

The deterministic enumeration is lexicographic, from `grid_id=0: (0,0,0,0,0)` through `grid_id=32767: (7,7,7,7,7)`.

## Configuration

The instance is `5×20`, `random_seed=101`, `chromosome_length=3`, and every fixed-w vector uses `w_j=k_j/7`. BP uses the formal baseline values: `delta=3000`, `investment_exponent=0.5`, `pricing_algorithm=0`, `pricing_max_cols_per_dc=3`, `dual_smooth_alpha=0`, `max_cg_iterations=2000`, `max_branch_nodes=10000`, `bp_time_limit_sec=600`, `bp_relative_gap=0`, `parallel_pricing=false`, and `cplex_threads=1`. The outer precheck used eight independent worker processes; each BP/CPLEX instance remained single-threaded.

## Results

| Measure | Result |
|---|---:|
| Expected combinations | 32,768 |
| Completed combinations | 32,768 |
| Unique IDs | 32,768 (`0`–`32767`) |
| Certified optimal BP results | 32,768 |
| Uncertified results | 0 |
| Timeouts | 0 |
| Failed combinations | 0 |
| Non-integer results | 0 |
| External wall time | 696.74 s (11:36.74) |
| User CPU time | 5,382.04 s |
| System CPU time | 164.20 s |
| CPU utilisation | 796% |
| Maximum parent RSS | 150,136 kB |
| Average individual BP time | 0.1642 s |
| Maximum individual BP time | 1.8943 s |
| Best grid ID | 12,690 |
| Best objective | 1,352,816.61395718 |
| Best w | (3/7, 0, 6/7, 2/7, 2/7) |

All 32,768 combinations have an individual preserved run directory under `runs/`, together with the aggregated `GRID_EVALUATIONS.csv`. No error, timeout, fork, CPLEX license, or file-collision message was observed.

## Formal-load estimate

At the measured eight-worker throughput, ten same-sized seeds would require approximately 1.9 wall-clock hours before queueing and retry overhead. This is an operational estimate only; it is not a formal result and does not change the Part 1 matrix.

## Decision

**PASS — retain A2 primary size 5×20 for formal experiments.**

The complete grid is operationally feasible and stable under the prescribed eight-worker outer parallelism. This precheck is excluded from paper statistics. Any later formal run must use its own seed-specific result directory and preserve the required configuration, resolved configuration, manifest, logs, and result tables.
