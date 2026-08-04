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
