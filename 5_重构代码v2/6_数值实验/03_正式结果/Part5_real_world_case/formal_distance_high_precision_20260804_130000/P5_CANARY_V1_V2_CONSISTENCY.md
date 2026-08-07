# P5-5.1 canary consistency

The new 10 km / seed 101 run was executed independently in the high-precision
directory. It was not copied from the old attempt_03 directory.

- Network decision: consistent after comparing structured IDs.
- Market assignment: consistent after comparing structured assignments.
- Inner BP status and integer feasibility: consistent (`OPTIMAL`, feasible).
- `best_w`: consistent within high-precision tolerance.
- V2 values rounded to the V1 display precision reproduce the V1 displayed profit and emission.
- Objective decomposition: PASS.
- Emission decomposition: PASS.
- Carbon identity: PASS.

V1 remains preserved for audit only; V2 is the source for all final statistics.
