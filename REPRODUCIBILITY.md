# Reproducibility

## Model and code

- Frozen synthetic baseline: `0dd725354732f9b8011e9e7e8540e0e64a3ff223`.
- Formal tag: `paper-exp-baseline-20260801`.
- Part5 explicit-arc extension: `fb7f435008a60c91b9e0908fd64d3239b4197fcd`.
- Formal execution code: `1566b7fca4d9311e1fe8195834ea46c770b75445`.
- Integration branch: `repro/part5-release`.

## Dataset versions

Historical formal-run dataset:

`b9fc758dbe11762882bf66cbbc7672da0918adb7`

Current public dataset:

`5b22a235b6c6712de0d98441fc4a4d5e6780e3c5`

The seven optimization-relevant processed inputs are byte-identical between these commits:

`nodes.csv`, `supplier_dc.csv`, `dc_market.csv`, `market_params.csv`, `dc_params.csv`, `inventory_params.csv`, and `dc_emission_params.csv`.

Only `case_config.json` changed, adding the model-derived quota metadata and artifact references after E0 derivation.

## Part5 evidence paths

All public Part5 evidence is under:

`5_重构代码v2/6_数值实验/03_正式结果/Part5_real_world_case/`

The E0 evidence is in `e0_baseline_20260804_20260804_111000/`. The 50 formal runs (five distances times ten seeds) are in `formal_distance_high_precision_20260804_130000/`. Paper-ready tables and figure data are in `part5_final_paper_materials/`.

## Key reported quantities

- E0: `14442.842886043889`.
- D10 mean total emission over ten formal runs: `14407.406144923654`.
- D10/seed808: six of eight candidate DCs opened and twelve of fifteen markets served.

The 6/8 and 12/15 values are seed808-level results and must not be interpreted as the D10 mean.

## Reproduction policy

Use the historical dataset commit to reproduce the exact formal-run provenance. Use the current public dataset to reproduce the same optimization-relevant input files plus the published E0-derived quota artifact. The adapter's dual-state validation must pass before any rerun.
