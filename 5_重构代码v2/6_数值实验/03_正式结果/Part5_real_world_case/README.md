# Part5 Real-World Case Evidence

This directory contains the selective, reproducibility-ready Part5 evidence package.

## Dataset lifecycle

- Historical formal execution dataset: `b9fc758dbe11762882bf66cbbc7672da0918adb7`.
- Current public dataset snapshot: `5b22a235b6c6712de0d98441fc4a4d5e6780e3c5`.

The E0 baseline and all 50 high-precision Part5 distance-sensitivity runs (five distances times ten seeds) were executed against `b9fc...`. At that time, `carbon_quota` was `not_yet_generated`. The neutral allowance was derived from the D10, no-investment E0 result:

`E0 = 14442.842886043889 tCO2e/year`.

The current public dataset preserves the same seven optimization-relevant processed CSV inputs and adds the model-derived carbon-quota artifact. It is not the historical execution commit.

## Source and execution provenance

- Frozen synthetic model baseline: `0dd725354732f9b8011e9e7e8540e0e64a3ff223`.
- Formal Part5 explicit-arc extension: `fb7f435008a60c91b9e0908fd64d3239b4197fcd`.
- Formal high-precision execution code: `1566b7fca4d9311e1fe8195834ea46c770b75445`.
- Adapter source: `5_重构代码v2/apps/real_case/part5_adapter.py`.

The adapter accepts both legal dataset states, verifies all seven optimization-input CSV hashes, and rejects quota or optimization-input tampering. The published quota is an E0-derived research parameter, not an observed government allowance.

## Evidence included

- `e0_baseline_20260804_20260804_111000/`: E0 artifacts and manifests.
- `formal_distance_high_precision_20260804_130000/`: 50 seed-level high-precision formal runs and aggregate outputs.
- `part5_final_paper_materials/`: paper-ready tables, figure data, narrative, and provenance manifests.

Temporary dry-run directories, controllers, caches, binaries, superseded runs, and failed runs are intentionally excluded.
