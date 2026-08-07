# P5-5.1 High-precision intracity distance sensitivity

## Scope and validation

The original V1 directory remains preserved and is marked `ROUNDED_OUTPUT_V1`.
It is not used for final statistics. V2 consists of a fresh 5 × 10 matrix of
GA–BP runs using the unchanged model, data, quota, carbon price, and algorithm
configuration. All 50 runs passed: inner BP `OPTIMAL`, integer feasible,
objective decomposition pass, emission decomposition pass, and carbon identity
pass. The outer result is `HEURISTIC_BEST_FOUND`; no outer global-optimality
claim is made.

Fixed values are `C=14442.842886043889`, `p=2`,
`transport_cost_mode=EXPLICIT_ARC_COST`, `delta=3000`, and `gamma=0.5`.
E0 was not recomputed, and the data repository was not modified.

## High-precision descriptive results

| Intracity distance | Mean profit | Sample SD | Mean total transport cost | Mean total emission | Mean investment cost | Mean carbon trading cost |
|---:|---:|---:|---:|---:|---:|---:|
| 0 km | 175883817.4704982 | 17.8005891 | 81021301.98014057 | 15156.14713105 | 49.17308580 | 1426.60849001 |
| 5 km | 175219913.24325782 | 13.7558017 | 81685207.65935686 | 15270.30547795 | 48.43363238 | 1654.92518381 |
| 10 km | 174587346.5310859 | 15.9048278 | 76840308.99815899 | 14407.40614492 | 44.02797112 | -70.87348224 |
| 15 km | 173966664.16934744 | 14.4423697 | 77460992.24907836 | 14520.48830129 | 43.37539685 | 155.29083050 |
| 20 km | 173345984.22842762 | 15.1682448 | 78081687.39894390 | 14640.05968367 | 27.32355191 | 394.43359525 |

All standard deviations in the high-precision summary use `ddof=1`. The complete
metric table is `P5_FORMAL_RESULTS_SUMMARY_BY_DISTANCE_HIGH_PRECISION.csv`.

## Interpretation

1. Profit decreases as distance increases in the paired means. The approximate
   mean step changes are −0.664 million (0→5 km), −0.633 million (5→10 km),
   −0.621 million (10→15 km), and −0.621 million (15→20 km). These are
   descriptive effects from the fixed instance and ten GA seeds per distance.
2. Supplier–DC and DC–market transport costs are reported separately in the
   high-precision summary. Their combined change is not forced to be strictly
   monotone because the optimized service structure and assignments can change.
3. Total emissions are non-monotone across the five distances. This is a
   network-optimization effect: distance changes alter transport emissions,
   carbon trading, and the optimized `w` decisions simultaneously.
4. The six-open-DC network is stable at every distance (10/10 runs per level).
   Market assignment is stable within each distance; the structured assignment
   changes between the 0/5 km group and the 10/15/20 km group.
5. The optimized reduction-rate vectors are evaluated from V2 full precision.
   The w-stability table should be read together with the discrete network and
   assignment tables; apparent changes are not rounding artifacts.
6. Carbon trading cost follows the emission/quota identity. Negative values mean
   allowance-sale revenue; positive values mean allowance purchase cost.
7. The 0 km case is a deliberate lower-bound scenario for intracity transport
   cost, not an assertion that real shipments have zero physical distance.
8. The 10 km case is the fixed E0-calibrated baseline. The 0/5/15/20 km paired
   differences quantify sensitivity around that baseline and use the same seed
   in each pair.

## Facts, statistics, inference, and limitations

- **Facts:** values in the V2 long table, 50 validation markers, and structured
  network/assignment outputs.
- **Statistics:** means, sample SD, medians, minima, maxima, and paired changes
  computed from the ten seed runs at each distance.
- **Inference:** the non-monotone emission pattern is consistent with joint
  network redesign and reduction-investment decisions; it is not evidence of a
  monotone physical-emission law.
- **Limitation:** GA seeds represent algorithmic randomness for one fixed real
  case, not ten independent real-world systems. The outer result is heuristic.

All final statistics are generated only from `P5_FORMAL_RESULTS_LONG_HIGH_PRECISION.csv`.
