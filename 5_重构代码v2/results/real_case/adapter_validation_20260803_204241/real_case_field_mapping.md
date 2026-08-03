# Real-case field mapping

| Concept | Source field | Status | Notes |
|---|---|---|---|
| supplier | nodes.supplier_flag / supplier_dc.supplier_id | DIRECT | one S1 |
| candidate DC | nodes.candidate_dc_flag / dc_params.dc_id | DIRECT | eight C01-C08 |
| market | market_params.market_id | DIRECT | fifteen markets; C01-C08 are co-located market IDs |
| annual demand | market_params.mu_annual_tonnes | DIRECT | tonnes/year |
| daily demand | market_params.mu_daily_tonnes | DIRECT | tonnes/day |
| demand variance | market_params.sigma2_daily | DIRECT | tonnes^2/day^2 |
| reserve price | market_params.v_i_baseline | DIRECT | CNY/tonne |
| DC fixed cost | dc_params.f_j_cny_per_year | DIRECT | CNY/year |
| replenishment/order cost | inventory_params.F_j_cny_per_order, g_j_cny_per_order | DIRECT | CNY/order |
| holding cost | inventory_params.h_cny_per_tonne_year | DIRECT | CNY/(tonne-year) |
| lead time | inventory_params.L_j_days | DIRECT | days |
| supplier/DC distance | supplier_dc.road_distance_km | DIRECT | km; not altered by intracity scenario |
| DC/market distance | dc_market.road_distance_km | DIRECT | km; only eight co-located arcs altered |
| transport rate | case_config.transport_rate_cny_per_tonne_km | DIRECT | CNY/(tonne-km) |
| transport emissions | case_config.transport_emission_factor_tco2e_per_tonne_km | DIRECT | tCO2e/(tonne-km) |
| facility emissions | dc_emission_params.hat_f_j_tco2e_per_year | DIRECT | tCO2e/year |
| inventory emissions | dc_emission_params.hat_h_tco2e_per_tonne_year | DIRECT | tCO2e/(tonne-year) |
| capacity | -- | NOT_AVAILABLE | no capacity field in candidate data; must be resolved before P5-3 |
| carbon quota | case_config.carbon_quota | PLAN_ONLY | not_yet_generated; no quota generated here |
| delta/gamma/w | frozen model Config | PLAN_ONLY | model parameters, not data fields; no solve here |
