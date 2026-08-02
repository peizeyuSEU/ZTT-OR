# RQ1 Code and Output Audit

- Objective used for pairing: `total_profit` from `result.csv`; it is reported as profit (not negated minimization objective).
- NO_INVESTMENT path: `run_mode: fixed_w`, top-level `fixed_w` vector of ten zeros, with `min_w=max_w=0` recorded in the run config.
- OPTIMIZED_INVESTMENT path: `run_mode: ga`, `min_w=0`, `max_w=1`, chromosome length 10.
- Carbon fields directly available in result.csv: `carbon_emission`, `carbon_cap`, `e_plus`, `e_minus`, `net_carbon_trading_cost`.
- Investment field directly available: `total_investment_cost`.
- Open facilities and w vector are available through result.csv/report outputs; unsupported facility/transport/inventory component costs are marked `UNAVAILABLE_FROM_FROZEN_INTERFACE` and are not inferred.
- Pair identity includes seed, size, algorithm commit/tag, config SHA-256 and derived instance fingerprint.
