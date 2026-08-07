# Part5 Integration Release Candidate Report

## Status

READY_FOR_FINAL_GATE

## Base

- origin/master: `6127b0cb7a42b03629794d7720c5f1cb411e74e8`
- integration branch: `repro/part5-release`
- Commit A: `48e03b41679a660a15efba71175ac2bb93095687`

## Dataset states

- Historical execution: `b9fc758dbe11762882bf66cbbc7672da0918adb7`
- Current public snapshot: `5b22a235b6c6712de0d98441fc4a4d5e6780e3c5`
- Seven optimization-input CSVs: byte-identical.
- Allowed case-config delta: model-derived quota/provenance metadata only.

## Adapter behavior

- Historical state: PASS.
- Current published state: PASS.
- Wrong quota: rejected.
- Modified optimization input CSV: rejected.
- Modified optimization-relevant case-config parameter: rejected.

## Regression

- Release build: PASS.
- Existing unit tests: PASS.
- 225 deterministic pricing regressions: PASS.
- BP integration test: PASS.
- Part5 formal entrypoint compilation: PASS.
- No formal optimization rerun was performed.

## Part5 source integrated

- `5_重构代码v2/apps/real_case/part5_adapter.py`
- `5_重构代码v2/apps/real_case/real_case_e0_baseline_main.cpp`
- `5_重构代码v2/apps/real_case/real_case_formal_main.cpp`
- `5_重构代码v2/apps/real_case/real_case_ga_bp_main.cpp`
- Explicit arc support in the three audited shared headers.

## Part5 evidence integrated

- E0 evidence package: present.
- 50 high-precision formal runs (five distances times ten seeds): present.
- Paper-ready materials: present.
- Temporary dry/controller/cache/binary files: excluded.

## Key provenance

- E0: `14442.842886043889`.
- D10 mean emission: `14407.406144923654`.
- D10/seed808: 6/8 DCs open and 12/15 markets served.

## Git safety

- Part1-Part4 results were not modified.
- No source files outside the audited Part5 whitelist were added.
- No deletions, tags, master merge, or push were performed.

## Recommendation

SAFE_TO_MERGE
