# Transport cost compatibility audit

Frozen DC-market cost reads dist[i][j] and applies 10/20/30/40 CNY per tonne bands for distances up to 100/300/600/1000 km. Supplier-DC uses 5/10/15/20. Direct mode returns raw distance as CNY per tonne and does not multiply by a fixed rate. No formula reads transport_cost_baseline and no per-arc cost matrix exists.

The source cost is 0.39 times physical distance. It cannot be injected without changing the frozen model: direct mode is not 0.39d, band mode is not 0.39d, and replacing distance corrupts emissions. Conclusion: FROZEN_MODEL_TRANSPORT_COST_INCOMPATIBLE.