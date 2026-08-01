# A1 scale-extension precheck

This precheck extends the existing A1 fixed-w validation at `num_dc=5`,
`random_seed=101`, and `uniform_w=0.4` over 12, 14, 16, 18, and 20 retailers.
It is excluded from paper statistics. Each scale is run sequentially with one
controller, serial pricing, and one CPLEX thread. The purpose is to identify a
stable target scale before freezing the extended formal matrix.

The complete-column CPLEX and fixed-w Branch-and-Price runs use the same
generated instance, frozen model parameters, and original-model feasibility
checks. The timing fields use one end-to-end wall-clock measurement per method,
with component timings recorded for diagnosis.
