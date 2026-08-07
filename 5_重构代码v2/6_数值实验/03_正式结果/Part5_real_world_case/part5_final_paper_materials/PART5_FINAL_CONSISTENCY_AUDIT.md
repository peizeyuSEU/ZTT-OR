# Part 5 final consistency audit

- Paper tables and figure data use HIGH_PRECISION_V2 only: PASS.
- V1 retained for audit and excluded from statistics: PASS.
- E0 recomputed in P5-6: false.
- Quota fixed at 14442.842886043889: PASS.
- Original P5-5 result files modified: false.
- Data repository changes are limited to the derived quota artifact and allowed case_config quota metadata: PASS.
- No optimizer, GA, BP, or CPLEX was invoked in P5-6: PASS.
- Canonical seeds: 0km=1010, 5km=202, 10km=808, 15km=707, 20km=808.
- Canonical open DC set: C02;C03;C04;C05;C07;C08 at every distance.

## P5-6.1 correction checks

- Narrative includes the real-case parameterization-versus-model-structure boundary: PASS.
- Narrative includes service-market grouping (13 markets at 0/5 km; 12 at 10/15/20 km): PASS.
- Narrative includes the 10 km mean quota-surplus interpretation: PASS.
- Narrative explains that closed-DC `w_j` values are not operating decisions: PASS.
- Canonical table contains `mean_w_open_dcs` and `demand_weighted_mean_w`: PASS.
- Both canonical columns are read from the exact matching HIGH_PRECISION_V2 `result.csv`: PASS.
- Model-side E0 source files and SHA-256 values are present in the source manifest: PASS.
- Data-artifact `sha256sum -c SHA256SUMS.txt`: PASS.
- No optimization, E0 recomputation, result rewrite, or source/include change occurred: PASS.
