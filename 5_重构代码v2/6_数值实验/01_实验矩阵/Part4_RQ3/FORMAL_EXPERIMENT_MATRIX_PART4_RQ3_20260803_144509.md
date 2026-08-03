# Part4 / RQ3 formal experiment matrix

Status: FINAL_FROZEN_PENDING_EXECUTION_APPROVAL

This matrix defines 70 formal GA-BP solver runs: 7 carbon-price levels (0, 1, 2, 4, 6, 10, 20) times 10 frozen seeds. The task key is (random_seed, carbon_price).

The five allowance levels (0, 40000, 100000, 200000, 350000) add zero solver runs. After the ten carbon-price=2 baseline runs are complete, 50 allowance observations will be analytically derived using the verified cap-profit identity. They must be labelled analytical derivations, not independent GA-BP runs.

Frozen common settings: size 10x30, delta=3000, gamma=0.5, GA population 30, maximum 50 generations, outer fitness parallelism 8, pricing serial, CPLEX one thread. Formal execution is not started by creation of this matrix and requires user approval.

Screening levels 8, 12 and 16 remain preserved in the completed screening records and are not deleted; they are excluded only from the seven-level formal matrix.
