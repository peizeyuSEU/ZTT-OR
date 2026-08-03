# P5-3 smoke test report

This is a wiring smoke test only, not a formal real-case experiment and not part of paper statistics. No E0 or quota artifact was generated. Both serial GA-BP runs used seed 101, population 10, five generations maximum, and temporary carbon price/quota 0.

| scenario | status | integer solution | inner BP | wall time (s) | BP calls | cache hits | best-w length |
|---|---|---|---|---:|---:|---:|---:|
| 0 km | PASS | true | OPTIMAL | 0.282194 | 36 | 4 | 8 |
| 10 km | PASS | true | OPTIMAL | 0.314065 | 36 | 4 | 8 |

The base-data fingerprint is identical across the two runs; resolved instance fingerprints differ because the eight same-city arcs and their derived coefficients differ. Both runs completed with outer status HEURISTIC_BEST_FOUND.
