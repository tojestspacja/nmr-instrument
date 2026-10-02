# Historical Physics Agent Handback

> Evidence class: TRANSCRIPT_REPORTED. The previous agent's report follows verbatim, decoded from the uploaded session record. It was not rerun or independently certified during handoff preparation. Estimates, uncertainties and unresolved issues remain in force; later source evidence may supersede it.

Source: `Pasted text.txt`, original line 4398. SHA-256: `c4f56a1a92747fb1d7b77dd57802df4431ef472958be1cf2dc8c7db71106579d`.

---

## NMR physics audit: nmr-instrument @0914edb and class-board-2026 w1-yi-tsai @da56d2b (read-only)

**Main findings**
- The field maps are sound and can serve as golden data. I re-ran the Biot–Savart part of `nmr_exact.wls` at 5 voxels and it reproduces `fieldmap-tube.json` (df, b1, phi) to every printed digit.
- `physics.js` reproduces its README table exactly. Its Bloch rotation and precession sign are correct (checked against an RK4 integration of dM/dt = γM×B).
- **The reference script `nmr_exact.wls` has a handedness bug.** The SpinDynamica pulse rotates about +H, but the free precession afterwards uses e^{-iΩt}. These are opposite senses.
  - Evidence: if I flip `physics.js`'s pulse axis to match SpinDynamica, its T2* moves from 79.88 to exactly 80.52 ms (tube) and from 40.96 to exactly 41.52 ms (bottle). Those are the SpinDynamica numbers.
  - So the 0.6 ms "agreement" gap in the README is this convention mismatch, not numerical error. `physics.js` is the self-consistent one.
- **The "FWHM 6 Hz" linewidth is not physical.** It is the spectral width set by the 0.2 s record (rectangular window, 1.207/T). With the exact tube map, record lengths of 0.1 / 0.2 / 0.4 / 0.8 s give 12 / 6 / 3 / 1 Hz. Do not use it as golden data.
- **Three models put the line at different frequencies for the same 1.5 A:**

| Model | Line at 1.5 A |
|---|---|
| `physics.js` approximate fields (B0nom = 89400/γ̄ fixed, `physics.js:20`) | 89 400 Hz |
| Mathematica, finite cross-section winding (2.10347 mT) | 89 560 Hz |
| `spectrum.py`, thin-wire Helmholtz with N = 312 (2.10407 mT, `spectrum.py:74-75`) | 89 585 Hz |

  - The README's "thin-wire formula 2.0997 mT" (`README.md:48`, `nmr_exact.wls:67`) is wrong. 2.0997 mT is the design target before N was rounded up from 311.36 to 312. Thin-wire with 312 turns gives 2.10407 mT.
  - The `probe.scad:296` echo prints the target field, not the field the coils produce.
  - The "retune to the tank" current (`instrument.scad:107`; the `test-physics` "1.484 A" case) assumes the line is at 89 400 Hz. With the exact field it should be about 1.5×88469/89560 ≈ **1.4817 A** (estimate).
- **The four models do not agree on the signal chain:**

| Model | Signal | T2* | Gain | Noise |
|---|---|---|---|---|
| Firmware SIM | "20 µV at coil", 18 mV at ADC | 0.4 s | ×900 | 3 mV uniform |
| `physics.js` | 0.90 µV emf, 8.8 µV at tank, about 0.2 V at ADC | 72–80 ms | ×25 500 | 49 mV rms |
| `spectrum.py` | firmware SIM constants (0.018 / 0.003 / 0.002 / DC) | intrinsic T2 = 2 s, plus 4th-order field spread | — | — |

- **`spectrum.py` uses a different sample model:**
  - It uses r = 15 mm, the tube's outer radius (`spectrum.py:87`); `physics.js` and Mathematica use 14 mm.
  - It keeps only x from −50 to +25 mm, the water inside the winding. The others use −58 to +45 mm.
  - So its field spread is 193 Hz, against 298 Hz in Mathematica.
  - Its "decay" T2* (1.03 s, from sliding-window peak height) is a different quantity from `physics.js`'s 1/e envelope T2*.
