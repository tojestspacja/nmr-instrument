# Milestone 0 — audit of the legacy sources

Sources audited (read-only):

| Archive | Repository | Commit |
|---|---|---|
| `class-board-2026 (2).zip` | TIGP-Experimental-Methods/class-board-2026, branch `w1-yi-tsai` | `da56d2b` (hardware/firmware identical to `main` @ `eef00d2`) |
| `nmr-instrument.zip` | tojestspacja/nmr-instrument, `main` | `0914edb` (now under `legacy/` on this branch) |

The audit ran KiCad 10 ERC/DRC, exported the netlist, built both firmware environments with PlatformIO, re-ran the Biot–Savart
field-map computation in Mathematica 13.3 and checked the Bloch code against an RK4 integration. Nothing in the legacy
sources has ever run on real hardware. Raw audit outputs: `validation/legacy-audit/`.

## 1. Feature inventory

### Electronics (class-board-2026, KiCad authority)

| Feature | Class | Evidence / reason |
|---|---|---|
| Si5351A (25 MHz crystal): CLK0 50 MHz DDS clock, CLK1 = 4·f_LO into a 74HC74 Johnson counter (I/Q LO) | RETAIN | Sound; one reference for TX and LO |
| AD9834 DDS + 3 MHz 3rd-order reconstruction filter | RETAIN | Values consistent (R801 6.8 k, 0.636 Vpp) |
| AD9834 RESET/SLEEP pins only pulled down, never driven | REIMPLEMENT | Reset only via control word; no hardware phase reset |
| OPA564 power stage, G = 25 (R807/R808), I_LIM 1.5 A (R809 11 k) | RETAIN topology | — |
| OPA564 I_FLAG/T_FLAG unconnected (DNP pull-ups, `pins.h:96-97` = −1) | REIMPLEMENT | Over-current abort documented but impossible |
| TX output snubber R814 10 Ω + C817 10 nF to GND | VERIFY | Loads a shared coil's tank heavily during receive (179 Ω at 89 kHz) |
| Crossed diodes D703/D704 directly on the RX node; **no T/R switch** | REIMPLEMENT | With one coil on TX and RX the diodes carry ≈1.4 A from the OPA564 (at I_LIM) and B1 drops ~6× — `docs/tx-rx.md` |
| Tank C710+C711+C712 all fitted = 1.322 nF on the board's RX node | VERIFY | Resonance ≈87 kHz, not 89.4 kHz; a tank on the board side of the cable is cable-dependent |
| OPA1656 LNA stage 1, gain set via header J3 | REIMPLEMENT | **Without a J3 shunt stage 1 has no feedback** (netlist: J3-1 and J3-2 on different nets) |
| DG419 blanking between LNA stages (RX_BLANK, GPIO8) | RETAIN | Correct place for post-LNA blanking; does not isolate the coil |
| Double-balanced TS5A23157 commutating mixer + OPA1612 difference amplifiers | RETAIN / VERIFY | IF gain computes to ≈10, documented 20; unequal P/N poles |
| ADS8688 on shared SPI2 (with AD9834 and DAC8563), ±5.12 V range, channels AIN3 (I) / AIN2 (Q) | VERIFY | Sustained two-channel rate on a shared bus not demonstrated |
| DRV8871 H-bridge as B0 driver, I_TRIP 2.0 A (R920 32 k), **no current sense** | REIMPLEMENT | Chopping limiter, not a regulated current source; B0 drifts with coil temperature; chopping ripple modulates B0 |
| Polarizer FET AOD4184A + UCC27517 | RETAIN / VERIFY | Driver VDD only via J12 shunt; ISENSE_COIL not routed to the ADC |
| One GND net on both boards (Decision #73) | VERIFY | 1000× receiver shares copper with the polarizer (13 A), H-bridge and TX returns |
| +VEXT TVS SMBJ26A (clamps ~42 V) | REIMPLEMENT | Above the OPA564's 26 V absolute maximum |
| ERC/DRC | RETAIN | ERC 0 errors (2 cosmetic warnings), DRC 0 violations, 0 unconnected |

### Firmware (class-board-2026 `firmware/`, Arduino-ESP32 2.0.17)

| Feature | Class | Evidence / reason |
|---|---|---|
| TX gate = `digitalWrite` + `esp_timer` busy-wait (`Sequencer.cpp:427-430`) | REIMPLEMENT | Pre-emptible (AsyncTCP prio 10, arduino_events on core 1): an over-pulse lasts as long as the pre-emption; no flag to catch it |
| ADC burst: Arduino `SPI.transfer32`, `esp_timer` polling per sample (`Ads8688.cpp:163-225`) | REIMPLEMENT | Jitter; late samples taken back-to-back silently; header reports requested, not achieved, rate |
| NMR-FIRMWARE.md ↔ code (ADS8688 "spi_device_polling_transmit", "critical region", "priority high", AD9834 PHASE1 90/180) | REMOVE | 10 documented discrepancies (firmware audit §2) |
| **Phase coherence: TX (PLLA/DDS) and LO (PLLB) free-run; scan start is ESP32-timed** | REIMPLEMENT (missing) | IF phase at the pulse is random per scan → averaging and CYCLOPS incoherent on hardware; SIM hides it (`Sequencer.cpp:627-629`) |
| Grow-only PSRAM buffers at `config` | RETAIN | No per-scan allocation |
| `Dsp.cpp` pure functions (offset, boxcar decimation, CYCLOPS rotation, radix-2 FFT, parabolic peak) | RETAIN concept, REIMPLEMENT | Correct but untested; no digital mixing; three incompatible spectrum implementations (firmware, JS, Python; 2√2 scale differences) |
| Si5351/AD9834 drivers | RETAIN / VERIFY | AN619-correct; `setClk1` return unchecked; SPI mutex inside the timed path |
| Per-scan 400 kB WebSocket broadcast under a lock | REIMPLEMENT | Heavy; jitters TR |
| `instrument.py next_binary` returns the oldest parked record | REMOVE | Returns scan 1, not the average |
| Full-record Hann window (`instrument.py:287-302`) | REMOVE | ≈10 dB SNR loss for a T2* ≪ record |
| Four simulators (firmware SIM, ADS8688 SIM, dev-nmr.html, docs) | REMOVE | Replace with the shared physics core |
| CI: build only, ruff; **no unit tests** | REIMPLEMENT | — |

### Physics and simulation (nmr-instrument `legacy/`)

| Feature | Class | Evidence / reason |
|---|---|---|
| Rodrigues rotation about −B_eff in the f_tx frame; precession e^{−iΔωt} (`physics.js`) | RETAIN | Matches RK4 of dM/dt = γ M×B to 5 decimals |
| Curie M0, reciprocity pickup, rotating B1 = ½ linear field | RETAIN | Analytically correct |
| Exact Biot–Savart field maps (`fieldmap-*.json`), elliptic-integral loop kernel | RETAIN as golden data; port the kernel | Recomputed voxels match every printed digit |
| Approximate 4th-order Helmholtz + on-axis solenoid | REIMPLEMENT | B0 ignores N rounding (311.4→312); B1 has no radial/two-layer model → t90 451 µs vs exact 417.4 µs |
| `nmr_exact.wls` SpinDynamica comparison | REIMPLEMENT | Handedness mismatch (pulse about +H, FID e^{−iΩt}); explains the 0.6 ms "agreement" gap |
| Reported "FWHM 6 Hz" | REMOVE | Window-limited by the 0.2 s record (1.207/T) |
| Scalar tank gain, no transient, unshaped noise | REIMPLEMENT | Needs complex response |
| `spectrum.py` sample model (r 15 mm, x −50…25 mm) | REIMPLEMENT | Disagrees with the other models (r 14 mm, −58…+25 mm + cone) |
| Coil-heating drift model | RETAIN | Self-consistent |
| `test-physics.js` | REIMPLEMENT | Prints, asserts nothing |
| Line at 1.5 A: 89 400 / 89 560 / 89 585 Hz in three models | REIMPLEMENT | One field model, one source of geometry |

### Mechanical and site

| Feature | Class | Reason |
|---|---|---|
| OpenSCAD housing, probe, B0 pair, cut files | RETAIN as reference; REIMPLEMENT canonical in build123d | Not STEP/BREP; board geometry imported as connector-only STL |
| Mesh assets for the 3D bench | RETAIN (assets) | — |
| Housing hand-in snapshot (class-board) | RETAIN | Course requirement |

## 2. Known problems that block real NMR (ordered)

1. **Phase coherence between TX, LO and acquisition** — averaging cannot work (ADR-0006).
2. **No T/R switch on a shared coil** — the transmitter is shorted into the RX clamp diodes (ADR-0007).
3. **B0 is not current-regulated and not measured** — 23 W heating moves the line ≈138 Hz/min; DRV8871 chopping modulates B0 (ADR-0008).
4. **LNA stage 1 open-loop without the J3 shunt** — bring-up step, then a schematic fix.
5. **Software-timed TX gate** — over-pulse risk with no hardware flag (ADR-0005).
6. Tank as fitted ≈87 kHz; IF gain ≈half the documented value; OPA564 flags unwired; TVS above the OPA564's absolute maximum.

## 3. Dependency map (legacy)

```text
nmr-params.scad ──regex──► spectrum.py           (constants copied by hand into:)
                          physics.js P{}         Sequencer.h defaults, nmr.js defaults,
                          nmr_exact.wls          instrument.py NMR_DEFAULTS, dev-nmr.html,
                                                 NMR-FIRMWARE.md, PROTOCOL.md
KiCad (class-board) ──export──► release/*.stl/.step ──► OpenSCAD housing, simulator meshes
firmware Sequencer ──WebSocket──► PWA nmr.js (own FFT) / instrument.py (own FFT)
```

Every arrow labelled "by hand" is replaced in v2 by generation from `design/instrument.yaml`.
