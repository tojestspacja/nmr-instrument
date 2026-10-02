# Current State — Available Snapshot and Intended Continuation

Prepared: 2026-10-03. This is an initial handoff reconstructed from supplied materials, not a live update from the user's Windows computer.

## Executive state

**Available ZIP:** legacy/main snapshot. **Intended continuation from the later transcript:** Milestone 4, partial ESP-IDF firmware after native-core/pulse-programming and WASM checkpoints. **Immediate action:** locate and protect the actual live v2 working tree before editing.

The earlier generic handoff example that stopped at Milestone 1 is outdated relative to the attached execution record. This does not turn transcript-only work into source present in the ZIP.

## Evidence vocabulary

| Label | Meaning |
|---|---|
| `ARCHIVE_OBSERVED` | Directly inspected in the supplied archive or its Git metadata. |
| `TRANSCRIPT_REPORTED` | Recorded in the uploaded execution history, not rerun here and not proof of current source availability. |
| `LIVE_REVALIDATED` | Reserved for new checks tied to actual files, hashes, output and time on the receiving system. None assigned here. |
| `UNKNOWN` | Not established by the supplied material. |

## Available archive

| Item | Observed value |
|---|---|
| Name | `nmr-instrument.zip` |
| SHA-256 | `5b47c19c3b04185bcf76827f23cc0bc47bf2260fd1638702b030e88a8a95b8ba` |
| Size | 13,750,572 bytes |
| ZIP entries | 359, including directories and Git metadata |
| Root | `nmr-instrument/` |
| Branch | `main` |
| HEAD | `0914edbc5f9424503e6046394dd632a815474fc3` |
| Refs | `main`, `origin/main`, symbolic `origin/HEAD`; no v2 reference observed |
| Content outside `.git` | 160 files, all tracked in the archived index |
| Index comparison | 48 byte-identical; 112 differ only by CRLF/LF; no other content differences found |
| Git object check | One unreachable blob; no unreachable v2 commit reported |

The line-ending comparison does not authorize resetting or normalizing a different live checkout.

Present: `README.md`, `index.html`, `simulator/`, `mechanical/`, `fabrication/`, `hardware/README.md`, `tools/handin.py`, `.nojekyll`, `.git/`.

Absent from the uploaded snapshot: `legacy/`, `design/`, `generated/`, `physics/`, `pulse/`, `firmware/`, `tests/`, `docs/audit.md`, `validation/legacy-audit/` and the earlier proposed orchestration/handoff documents. See `snapshot-manifest.json` for all 160 file fingerprints.

## Later transcript progress

| Phase | Recorded work | Original transcript lines | In ZIP? |
|---|---|---|---|
| M0 | Electronics, firmware/host/DSP and physics agents returned detailed audits; checks/builds reported | 3920–4414 | No audit artifacts; reports preserved here as historical evidence |
| v2 separation | New branch; old site/CAD/simulator moved into `legacy/` | 4457–4473 | No |
| M1 | YAML, schema, generator, generated files and 12 passing config tests | 4598–5310 | No |
| M2/M3 | C++20 physics/DSP, pulse IR/compiler/validator/codec, simulation, C ABI, Python binding, tests | 5311–8617 | No |
| WASM | Shared-core build, JS loader/ABI checks, one native/WASM parity test | 8618–9077 | No |
| M4 planner | Shared pulse backend and 20 planner checks; selected suites rerun after rate changes | 9104–9478 | No |
| M4 firmware WIP | PlatformIO/ESP-IDF project, shared components, gate engine, ADC streamer, RF component | 9078–10746 | No |
| Interruption | RF component registration and I2C header lookup immediately before usage-limit message | 10737–10747 | Historical stopping point |

## Commit locators from the transcript

| Prefix | Recorded checkpoint |
|---|---|
| `b61c1e9` | M0/M1 audit, legacy migration, canonical model and tests |
| `1223c06` | LF policy for generated-file checks |
| `de1a13f` | Corrected M2/M3 checkpoint after fixing a failed duplicated-constant test |

These are lookup hints, not available commits in this ZIP or verified remote refs. `85d58a3` was amended and should not be preferred over the corrected checkpoint. A later WASM commit command is recorded without its resulting hash. Subsequent firmware work may be uncommitted. **Recover WIP separately from commits.**

## Exact interruption point

Last recorded working area: `C:/Users/Liang/tigp-2026/nmr-instrument/firmware`.

Relevant final files:

```text
firmware/components/engine/include/engine.hpp
firmware/components/engine/engine.cpp
firmware/components/adc/include/adc_stream.hpp
firmware/components/adc/adc_stream.cpp
firmware/components/adc/CMakeLists.txt
firmware/components/rf/include/rf.hpp
firmware/components/rf/rf.cpp
firmware/components/rf/CMakeLists.txt
```

The transcript announces a fractional-delay I/Q sampling-skew correction and firmware main control loop as upcoming work. The last RF header/API lookup is **not** an end-to-end firmware build result. No complete new-firmware build/flash/bench result is established after those final writes.

## Hardware state

The archive README states design stage and no physical measurements. Later the user says an ESP32 was connected; serial enumeration reports `COM6`, VID `303A`, PID `1001` (lines 5049–5063). Still later the session identifies an N16R8 configuration from project information (9547–9586).

These are historical observations. Do not repeat the earlier “no board connected” statement as current fact. A connected USB device is not flash authorization, an electronics test or evidence of an FID. No measured NMR FID, spectrum or spin echo is evidenced in the supplied materials.

## Ownership

The archive's `README.md` and `hardware/README.md` keep the existing class-board hardware/firmware in the shared course repository, pinned to `eef00d20a10deca994a8efe07c588c22af2b9db0`. The later rewrite starts experimental firmware in `nmr-instrument/v2`.

Preserve both facts: v2 is a successor under development, not an approved replacement for the instructor's repository. Document the integration boundary when the live rewrite is recovered; do not silently overwrite the class-board firmware.

## Next operation

Read-only discovery of the live checkout, its uncommitted files, relevant refs and worktrees. Follow `RECOVERY_PLAN.md`, then `NEXT_ACTIONS.md`. Do not restart the project or replay the legacy migration.
