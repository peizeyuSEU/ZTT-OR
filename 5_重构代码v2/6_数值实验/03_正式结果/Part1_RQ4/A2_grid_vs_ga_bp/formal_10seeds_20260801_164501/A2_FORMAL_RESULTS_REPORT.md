# A2 Formal Paired Results Report

## Experiment design

- Part 1 RQ4 A2, 5×20 instance, ten unified seeds: 101, 202, 303, 404, 505, 606, 707, 808, 909, 1010.
- Three-bit common discrete space; each of five DCs has levels k/7 for k=0,...,7; Grid evaluates 8^5 = 32,768 vectors.
- Grid Search–BP and GA–BP use the same deterministic instance generator and unified random_seed for each pair.
- Grid uses eight outer workers; GA uses parallel_fitness=true and num_threads=8. Both use serial pricing and cplex_threads=1.

## Completeness and comparison

- Complete paired runs: 10/10.
- Grid certified-complete rate: 100.00%.
- GA final inner-BP certified rate: 100.00%.
- GA reached the Grid optimum in 5/10 pairs (50.00%).
- Mean objective gap (Grid−GA)/|Grid|: 0.071030% (sample SD, ddof=1: 0.125702%).
- Mean evaluation saving: 98.76%.

## Runtime

- Grid wall time mean: 400.12 s (range 214.07–695.24 s).
- GA wall time mean: 5.42 s (range 2.91–11.21 s).

## Interpretation

The Grid result is the deterministic optimum within the common three-bit discrete reduction-rate space, not a claim about a continuous global optimum. The paired comparison evaluates whether GA–BP can reach or approach that discrete-space benchmark while using substantially fewer BP evaluations.

## Decision

**PASS — A2 formal paired experiment complete and included in subsequent paper statistics.**
