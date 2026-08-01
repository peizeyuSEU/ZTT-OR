# A1 extended formal scale matrix

The unified A1 experiment is **Complete-column CPLEX versus fixed-w
Branch-and-Price**. The original three scales remain unchanged and three
additional scales are added after sequential precheck and three-seed
confirmation.

Formal scales are 3×5, 3×10, 5×10, 5×14, 5×16, and 5×18. Each scale is tested
at fixed (w\in\{0.0,0.4,0.8\}) with the ten frozen seeds. The new extension
therefore contributes 90 paired comparisons and the merged A1 contains 180.

The target 5×18 scale was selected because its confirmation CPLEX end-to-end
times were 34.36–51.90 seconds. 5×14 and 5×16 provide the preceding seconds
and tens-of-seconds levels. 5×12 and 5×20 remain precheck-only evidence and
are excluded from formal statistics.

All formal runs use one controller, serial pricing, and one CPLEX thread.
Timing is measured end-to-end per method, with enumeration, master build,
optimization, validation, BP initialization, BP solve, and BP validation
components retained for audit.
