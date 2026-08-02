# RQ2 investment exponent implementation audit

- Audit stage: pre-run implementation audit
- Algorithm commit: 0dd725354732f9b8011e9e7e8540e0e64a3ff223
- Formal tag: paper-exp-baseline-20260801
- Date: 2026-08-02

## Findings

| Check | Result | Evidence |
|---|---|---|
| Config field name | PASS | src/10_common/Config.h:146,610-616 defines and parses investment_exponent, validating 0 < gamma <= 1. |
| Resolved serialization | PASS | Config::writeYaml() emits investment_exponent; the frozen 10x30 baseline resolved config contains it. |
| Investment formula | PASS | src/7_formula/05_InvestmentCost.h:28-33 computes 0.5 * delta * w_j^2 * pow(D_j, exponent). |
| Column/objective path | PASS | src/2_orchestrator/Orchestrator.h:316-325 recomputes the same configured exponent for reported total investment; the column-profit path receives the configured exponent through the formula layer. |
| use_sqrt_investment semantics | PASS | It selects the demand-scale versus legacy quadratic model; it does not overwrite investment_exponent. |
| RQ2 requested gamma levels | PASS | All requested values 0.1--0.9 satisfy parser validation and will be written explicitly per run. |
| Unknown-key safety | CONTROLLED WARNING | Config::setField() has an intentional ignore-unknown-fields path. The RQ2 controller will use an allow-list and compare every run key against resolved_config.yaml; no unknown key is present in the supplied matrix. A misspelled future key would otherwise be silently ignored by the core parser. |

## Decision

The requested investment-exponent implementation is confirmed for the supplied RQ2 matrix. Screening can proceed only with the external key allow-list/resolved-config comparison recorded in each precheck and run manifest. No src/ or include/ file is modified by this task.
