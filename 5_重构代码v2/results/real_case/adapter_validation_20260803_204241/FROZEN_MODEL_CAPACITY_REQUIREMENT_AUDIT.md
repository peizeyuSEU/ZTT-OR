# Frozen model capacity requirement audit

The frozen model has no DC throughput-capacity parameter, supplier-capacity parameter, or capacity constraint. Assignment feasibility is represented by service-set columns and RMP assignment/opening constraints; assigned demand enters inventory and transport formulas but is not capped.

Evidence at frozen commit 0dd725354732f9b8011e9e7e8540e0e64a3ff223: Instance.h, Config.h, DataGenerator.h, ConstraintsFormula, InventoryCostFormula, ColumnGeneration, BranchAndBound and BranchAndPrice contain no capacity member or capacity row.

Conclusion: a tonnes/year DC capacity field is not required by the frozen model. Warehouse area is not interpreted as throughput capacity; it may remain source metadata for fixed-cost calibration. No supplier capacity constraint exists. Adding capacity would change the mathematical model and is out of scope.
