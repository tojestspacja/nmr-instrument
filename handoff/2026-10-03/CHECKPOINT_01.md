# Live Handoff Checkpoint 01

> Live-verified. Every result here was produced on the receiving machine this session (LIVE_REVALIDATED), not copied
> from the transcript. Historical/transcript claims stay in the 2026-10-03 handoff docs.

## Identity

- Timestamp with timezone: 2026-10-03, Asia/Taipei (UTC+8)
- Actual workspace: `C:/Users/Liang/tigp-2026/nmr-instrument`
- Branch and full HEAD: `v2` @ (this checkpoint's commit; parents below)
- Config hash: `5469d421b230e04050b1857e02686054efc885e866bb7cbee033ac091047e0a4` (`gen_config.py --check` = up to date)
- Parent handoff: `handoff/2026-10-03/CURRENT_STATE.md`
- Recovery case: **Case 1** — the live v2 working tree and WIP were present and intact; no reconstruction needed.
- Access-controlled backup/checkpoint: local commits only. **Not pushed to any remote** (origin has only `main` @ `0914edb`). Off-disk backup still OWED.

## Recovery handshake (resolved)

The 2026-10-03 handoff was built from an older `main`-snapshot ZIP, so it treated "v2 source lost" (HANDOFF-001) and
"raw validation artifacts lost" (HANDOFF-002) as blockers. Live inspection disproved both: branch `v2`, the full commit
chain `b61c1e9 -> 1223c06 -> de1a13f -> 93a0c84`, all source, and `validation/` (DRC/ERC json+rpt, golden fieldmaps,
legacy-audit scripts) are present. Only the WASM binary was stale, and `firmware/main/` was empty.

## Work done this session

1. Protected the as-found WIP verbatim before editing (`7649f41` code, `5794b56` handoff docs).
2. **SIM-PARITY-001 closed** (`5968d6b`): the committed `nmrcore.wasm` embedded config `b759cbb0...`, native was
   `5469d421...`; rebuilt WASM from current sources (393 KiB), parity now passes.
3. **FW-RESUME-01 + FW-RESUME-03** (`11c074e`): wrote `firmware/main/` (control state machine + experiment sequencer)
   and fixed two build blockers (`default_16MB.csv` partition table; `adc_stream.hpp` volatile `++`). Firmware builds.

## Working files

| Path | State | Intent/completeness | Owner | Backup |
|---|---|---|---|---|
| `firmware/main/{controller.hpp,controller.cpp,main.cpp,CMakeLists.txt}` | committed `11c074e` | FW-RESUME-01 complete; builds; NOT bench-validated | this session | local commit only |
| `firmware/default_16MB.csv` | committed `11c074e` | N16R8 partition table | this session | local commit only |
| `firmware/components/adc/include/adc_stream.hpp` | committed `11c074e` | volatile-`++` build fix | this session | local commit only |
| `simulator/web/nmrcore.wasm` | committed `5968d6b` | rebuilt from current config | this session | local commit only |
| `firmware/sdkconfig.esp32s3` | gitignored | PlatformIO-generated build output | — | n/a |

Working tree is clean. An empty diff here means committed, **not** backed up off this disk.

## Evidence checked in this session

| Command | Source/config identity | Result | Raw log |
|---|---|---|---|
| `py tools/gen_config.py --check` | config `5469d421` | up to date (exit 0) | stdout |
| `cmake -S . -B build -G Ninja && cmake --build build` | HEAD v2 | build OK (exit 0) | `/tmp/build.log` (transient) |
| `ctest --test-dir build` | HEAD v2 | 6/6 PASS (bloch, field, dsp, pulse, sim, backend) | stdout |
| `py -m pytest -q tests` | HEAD v2 | 16 passed | stdout |
| `py simulator/wasm/build.py` | config `5469d421`, wasi-sdk 25 / clang 19 | wrote `nmrcore.wasm` 393 KiB (exit 0) | stdout |
| `py -m pytest -q tests/test_wasm_parity.py` | native+WASM both `5469d421` | 1 passed (config sha match, I=Q=0 LSB) | stdout |
| `pio run -d firmware -e esp32s3` | HEAD v2, espressif32@7.1.2 / ESP-IDF 6.1.0 | **[SUCCESS]** RAM 8.1% (26544 B), Flash 7.8% (325155 B) | task log `b7cq1lknu.output` |

Toolchain seen: Python 3.11.5, CMake 3.31.5, Ninja 1.12.1, g++ 14.2 / clang 19.1.7, Node 24.15, PlatformIO 6.2.0.
`idf.py` not on PATH; ESP-IDF is used via PlatformIO's `.espidf-6.1.0`.

## Issues and decisions

- Closed this session: SIM-PARITY-001; FW-RESUME-01, FW-RESUME-03.
- Downgraded (live-disproven): HANDOFF-001, HANDOFF-002.
- Still open (unchanged): FW-IQ-001 (I/Q aperture skew, below), FW-ADC-001, SYS-PHASE-001, HW-TXR-001, HW-B0-001,
  HW-SAFE-001, HW-INTEGRATION-001, NMR-EVIDENCE-001, DOCS-ADR-001 (no `design/adr/` exists; ADR-0004..0009 referenced
  in code/comments are not files).
- Hardware actions authorized: **none**. No flashing, no energising performed or requested.

## FW-IQ-001 — scoped, not yet implemented

ADS8688 AUTO_SEQ delivers Q (ch 2) then I (ch 3), so within each complex sample I is sampled one conversion period
(1 / 250 kHz = 4 us) after Q. At the 5.4 kHz IF that is 4 us x 5400 Hz x 360 deg = ~7.8 deg of I/Q phase skew — matching
the transcript's estimate. Correction (a fractional-delay realignment) belongs once in the shared `physics/src/pipeline.cpp`
and the simulator, with tests for sign, channel order, +/- tone offsets, transient boundaries and replay. The firmware
controller deliberately does NOT apply it and claims no I/Q alignment.

## Next agent's first task

Implement FW-IQ-001 in the shared pipeline. Inputs: `physics/src/pipeline.cpp`, `physics/include/nmr/pipeline.hpp`,
`physics/src/dsp.cpp`, the simulator record generator, and a new/extended test under `physics/tests/`. Permitted writes:
those paths + `firmware/` only if an interface changes. Completion test: a host test shows a known-tone's recovered phase
corrected to within tolerance for both tone signs, `ctest` stays 6/6 (+ the new case), `pytest` 16+ passes, and native/WASM
parity still passes after rebuilding the WASM. Do NOT claim physical I/Q alignment without bench measurement.

Also OWED and separate: push `v2` off-disk (no backup exists yet) — requires the user's go-ahead.
