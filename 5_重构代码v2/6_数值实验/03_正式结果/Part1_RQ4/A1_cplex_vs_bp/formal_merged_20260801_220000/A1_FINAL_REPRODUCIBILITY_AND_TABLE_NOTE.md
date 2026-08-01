# A1 final reproducibility and paper-table note

## Final experiment

- Formal scales: 3x5, 3x10, 5x10, 5x14, 5x16, 5x18
- Formal paired comparisons: 180
- CPLEX certified: 180/180
- BP certified: 180/180
- Objective matches: 180/180
- Maximum absolute objective difference: 2.3283064365386963e-10
- Maximum relative objective difference: 2.7415052240088361e-14%
- Core solver modified: false

The 5x12 and 5x20 runs were precheck-only and are excluded from formal statistics. The original 90 pairs were rerun under the unified end-to-end timing protocol before merging with the 90 added pairs.

## Script reproducibility audit

- Uncommitted script: `a1_formal_batch.py`
- Uncommitted script SHA-256: `02f6b75bfb9390dacd76e11b2563b1d0790d565747449ea4d3a4aa9ecc5660f6`
- `uncommitted_script_used_for_formal_A1: true`
- Evidence: mtime 20:21:30 on 2026-08-01 and retimed result files written at 20:21:36. The change only selects unified timing fields when summarizing results; it does not alter model construction, solver code, scales, or parameters.
- Timestamped copy: `a1_formal_batch_used_20260801_2021.py`.
- The extension used `a1_extension_batch.py`; merging used `a1_merge_results.py`.
- The original uncommitted script and `project_docs/` remain untouched.

## Memory fields

The original paired CSVs retain `mean_reference_peak_rss_mb` and `mean_bp_peak_rss_mb` as raw server records. They are excluded from the final paper table because CPLEX and BP ran sequentially in one controller process and process-level `ru_maxrss` is a cumulative lifetime peak that cannot be attributed reliably to either method. Equal RSS values must not be interpreted as equal method memory usage.

`mean_column_reduction_ratio` is also excluded because the frozen BP interface exposes cumulative generated columns but not a certified final-master-column count; `bp_generated_columns` is not used to infer a reduction ratio.

## Paper data file

The formal paper table is `A1_PAPER_TABLE_FINAL.csv`; `A1_PAPER_TABLE_MERGED.csv` is preserved unchanged.
