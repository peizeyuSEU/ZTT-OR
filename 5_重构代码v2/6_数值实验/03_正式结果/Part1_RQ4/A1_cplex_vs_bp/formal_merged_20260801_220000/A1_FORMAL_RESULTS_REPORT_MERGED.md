# A1 — Complete-column CPLEX versus fixed-w Branch-and-Price

PASS — A1 complete-column CPLEX versus fixed-w Branch-and-Price experiment completed across all retained scales.

- Unified paired comparisons: 180
- Existing A1: 90 pairs, rerun with the unified end-to-end timing protocol.
- Added A1 extension: 90 pairs at 5×14, 5×16, and 5×18.
- All retained scales and fixed-w levels use ten seeds.
- All CPLEX runs are OPTIMAL with gap 0; all BP runs are OPTIMAL_CERTIFIED with gap 0.
- All paired solutions pass the original-model feasibility checks.

The precheck-only 5×12 and 5×20 runs are excluded from formal statistics. The 5×18 target was selected after sequential precheck and three-seed confirmation.

The paper table is `A1_PAPER_TABLE_MERGED.csv`. `mean_column_reduction_ratio` is marked UNKNOWN because the frozen BP public interface exposes cumulative generated columns but not a certified final-master-column count; no proxy is substituted.
