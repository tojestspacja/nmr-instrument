# Design closure — from a plausible model to a buildable low-field NMR instrument

Engineering-closure working document. Phases 1–2 (audit + physics closure) are **done and quantified below**; Phases
3–10 are staged with what can be done in CAD/analysis vs what needs physical hardware. Numbers come from the shared
field solver (`physics/`, via the WASM core over the real voxelised sample), not assumptions. Frame: **+x** sample /
RF-solenoid axis, **+y** Helmholtz / B0 axis, **+z** up.

Source-of-truth discipline (§21): the real PCB + firmware stay authoritative in **class-board-2026** (pin a commit for a
physical build); mechanical/instrument/docs work lives here in **nmr-instrument**. No firmware/PCB is duplicated.

---

## Phase 1 — Current-state audit (gap list)

| Item | State | Gap |
|---|---|---|
| RF former, sample sleeve, cradle | CAD + STL (`legacy/mechanical/probe.scad`) | sleeve/cap is the axial stop but the **datum chain to the B0 centre is not explicit** |
| Helmholtz ring formers + spacing rails/brace | CAD (`probe.scad` b0_ring ref; `cut-probe.scad` B0 rails) | **no coaxiality/alignment datum**, no mount to a common base |
| Housing for the boards | CAD (`housing.scad`, `cut-housing.scad`) | a **separate enclosure**, not integrated with probe + magnet |
| B0 windings, PCB models, cables | **presentation only** (sim meshes) | not fabrication geometry; cables are decorative |
| Base / frame tying magnet + probe + electronics | **absent** | the three subsystems have no common mechanical reference |
| Electronics tray / standoffs integrated with the instrument | **absent** | boards sit in their own housing beside the experiment |
| Cable routing / strain relief as real features | minimal (probe stub tie, anchor) | no routed paths/clips between subsystems |
| Active NMR volume (ROI) definition | **absent** | now defined below |
| Shim hardware | none (Level 0) | decision pending measurement |

## Phase 2 — Physics closure (computed over the real 50 mL tube, I = 1.5 A, γ̄ = 42.576 MHz/T, 2 mm grid, 7924 voxels)

**B0 over the sample:** f_mean **89 528 Hz**; peak-to-peak **304 Hz (3400 ppm)**; RMS 58 Hz. Receive-weighted
(b1⊥²·V) RMS **35 Hz (388 ppm)**. The simulator's "6 Hz FWHM" is the cusp of a line with a long tail — the bulk of the
signal is spread over tens of Hz.

**B0 along the coil axis x (transverse to B0):** flat to within +17…+31 Hz for |x| ≲ 30 mm, then falls hard toward the
ends: −24 Hz at ±40 mm, **−116 Hz at −50 mm, −204 Hz at −60 mm**. The long sample's ends sit where the Helmholtz field
has dropped — they contribute broad, off-resonance signal.

**The central result — active length vs linewidth (receive-weighted):**

| Active length (x) | rel. received signal | f RMS | p2p | signal within ±5 Hz |
|---|---|---|---|---|
| **100 mm (current)** | 98 % | **23.9 Hz** | 172 Hz | **11 %** |
| 80 mm | 91 % | 11.3 Hz | 77 Hz | 57 % |
| **60 mm** | **78 %** | **4.4 Hz** | 33 Hz | **82 %** |
| 40 mm | 55 % | 1.6 Hz | 13 Hz | 99 % |
| 30 mm | 42 % | 0.9 Hz | 8 Hz | 100 % |

### Finding (§3, §4): the 100 mm RF coil / 50 mL sample is too long for the B0-homogeneous volume

The active sample volume extends well beyond the homogeneous region, so ~90 % of the received signal lies outside ±5 Hz
of the line. Restricting the **active region to ~60 mm** keeps **78 % of the signal** while cutting the field RMS **≈5×**
(24 → 4.4 Hz) — a large linewidth/T2* win for a modest signal loss. 40 mm gives a near-ideal line (1.6 Hz) at ~55 %
signal. The **active NMR ROI** is therefore the central cylinder **|x| ≲ 30 mm** (not the full 100 mm), intersected with
the RF-sensitive and B0-homogeneous volumes.

### V1 vs V2 decision (§2, §8): keep V1; do not rotate to vertical

The horizontal simple solenoid is physically correct (B1 ≈ +x ⟂ B0 = +y), has an excellent filling factor and B1/I, and
the measured deficiency is **length**, not orientation. The fix is a **V1 refinement — shorten the active region to
~60 mm** (shorter winding, or a defined ROI with the sample centred), *not* a vertical top-loading V2. Rotating the
sample ∥ B0 would force B1 ∥ B0 (invalid) and require a transverse (saddle/cos-θ) coil — a separate study only, **not
justified by this data**. V1 stays the baseline.

### Phase 3 implemented — 60 mm coil adopted (2026-10-03)

Done as a coupled change through the single source of truth, verified green:
- `design/instrument.yaml`: `rf_coil` → length 0.060 m, 266 turns (2×133), B1/I **4.611195e-3 T/A** (Biot–Savart solver),
  L ≈ 1.87 mH, R ≈ 4.46 Ω (estimates, to be measured); `gen_config` recomputed the chain and **t90 → 369 µs**,
  coil current 6.9 mA, and passed all consistency checks. Config sha updated.
- **Validation frozen at 100 mm:** added `legacy_probe()` + ABI `nmr_legacy_fields_at`; `test_field` and `test_golden`
  now cross-check the solver against the Mathematica golden maps at the *frozen 100 mm* reference, independent of the
  design coil. So the solver stays independently validated; the 60 mm field is computed by that validated solver.
