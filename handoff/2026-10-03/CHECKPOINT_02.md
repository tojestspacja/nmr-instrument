# Live Handoff Checkpoint 02

> Live-verified on the receiving machine (LIVE_REVALIDATED). Follows CHECKPOINT_01.

## Identity

- Timestamp: 2026-10-03, Asia/Taipei (UTC+8)
- Workspace: `C:/Users/Liang/tigp-2026/nmr-instrument`
- Branch / HEAD: `v2` @ `2323d04` (pushed; `local == origin/v2`)
- Config hash: `5469d421b230e04050b1857e02686054efc885e866bb7cbee033ac091047e0a4` (unchanged; `gen_config --check` up to date)
- Backup: pushed to `origin/v2` (github.com/tojestspacja/nmr-instrument); local `v2-backup-pre-rewrite` kept.
- Commit identity: GitHub noreply `171763498+tojestspacja@users.noreply.github.com` (privacy + push protection ON).

## Work since CHECKPOINT_01

**FW-IQ-001 done** (`2323d04`): I/Q aperture-skew correction implemented once in the shared DSP.
- `dsp::frac_delay` — centred windowed-sinc fractional-delay FIR.
- `PipelineConfig.iq_skew_samples` (opt-in, default 0 → C ABI / WASM parity untouched); I delayed onto Q's grid.
- `InstrumentModel.iq_skew_samples` — simulator models the skew; the skew==0 path is bit-identical (RNG order preserved).
- firmware controller sets it from channel order (+0.5, ch_i>ch_q).
- `physics/tests/test_iq.cpp` (13 checks) — see evidence.

Also pushed earlier this session: Case-1 recovery + WIP protection, SIM-PARITY-001 (WASM rebuild), FW-RESUME-01/-03
(firmware main control loop + it builds). See CHECKPOINT_01.

## Evidence (LIVE_REVALIDATED, 2026-10-03)

| Command | Result |
|---|---|
| `cmake --build build && ctest` | 7/7 suites pass (test_iq: skew err 7.5e-2 uncorrected → 3.6e-4 corrected; wrong-sign worse) |
| `py -m pytest -q tests` | 16/16 (incl native/WASM parity, duplicated-constant guard) |
| `py simulator/wasm/build.py` | nmrcore.wasm 395 KiB, config sha matches |
| `pio run -d firmware -e esp32s3` | [SUCCESS], RAM 8.1% (26544 B), Flash 7.8% (326007 B) |

## Issues

- Closed in session: SIM-PARITY-001, FW-RESUME-01, FW-RESUME-03, FW-IQ-001.
- Still open: FW-ADC-001, SYS-PHASE-001, HW-TXR-001, HW-B0-001, HW-SAFE-001, HW-INTEGRATION-001, NMR-EVIDENCE-001
  (all hardware/bench — unchanged by software), and DOCS-ADR-001 (no `design/adr/`; ADR-0004..0009 are referenced but
  not files).
- Hardware actions authorized/performed: none.

## Next agent's first task

Software options, pick one and scope it:
(a) DOCS-ADR-001 — write `design/adr/ADR-0004..0009.md` as retrospective records of decisions already in the code
    (tick/IR, RMT gates, ADR-0006 phase, ADR-0007 TX diodes, ADS8688 I2S). Do NOT fabricate historical approvals.
(b) Expose `iq_skew_samples` through the C ABI (`nmr_pipeline`) + JS loader so the browser simulator can demonstrate the
    skew/correction too; update `nmr_abi_layout` consumers and re-verify WASM parity.
(c) A console program-loader for the firmware (decode an NMRP program over USB-serial) instead of the built-in default.

The instrument blockers (TX/RX isolation, B0 stability, phase-reference closure, real FID) are hardware and remain the
true gates; keep them separate from software milestones. Completion test for any software task: `ctest` + `pytest` stay
green, WASM rebuilt and parity passes, firmware still builds.
