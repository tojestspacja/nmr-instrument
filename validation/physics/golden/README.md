# Golden reference data

| File | Provenance | Trusted quantities | Not trusted |
|---|---|---|---|
| `fieldmap-tube.json`, `fieldmap-bottle.json` | legacy `nmr_exact.wls` (Mathematica 13.3, exact single-loop Biot–Savart with elliptic integrals); the Milestone 0 audit recomputed sample voxels independently and matched every printed digit | voxel positions `p` [mm], `df` [Hz], `b1` (B1⊥ per amp) [T/A], `phi` [rad], `B0_centre_mT_at_1.5A`, `centre.b1` | the `spindynamica` block (T2*, flip, FWHM): handedness mismatch in the legacy script, and a record-length-limited FWHM (docs/audit.md) |

`df` is relative to the legacy constants in `legacy-constants.json` (γ̄ = 42.577 MHz/T unshielded and rounded,
f_tx = 89 400 Hz, I = 1.5 A). They are legacy values, kept only to interpret the golden data; the new implementation's
constants live in `design/instrument.yaml`.

Rounding of the stored values (the tolerance floor of any comparison): `df` 1 mHz, `b1` 6 significant figures,
`phi` 1e-4 rad.
