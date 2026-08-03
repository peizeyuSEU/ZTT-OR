# Part 5 real-case experiment plan

This plan freezes the real-case design and records the P5-1/P5-2 scope. The
current stage is validation-only: no carbon quota is generated and no
GA--BP/CPLEX solve is run.

- Data: YRD Steel v0.2.0 candidate, fixed data commit
  b9fc758dbe11762882bf66cbbc7672da0918adb7.
- Model: frozen commit 0dd725354732f9b8011e9e7e8540e0e64a3ff223,
  tag paper-exp-baseline-20260801.
- Structure: one supplier, eight candidate DCs, fifteen markets, and an
  eight-bit chromosome for future GA--BP runs.
- P5-1 freezes the experiment plan and mapping decisions.
- P5-2 validates read-only loading and the five intracity-distance scenarios
  (0, 5, 10, 15, and 20 km). Only the eight explicitly identified
  same-city DC--market arcs may change.

Future E0 and formal runs are recorded as a plan only. E0 uses 10 km,
carbon price 0, temporary quota 0, and fixed w=0. Formal runs use carbon
price 2, quota derived from E0, optimized abatement, five distance levels,
and ten seeds. No solver is invoked in P5-1/P5-2.