- **`nmr_exact.wls` cannot run in full here.** SpinDynamica is not installed (no `SPINDYNAMICA` variable, no `sd371` folder, none under the user profile or the Wolfram install). The Biot–Savart part runs at about 0.03 s per voxel, so a full field map takes about 2 min (estimate). The README says about 5 min per run including SpinDynamica.

## 1. The models

**`simulator/physics.js`** (SI units; M in A/m; emf in V)
- **Field, approximate mode:** Helmholtz 4th-order term −1.152(y⁴−3y²ρ²+⅜ρ⁴)/R⁴, with y along the pair axis (L56-62). Earth's field is added along B0 and in quadrature across it.
- **Field, exact mode:** from the maps, B = (89400+df)/γ̄·(I/1.5) + B_earth (L96-98).
- **B1, approximate mode:** on-axis finite solenoid μ0N/L·½[(x+h)/√… − (x−h)/√…] (L32-35, L101). It ignores the radial variation and the real two-layer winding, which is why it gives t90 = 451 µs.
- **Pulse:** the rotating component is half the linear field (L104). The rotation is exact Rodrigues about −B_eff in the f_tx frame, including the B1 direction φ (L64-70, L103-108).
- **Coil current:** iTx = 7.95/|6.7 + jωL| (L91). It ignores R813 (4.7 Ω) and the tank capacitor on the shared coil.
- **Magnetization:** Curie law M0 = Nγ²ħ²B/(4kT) (L107).
- **Pickup:** reciprocity, emf = ω·B1⊥/I·M⊥·dV (L120). It uses ω_tx rather than the local Larmor frequency (about 0.2 % error).
- **Free precession:** M⊥·e^{−iΔωt}·e^{−t/T2}, with received phase e^{−iφ} (L122-131).
- **Relaxation:** T1 recovery appears only in `state()` (L196). Averaging uses a steady-state Mz from the centre voxel's flip, (1−E1)/(1−E1cosθ) (L111-114).
- **Tank:** a scalar Lorentzian gain Q/√(1+(2QΔf/f0)²) at the line frequency (L136). No phase shift and no noise shaping.
- **Detection:** the signal is conjugated so that a line above f_tx lands above the 5.4 kHz IF; I = Re, Q = Im of e^{+i2πf_IF t} (L142-143).
- **Noise and blanking:** Gaussian, σ = 15.6 nV/√Hz·√15 kHz/√n per channel (L137). That is about 10 % above en·√(12.5 kHz) at 25 kS/s complex (estimate; needs checking). The first 1.2 ms are zeroed (L145).
- **Line position:** phase slope over the first 1 ms (L177-185).
- **Linewidth and spectrum:** naive DFTs (L156-160, L203-216).
- **Not modelled:** spin echo, pulse ringdown, the tank's transient.

**`mechanical/spectrum.py`**
- Relative field offsets from the 4th-order Helmholtz term over a uniform disk sample (L89-100).
- Signal amplitude scales as f² times the tank response (L133). `f_tank` defaults to f_tx, not 88 469 Hz (L70).
- Lines are placed on 0.5 Hz FFT bins, then shifted by the t0 dead time and multiplied by e^{−t/T2} (L106-120).
- Hum, noise and DC offsets are the firmware SIM constants (L118-120).
- Voltage-drive heating: dT += I²R0(1+αdT)·t_rep/(m·c), with I = I_set/(1+αdT) (L123-136). This is self-consistent.
- Its own `spectrum()` uses a half-Hann window with truncation (L146-163).
- It reads `.scad` constants with a regex (L49-55).

**`simulator/build/nmr_exact.wls`**
- Exact single-loop Biot–Savart using elliptic integrals (L37-43).
- B0 pair: 5×5 filaments per coil, 312 turns, R = 200 mm, cross-section 28×27 mm (L46-47).
- Class coil: 222 turns at r = 20.225 mm plus 178 turns at r = 20.675 mm, 0.45 mm pitch, from x = −50 mm (L50-51).
- B1⊥ and φ in a frame built from the local B0 (L70-76).
- SpinDynamica pulse (L80-82), then FID with e^{−iΩt} (L97). This is the handedness inconsistency described above.
- Same tank and FWHM definitions as `physics.js` (L101-107).
- Voxel grid starts at x0+g/2 (L59), so it is off-centre: tube y and z run −12.75 to +12.25 mm; bottle y and z run −16 to +17 mm. As a result, tube volume is 55.55 mL against 55.88 analytic, and bottle is 121.77 mL against 119.3 (+2 %).