- Mechanical: `probe.scad` → 60 mm/266-turn winding (former 85 mm; the 50 mL tube overhangs 16 mm, datum = cap at the
  sleeve flange; the sample covers the winding z 6–66 mm). Fabrication STLs + sim meshes regenerated; probe render
  inspected (`tools/nmr-render` probe preset).
- Tests: `ctest` 7/7, `pytest` 16/16 (golden on frozen 100 mm; parity on 60 mm config), OpenSCAD renders manifold.
- Still to recalibrate on the bench (§16): t90 by nutation, and L/R/Q with the real matching network — the config
  values are the best estimates, flagged accordingly.

### Shim decision (§5): defer to measurement; expect Level 0–1

At a 60 mm active region the Helmholtz pair alone gives ~4 Hz RMS, comparable to the intrinsic ~2 Hz water line. That is
likely enough for a first FID/spectrum. Start **Level 0** (geometry + reproducible positioning); add a **Level 1** linear
shim only if the *mapped* field / *measured* linewidth demand it. Do not add higher-order shims pre-emptively.

---

## Phases 3–10 — staged plan

### Phase 4 implemented — magnet structural closure + baseplate (2026-10-03)

The B0 pair already had laser-cut rails + braces (`cut-probe.scad`) that seat both ring stacks at the 200 mm spacing,
but they were never in the 3-D assembly and there was no common base. Added (faithful solids from the same rail/ring
numbers, in `cut-probe.scad`): `b0_rail3d`/`b0_brace3d` (the two rails at x = ±100 seating the rings, tied by braces)
and a `baseplate()` that carries both rails **and** the probe tripod — one mechanical reference (x coil axis, y B0,
z up). Exposed as render components (`exp-probe.scad` → sim meshes) and on the website (new "Frame" group); the renderer
`instrument`/`exploded` presets show the supported instrument on its base, verified by inspection. Clearance echo
confirms the baseplate spans both rails + the probe. Still open for Phase 6: a full `instrument()` interference pass and
the electronics tray's mount to this baseplate (Phase 5).

**Doable now in CAD/analysis (with the `tools/nmr-render` oracle for visual checks):**

- **Phase 3 — Probe mechanical closure.** Make sample positioning a real datum chain: sleeve flange + tube cap as the
  axial hard stop referenced to the winding centre (x = 0), radial centring in the bore, and a defined ROI label. If we
  adopt the 60 mm active region, add a shorter-winding variant parameter (`wind_l`) and re-verify B1/I, L, Q, t90.
- **Phase 4 — Magnet structural closure.** Coaxiality + spacing datums for the Helmholtz pair (rails already fix
  spacing; add alignment features) and a mount to a common base.
- **Phase 5 — Electronics integration.** Real board dimensions (from `class-board-2026/hardware`), a tray + standoffs,
  connector access, strain relief.
- **Phase 6 — Full-instrument assembly.** One OpenSCAD `instrument()` of assemblies (base / helmholtz / probe /
  electronics / cable) with interference + serviceability checks, rendered and inspected.
- **Phase 7 — Fabrication outputs + BOM + assembly order.** Regenerate STL/DXF from the closed design.

**Requires physical hardware / measurement (procedures authored here; cannot be executed in this environment):**

- **Phase 8 — RF/electrical validation:** measure L/R, tune/match near the Larmor line, verify TX current, **recalibrate
  t90** (the 417–458 µs value is invalid after any coil change), measure ring-down/recovery.
- **Phase 9 — Commissioning:** energise B0, measure/ map the field around the ROI, compare to this simulation, acquire
  the first FID, check I/Q polarity, average, measure linewidth/T2*.
- **Phase 10 — Shim decision:** from the measured map/linewidth, add only justified shim DOF.

### Commissioning sequence (§18) — mechanical → B0 → RF → NMR → shim
Assemble frame → mount + align Helmholtz (verify R, spacing, coaxial) → mount cradle, insert probe, confirm ROI at
centre → mount electronics, route cables → measure coil R, energise B0 safely, measure central B0, **map B0 over the
ROI and compare to this doc** → measure RF L/R, tune/match, verify TX, calibrate t90, measure ring-down → first FID,
check I/Q, set gain, average, measure linewidth → shim only if the data demand it, then re-verify sample-insertion
repeatability.

### Acceptance criteria (§19)
- *Mechanical:* Helmholtz spacing/alignment repeatable; probe constrained; sample inserts/removes without disturbing
  alignment and returns to the ROI; PCB mounted; cables strain-relieved.
- *Electrical:* B0 coils at current without excess heating; RF tunes near Larmor; TX reaches amplitude; receiver
  recovers; ADC acquires.
- *Magnetic:* measured B0 ≈ predicted; ROI homogeneity quantified; map vs simulation.
- *NMR:* a real sample gives a reproducible FID, coherent phase, averaging gain, a spectrum consistent with the measured
  B0, and a **measured** linewidth. Keep "simulated" and "measured" spectra clearly distinct throughout.

## Remaining uncertainties needing physical measurement (§23, §27)
Actual B0 vs predicted; mapped ROI homogeneity vs the table above; B1/I (→ t90) after any coil change; coil L/Q with the
real matching network + coax; TX/RX isolation and receiver recovery on one shared coil (the open blocker — see the
simulator's "TX and RX on one coil" check); sample-insertion repeatability; whether the 60 mm active region is adopted.
