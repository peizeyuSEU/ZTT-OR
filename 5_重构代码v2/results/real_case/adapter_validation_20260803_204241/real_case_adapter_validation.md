# Real-case adapter validation

Five scenarios (0, 5, 10, 15, 20 km) were loaded read-only. Each has 1 supplier, 8 candidate DCs, 15 markets, 8 supplier--DC arcs and 120 DC--market arcs. Exactly eight co-located DC--market arcs change under the scenario distance; all other arcs remain byte-equivalent in source distance. The 0-versus-10 diff contains exactly eight rows.

The source case declares carbon_quota=not_yet_generated. The frozen model has no capacity field or capacity constraint; capacity is NOT_REQUIRED_BY_FROZEN_MODEL. Warehouse area is not throughput capacity. No solver was invoked.