**Firmware SIM (`Sequencer.cpp:51-63`, `:611-685`)**
- A single complex exponential at the IF (sim_larmor − f_lo) with the scan phase, a fixed 18 mV amplitude, and e^{−t/0.4 s} starting at the first sample.
- Uniform noise of 3 mV rms from an LCG, 2 mV of 50 Hz hum as a complex tone, and DC offsets.
- Generated at the raw 100 kS/s and boxcar-decimated by 4 (`Sequencer.h:45-46`).
- It ignores t90, flip angle, echo (`:436-451` waits but simulates nothing), field and tank.
- The I/Q convention matches everywhere else: Larmor = f_lo + peak (`NmrBlock.cpp:286`, `instrument.py:290`).

**I/Q sign:** all four models agree that a positive IF offset means a line above f_lo. None of them has been checked against the real mixer wiring.

## 2. Analytic checks
- **`test-physics.js`:** has no assertions; it only prints. In Node the field maps are never loaded, so it runs the approximate model. Default tube: emf 0.83 µV, t90 451 µs, T2* 72.4 ms, FWHM 6 Hz, about 0.4–0.6 s per case, 3.6 s in total.
- **Helmholtz 4th order (numeric loop integration):**

| Offset | Axial error | 1.152(s/R)⁴ | Radial error | ⅜·1.152(s/R)⁴ |
|---|---|---|---|---|
| 10 mm | −7.18e-6 | −7.20e-6 | −2.706e-6 | −2.700e-6 |
| 30 mm | −5.69e-4 | −5.83e-4 | −2.23e-4 | −2.19e-4 |

  The coefficients are correct. The 6th-order term adds about 2.5 % at 30 mm.
- **Finite solenoid on axis:** numeric 4.65246e-3 T/A against analytic 4.65245e-3 at x = 0; it also matches at x = 40 and −55 mm.
- **Two-layer coil centre:** my independent loop sum gives 5.029625e-3 T/A, identical to the map's `centre.b1`.
- **Coil current and t90:** iTx = 5.59402 mA gives t90 = 417.38 µs. The long-solenoid formula gives 417.64 µs, so the workbook's 417 is the infinite-solenoid number and matches the exact value partly by coincidence. The approximate model gives 451.22 µs.
- **Curie law:** M0 = 6.909e-6 A/m at 2.0997 mT and 293 K. The formula and N_H = 6.69e28 m⁻³ are correct.
- **Bloch rotation:** Rodrigues and RK4 agree to 5 decimals at Δf = 0, +160 Hz, and −300 Hz with φ = 0.4. Example: Δf = 160 Hz, 417.38 µs, B1rot = 14.068 µT gives (0.26306, 0.96466, 0.01522).
- **Precession sign:** at 100 Hz for 1 ms, M⊥ goes to e^{−i0.2π}, which is clockwise and correct for γ > 0.
- **Hahn echo (90x–τ–180y–τ, Δf spread ±150 Hz):** hard pulses refocus to 1.0000. The real 14.07 µT B1 gives 0.968 at the echo (0.990 right after the 90°). This is my script, not a project feature.
- **Relaxation:** T2 decay and the T1 recovery formula in `state()` are correct.

## 3. Field maps
- **Format:** JSON with `source`, `sample`, `grid_mm` (2.5 for the tube, 3 for the bottle), `B0_centre_mT_at_1.5A` (2.10347), `centre.b1` (T/A), and per voxel:
  - `p`: position in mm, coil frame (x along the coil axis, y along B0);
  - `df`: Hz offset from 89 400 at 1.5 A, no Earth field, rounded to 1 mHz;
  - `b1`: coil field per amp perpendicular to the local B0, 6 significant figures;
  - `phi`: direction in rad, rounded to 1e-4.
