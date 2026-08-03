# P5-2B solver-facing adapter validation

Both 0 km and 10 km dry-runs passed. The C++ entry reads all candidate CSVs, explicitly sorts IDs, constructs the frozen Instance object, applies the eight same-city distance overrides, and stops before GA/BP/CPLEX optimization under --dry-run. Dimensions are 1x8 supplier-DC and 8x15 DC-market; best_w length is 8. The final solver input differs between 0 and 10 km only on the eight same-ID arcs and their derived transport coefficients.
