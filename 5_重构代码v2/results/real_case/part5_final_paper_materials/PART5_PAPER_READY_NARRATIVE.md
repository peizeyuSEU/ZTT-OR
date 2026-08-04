# Part 5 paper-ready narrative

## Real case and parameterization
The case contains one supplier, eight candidate distribution centers, and fifteen markets. Demand, distances, fixed costs, and emission parameters are taken from the frozen dataset branch. The real-case transport cost uses the explicit linear rate 0.39 CNY/(tonne*km); the original stochastic experiments remain unchanged.

## E0 and quota
At 10 km, carbon price zero, temporary quota zero, all abatement rates fixed to zero, and direct Branch-and-Price, the validated baseline emission is E0=14442.842886043889 tCO2e/year. The formal baseline sets C=E0 and p=2 CNY/tCO2e. The quota multipliers are analytically derived research scenarios, not external or government quota observations.

## Formal design and findings
The distance levels are 0, 5, 10, 15, and 20 km with ten GA seeds per level, for 50 GA–BP runs. All inner BP solves are OPTIMAL; the outer results are HEURISTIC_BEST_FOUND. Mean profit decreases from 175883817.4704982 at 0 km to 173345984.22842762 at 20 km. Relative to 10 km, mean paired profit differences are +1296470.9394122988 (0 km), +632566.7121719122 (5 km), -620682.3617384821 (15 km), and -1241362.3026582957 (20 km).

The six-open-DC network is stable at all distances. Market assignment is stable within each distance but changes between the 0/5 km and 10/15/20 km groups. Emissions are non-monotone because transport, assignment, network selection, and abatement investment are optimized jointly.

## Limitations
GA seeds represent algorithmic randomness for one fixed case, not independent real-world systems. The outer GA result is not claimed globally optimal. The 0 km level is a lower-bound sensitivity scenario rather than a literal zero-distance shipment claim.

## Supplementary model-structure clarification

Part 5 adds an explicit linear intracity transport-cost parameterization for the real-case adapter. This is an input-side parameterization extension only: it does not change the mathematical decision variables, constraints, network structure, GA logic, BP logic, or carbon-emission formulas. The original stochastic experiments continue to use the frozen tiered transport-cost interface, while the real case uses the linear rate 0.39 CNY/(tonne·km).

## Market service structure

The optimized service structure serves an average of 13 markets at 0 km and 5 km, and 12 markets at 10 km, 15 km, and 20 km. The change between 5 km and 10 km is an optimization outcome of this case study; it should not be interpreted as a causal real-world market response. Accordingly, transport cost and total emissions need not be strictly monotone in distance.

## 10 km quota interpretation

At 10 km, mean total emissions are 14407.406144923654 tCO2e/year against the fixed quota 14442.842886043889 tCO2e/year. The mean net carbon trading cost is -70.87348224047128 CNY/year. Under the model sign convention, this negative value indicates average allowance-sale revenue from surplus allowances; it is not a government subsidy or an externally observed carbon-market revenue.

## Interpretation of w at closed DCs

The complete eight-component `best_w` vector is retained for reproducibility. A `w_j` value at a closed DC does not represent an actual operating or abatement investment decision. Interpretive summaries should focus on `mean_w_open_dcs` and `demand_weighted_mean_w`, together with the open-DC set.