- **Size:** 3555 voxels (tube) and 4510 (bottle). An `spindynamica` summary block is included.
- **Ranges:** tube df −128.9 to +169.2 Hz, b1 1.56 to 5.11 mT/A, φ ±0.47 rad.
- **How made:** `nmr_exact.wls`, then `compact.py`. Note that `compact.py:7` hardcodes iTx.
- **Trust:** the Biot–Savart values are reproducible and correct. They are suitable as golden data for B0 and B1 at fixed points. Caveats:
  - asymmetric, coarse grid;
  - idealised winding (uniform 5×5 filaments; layer 2 from −50 to +30 mm per the winding note);
  - the SpinDynamica summary fields T2star, flip and fwhm are not golden (see above).

## 4. Constants (value, file:line)

| Quantity | Values found | Status |
|---|---|---|
| γ̄ | 42.577e6 Hz/T: `nmr-params.scad:9`, `physics.js:12`, `wls:28` | consistent |
| f_tx / f_lo | 89400 / 84000: `nmr-params:6-7`, `Sequencer.h:35-36`, `instrument.py:230`, `wls:30` | consistent |
| B0 at 1.5 A | 2.0997 mT target (`physics.js:20`, `nmr-params:10`); 2.10407 thin-wire N = 312 (`spectrum.py:75`); 2.10347 exact (map) | **3 values** |
| B0 pair | R = 200 mm, I = 1.5 A, N = 312 (computed, `probe.scad:224-232`), 28×27 mm (`probe.scad:228,233`; `wls:33`) | consistent; 10.33 Ω, 9.2 kg Cu |
| Coil | 400 turns, 100 mm, wire 0.45 mm, former 40 mm (`probe.scad:48-52`); mean radius 20.45 mm (`physics.js:18`) vs 20.225/20.675 mm (`wls:50`) | consistent (mean vs layers) |
| L, R, Q | 2.53 mH, 6.7 Ω, 10: `nmr-params:13-15`, `physics.js:18`, `wls:31` | consistent |
| TX drive | 7.95 V pk → 5.594 mA | consistent |
| Tank C / fTank | 1.2527 nF needed (`nmr-params:16`); fTank 88 469 Hz (`physics.js:21`, `wls:30`, computed in `instrument.scad:99`); `spectrum.py` defaults to 89 400 | **disagree** |
| t90 | 417 µs (`nmr-params:26`, `Sequencer.h:38`, `wls:30`); 451 µs from `physics.js` approximate mode; t180 834 µs (`Sequencer.h:39`) | approximate mode is wrong |
| Dead time / acquisition start | 1.0 / 1.2 ms (`Sequencer.h:42-43`); 1.2 ms used as blanking (`physics.js:22`, `spectrum.py` t0) | minor |
| Record length | 2 s (`nmr-params:28`, `Sequencer.h:44`); 0.2 s in `physics.js:22`; 5000 samples in `wls:93` | disagree; this is what sets the 6 Hz FWHM |
| Sample rate | 100 kS/s raw ÷ 4 = 25 kS/s | consistent |
| T1 / T2 / TR | 2.5 / 2.0 / 3.0 s | consistent |
| Firmware SIM T2* | 0.4 s (`Sequencer.cpp:58`) vs 80 ms modelled | **disagree** |
| Noise / gain | 15.6 nV/√Hz, 15 kHz, ×25 500 (`physics.js:24-25`); 3 mV and ×900 (`Sequencer.cpp:52-59`) | **disagree** |
| Sample geometry | r 14 mm, x −58..+25 mm plus 20 mm cone (`physics.js:39`, `wls:56`); r 15 mm, x −50..+25 mm (`spectrum.py:86-87`) | **disagree** |
| Copper constants | α = 0.00393 /K, c = 385 J/(kg·K) | consistent |

## 5. Performance
Node timings with the exact maps:
- `setup()` takes 160–180 ms. The hot loop is voxels × samples (3555 × 5000, `physics.js:122-131`).
- The linewidth DFT is 801 frequencies × 2500 samples (L156-160).
- `spectrum()` is a naive DFT, 400 frequencies × 4970 samples, about 80 ms (L203-216).
- `state()` takes about 0.5 ms.
- Cost scales linearly with record length: 567 ms at 0.8 s, so roughly 1.5–2 s at the firmware's 2 s record (estimate).
- `spectrum.py` runs in 1.7 s for a sweep; `test-physics.js` in 3.6 s total.

