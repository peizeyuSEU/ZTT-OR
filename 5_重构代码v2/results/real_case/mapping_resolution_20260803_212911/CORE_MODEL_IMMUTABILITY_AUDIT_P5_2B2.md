# P5-2B.2 model extension audit

mathematical_variables_changed=false
mathematical_constraints_changed=false
network_structure_changed=false
ga_algorithm_changed=false
bp_algorithm_changed=false
carbon_emission_formula_changed=false
transport_cost_parameterization_extended=true
objective_coefficient_input_extended=true
legacy_mode_behavior_change=false

Allowed core wiring changes are limited to Instance transport-cost fields, the unified TransportCostFormula accessors, and PricingSolver call sites. No mathematical variable, constraint, network structure, GA, BP, CPLEX, or carbon-emission formula was changed.
