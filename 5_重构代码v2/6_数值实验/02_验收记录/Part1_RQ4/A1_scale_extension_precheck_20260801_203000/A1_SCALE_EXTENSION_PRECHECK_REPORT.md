# A1 scale extension precheck

All five sequential precheck scales completed with certified matching objectives.

| Scale | Complete columns | CPLEX total (s) | BP total (s) | Peak RSS (MiB) | Status |
|---|---:|---:|---:|---:|---|
| 5×12 | 40890 | 0.478122 | 0.011014 | 80.2 | OPTIMAL / OPTIMAL_CERTIFIED |
| 5×14 | 163760 | 4.048943 | 0.023563 | 305.6 | OPTIMAL / OPTIMAL_CERTIFIED |
| 5×16 | 655270 | 12.373130 | 0.032516 | 1203.0 | OPTIMAL / OPTIMAL_CERTIFIED |
| 5×18 | 2621340 | 49.865363 | 0.117985 | 5021.1 | OPTIMAL / OPTIMAL_CERTIFIED |
| 5×20 | 10485650 | 201.923513 | 0.521012 | 23791.0 | OPTIMAL / OPTIMAL_CERTIFIED |

Timing is end-to-end per method; component timings are retained in each pair result.
The 5×20 run took 201.924 s and remained below the 300 s hard stop with approximately 18.1 GiB peak RSS. No larger precheck scale was started.