## 6. Classification

| Feature | Verdict | Why |
|---|---|---|
| Rodrigues rotation about −B_eff, precession e^{−iΔωt} | RETAIN | Matches the ODE; correct handedness |
| Curie M0, reciprocity emf, rotating B1 = ½ linear | RETAIN | Analytically correct |
| Exact Biot–Savart field maps / elliptic loop kernel | RETAIN (port the kernel to C++) | Reproduced exactly; fast |
| Approximate 4th-order Helmholtz and on-axis solenoid | REIMPLEMENT | B0nom ignores N = 312; B1 has no radial or two-layer model (451 vs 417 µs) |
| Exact-mode voxel grid | REIMPLEMENT | Off-centre grid, ±2 % volume |
| Scalar tank gain, no transient | REIMPLEMENT | Needs a complex response; noise not shaped |
| Noise σ = en√bw | VERIFY | Bandwidth and decimation inconsistent (about 10 %) |
| I/Q convention | VERIFY | Consistent in code, not checked against hardware |
| Steady-state averaging from the centre voxel | VERIFY | Approximation; not SSFP-correct with T2 ≈ TR |
| `nmr_exact.wls` SpinDynamica comparison | REIMPLEMENT | Handedness mismatch |
| FWHM / "linewidth" output | REMOVE or redefine | Window-limited artifact |
| Firmware SIM constants | REIMPLEMENT | Should come from the shared core (no echo, wrong T2*, wrong amplitude) |
| `spectrum.py` physics layer | REIMPLEMENT on the shared core | Third line position and different sample model; keep the print/heating logic |
| Coil-heating drift | RETAIN | Self-consistent |
| `test-physics.js` | REIMPLEMENT as asserting tests | Currently print-only |

**Golden reference values (provenance in brackets):**
- **Analytic:**
  - Helmholtz thin-wire, N = 312, I = 1.5 A, R = 0.2 m: 2.10407 mT.
  - 4th-order coefficients 1.152 and 0.432.
  - Finite solenoid, 400 turns, L = 0.1 m, a = 20.45 mm: 4.65245e-3 T/A at the centre.
  - M0 = 6.9094e-6 A/m (N = 6.69e28 m⁻³, 2.09973 mT, 293 K).
  - iTx = 5.59402 mA.
- **Biot–Savart (Mathematica, independently confirmed):**
  - B0 centre 2.1034711 mT at 1.5 A.
  - B1 centre 5.029625e-3 T/A, giving t90 = 417.38 µs and B1rot = 14.068 µT.
  - Voxel spot checks, as (position mm, df Hz, b1 T/A, φ rad): (−56.75, −12.75, −5.25) → 10.4631, 1.558114e-3, −0.20769; (5.75, −0.25, 4.75) → 159.5552, 4.966064e-3, −0.00833.
- **Bloch:** the RK4 vectors above; Hahn refocusing 1.0000 with hard pulses.
- **`physics.js`, exact maps, 89.4 kHz, 417 µs, seed 3:**

| Sample | emf (µV) | Tank (µV) | T2* (ms) | Line (Hz) |
|---|---|---|---|---|
| Tube | 0.90215 | 8.766 | 79.88 | 89 559.5 |
| Bottle | 1.64262 | 15.96 | 40.96 | 89 559.2 |

  These are trustworthy for regression after the convention fix. Do not use the FWHM, the SpinDynamica T2* values, the SNR or the firmware SIM amplitudes as golden data.

**Files** (all scratch output in `C:\Users\Liang\AppData\Local\Temp\claude\audit-physics\`; nothing in either repo was modified):
- `checks.py`: Helmholtz, solenoid, Curie and coil-centre checks
- `bloch.js`: rotation, precession sign and Hahn echo checks
- `exact.js`: `physics.js` with the field maps loaded
- `conv.js`, `physics_sdconv.js`: the handedness experiment
- `fw.js`: FWHM against record length
- `bs.wls`: Biot–Savart recompute of 5 map voxels

