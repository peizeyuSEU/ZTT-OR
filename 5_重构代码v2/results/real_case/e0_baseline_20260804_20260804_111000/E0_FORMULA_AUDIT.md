# E0 Formula Audit

E0 is defined by the 10 km real-case instance with p=0, temporary C=0, and all w_j=0.

| component | value (tCO2e/year) |
|---|---:|
| facility_emission_tco2e_per_year | 660 |
| inventory_emission_tco2e_per_year | 3.8040093053711064 |
| supplier_dc_transport_emission_tco2e_per_year | 8499.6773003709604 |
| dc_market_transport_emission_tco2e_per_year | 5279.3615763675607 |
| E0 | 14442.842886043889 |
| independent sum | 14442.842886043893 |
| absolute error | 3.637978807091713e-12 |
| tolerance | 1.444284288604389e-06 |

Formula check: E0 = facility + inventory + supplier--DC transport + DC--market transport.
