# Part4 / RQ3 allowance analytical derivation protocol

No additional GA-BP runs are created for allowance levels. The ten formal carbon-price=2, carbon-cap=100000 results are the solver-generated baseline observations. For each of the ten seeds, five allowance levels (0, 40000, 100000, 200000, 350000) produce 50 analytically derived observations.

The derivation uses E unchanged, investment and network decisions unchanged, e_plus=max(E-C,0), e_minus=max(C-E,0), net carbon trading cost = 2*(E-C), and profit(C)=profit(100000)+2*(C-100000). The 15 screening allowance runs were used to verify this identity before freezing the protocol.

Derived allowance observations must be labelled analytical derivations, never presented as 50 independent solver runs. The paper must distinguish solver-generated results from analytically derived allowance results. If a formal carbon-price=2 baseline violates the verified identities, derivation must stop and the discrepancy must be investigated.
