# Global w-vector schema audit

- generated: 2026-08-02
- solver rerun: false
- A1 affected: false (fixed scalar w, no candidate-vector metrics)
- A2 affected: false (explicit indexed w fields for 5 DCs)
- A3 completed runs audited: 97
- A3 interrupted runs audited: 1
- RQ1 affected runs: 20 (derived normalized outputs only)
- raw solver outputs affected: false
- derived outputs affected: true for old naming/denominator convention; corrected in clean normalized files
- paper conclusion affected: false

## Classification counts

{'NOT_APPLICABLE': 270, 'VALID_EXACT_LENGTH': 127, 'UNRECONSTRUCTABLE': 1}

A3 completed vectors scanned from `best_w` all matched `num_dc`; no trailing-zero truncation was found in the 97 completed runs. The interrupted 30x100 seed=808 is audited only and remains excluded from final A3 statistics.
