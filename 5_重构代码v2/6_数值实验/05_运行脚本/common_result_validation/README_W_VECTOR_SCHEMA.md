# Unified w-vector schema

`w_vector_candidate_dc` is always ordered DC 0 through DC `num_dc-1` and must have exactly `num_dc` finite values in the configured range.

Missing tail positions may be reconstructed only when indexed/raw evidence proves they are omitted closed DCs and no nonzero value is lost. Otherwise classify as `UNRECONSTRUCTABLE`. Derived metrics are: `positive_w_count`, `mean_positive_w`, `mean_all_candidate_w` (denominator `num_dc`), `mean_open_dc_w`, `max_w`, and `min_positive_w`.

Run `validate_w_vector.py` after each future Part 3–5 task and before final aggregation.
