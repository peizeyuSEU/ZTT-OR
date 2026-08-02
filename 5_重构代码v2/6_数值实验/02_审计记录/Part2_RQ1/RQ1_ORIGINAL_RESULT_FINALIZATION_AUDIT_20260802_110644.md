# Original RQ1 Result Finalization Audit

- audited_batch: formal_20runs_resumable_20260802_1100
- completion: 20/20; NO_INVESTMENT 10/10; OPTIMIZED_INVESTMENT 10/10; pairs 10/10; failed=0; timeout=0
- original core result status: VALID and preserved
- final paper status: SUPERSEDED_BY_CLEAN_RERUN
- paired instance fingerprints: checked for all 10 seeds and equal within each pair
- original summary mean absolute profit change: 170512.81734644898
- original summary sample SD absolute profit change: 47574.133929757954
- original summary mean emissions reduction percent: 41.372112854280736
- original summary sample SD emissions reduction percent: 8.097847900523542
- sample SD convention: ddof=1

## Issues requiring clean rerun

1. Prior wall-time strings contained explanatory text instead of numeric seconds.
2. OPTIMAL was the final inner-BP status, not an outer-GA global-optimality claim.
3. The prior output alternated between saying the optimized w-vector was available and marking its aggregate metric unavailable.
4. The paper table lacked scenario-level and paired network, investment, and carbon fields.

The old single-run results and old final batch are not deleted, overwritten, or mixed into the clean batch.
