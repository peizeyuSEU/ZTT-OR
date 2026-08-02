# Part 2 / RQ1 Final Decisions

- Matrix frozen after controlled A3 pause at 97/100 completed runs.
- Representative size: 10x30; paired seeds: 101,202,303,404,505,606,707,808,909,1010.
- NO_INVESTMENT is fixed-w with all ten entries zero; OPTIMIZED_INVESTMENT is the frozen GA-BP configuration.
- One scenario at a time under the shared experiment resource lock.
- Seed 101 paired precheck is excluded from formal statistics.
- Formal optimized results are best-found heuristic solutions; no global-optimality claim.
- Output objective is `total_profit`; unsupported component costs are not inferred.
