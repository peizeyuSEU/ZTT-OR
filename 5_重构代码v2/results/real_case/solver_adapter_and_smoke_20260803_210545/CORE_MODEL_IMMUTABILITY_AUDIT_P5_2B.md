# P5-2B core immutability audit

Only new files under apps/real_case and results/real_case were added. Existing objective, constraint, variable-definition, GA, BP, CPLEX and synthetic experiment files are unmodified. core_model_change_count=0. The adapter calls the frozen GeneticAlgorithm/BranchAndPrice classes directly for future smoke mode; it does not reimplement the mathematical model.

Acceptance remains blocked because faithful transport-cost/emission injection is not available without core-model changes.
