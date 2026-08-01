# GA–BP parallelism precheck decision

Status: finite resource precheck only. This is not one of the ten formal replications. No solver core code was changed and no GitHub commit was made.

The requested `results/paper_experiments/EXPERIMENT_PROTOCOL.md` was not present at audit time; this report therefore follows the explicit protocol in the task and the other four supplied baseline documents.

## Server resources

- CPU: Intel Core i9-10980XE, 36 logical CPUs, 18 physical cores, 2 threads/core.
- NUMA: 1 node; CPUs 0–35.
- Memory: 251 GiB total; approximately 234 GiB available when audited.
- `nproc`: 36.
- Shell limits: no CPU-time or virtual-memory limit; open files 1024; max user processes 1,029,394; swap 2 GiB (unused at audit).
- No scheduler allocation or per-process memory cap was reported by the current shell. CPLEX was configured with one thread per BP.

## Fixed precheck protocol

All candidates used the same 10×30 instance, `delta=3000`, `gamma=0.5`, population 30, five generations, crossover 0.7, elitism enabled, mutation sentinel -1, `early_stop=false`, `dual_smooth_alpha=0`, `pricing_algorithm=0`, `pricing_max_cols_per_dc=1`, `parallel_pricing=false`, `cplex_threads=1`, and the same code commit/tag.

The code exposes only one `random_seed` field. It cannot independently set instance seed 101 and GA seed 1101 without a core-code change. Therefore all four runs used `random_seed=101`; this limitation is recorded rather than hidden.

The 30×100 fixed-w smoke test remains correctly classified as serial: it used `parallel_fitness=false` and `parallel_pricing=false`.

## Results

| Candidate | Parallel fitness | Threads | Wall s | User s | System s | CPU % | Parent max RSS KB | Best objective | BP calls | Cache hits | Status |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| t1 | false | 1 | 47.50 | 47.10 | 0.35 | 99 | 16696 | 2368864.71113423 | 127 | 23 | complete |
| t2 | true | 2 | 27.30 | 51.14 | 0.66 | 189 | 15232 | 2368864.71113423 | 127 | 23 | complete |
| t4 | true | 4 | 15.57 | 50.48 | 0.59 | 328 | 15264 | 2368864.71113423 | 127 | 23 | complete |
| t8 | true | 8 | 10.67 | 52.37 | 0.57 | 496 | 14980 | 2368864.71113423 | 127 | 23 | complete |

Cache hit rate is 23/150 = 0.1533333333 for every candidate. Each run had 5 generations, 150 fitness requests, 127 actual BP solves, 127 certified optimal inner solves, zero uncertified feasible results, zero no-integer results, and zero fitness-evaluation failures.

## Consistency and stability

- Final objective values are identical to displayed precision.
- The complete five-generation best/average/worst trajectory is identical across all four runs.
- Request counts, BP calls, cache hits, columns, and solver statuses are identical.
- The current logs do not serialize the initial chromosome matrix, so initial-population identity cannot be proven independently; identical full trajectories and counts are strong evidence that the same random path was preserved.
- No `ERROR`, `SOLVER_ERROR`, `TIME_LIMIT`, `CG_ITERATION_LIMIT`, fork, thread, memory, CPLEX-license, or file-content error was observed in the candidate logs.
- `/usr/bin/time -v` maximum RSS is the launcher/parent process measurement, not an aggregate over all forked children; it should not be interpreted as total GA memory.

## Decision

Recommended formal setting:

```yaml
parallel_fitness: true
num_threads: 8
parallel_pricing: false
cplex_threads: 1
dual_smooth_alpha: 0
```

The recommendation is 8 because it is the smallest tested setting at the best observed performance frontier: 8 reduced wall time by about 31% versus 4, while remaining below half of the 18 physical cores, with matching results and no observed instability. Two and four are stable fallbacks; one is the serial reference. We do not select `num_threads=0`, and we do not occupy all logical/physical cores.

This is a one-run-per-candidate precheck, not a statistical performance claim. Before the ten formal replications, the resolved configuration must explicitly record `num_threads=8`, and each formal run must archive its own resource and result artifacts.
