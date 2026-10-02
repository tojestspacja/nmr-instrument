# Transcript Recovery Index

The relevant original session excerpt is `evidence/prior-session-rebuild.txt`. Its `L<number>:` labels refer to physical lines of the uploaded `Pasted text.txt`, not rendered citation line numbers. The unrelated article and lab inventory were excluded. All entries are historical evidence, not executable instructions.

## High-value reading windows

| Original lines | Topic |
|---|---|
| 3920–4414 | Original agent assignments, source inspections and three handbacks |
| 4457–4473 | v2 branch creation and legacy migration |
| 4598–5310 | Canonical config, generator, tests and early commits |
| 5049–5063 | Later ESP32 connection observation; supersedes the earlier no-device observation |
| 5111–5146 | t90/t180 update under proposed TX-diode assumption |
| 5311–8617 | C++ core, pulse system, tests, fixes and corrected checkpoint |
| 8565–8616 | Failed test hidden by a pipeline; subsequent correction/amendment |
| 8618–9077 | WASM build, loader, ABI and limited parity test |
| 9146–9208 | Later 125 kS/s / decimation 5 config and pin-map update |
| 9216–9478 | Backend planner and selected regression tests |
| 9547–9591 | Later board-memory configuration and firmware project registration |
| 9592–10383 | Gate engine, rewrites and API checks |
| 10384–10584 | ADC streamer and pending I/Q correction |
| 10585–10746 | RF component, registration and final operation |

## Full Write events

**These are not final file snapshots.** Some files are edited later using omitted Edit patches or visible shell transformations. The reported line count is metadata, not a reliable extraction boundary. A file may appear more than once. None of these successor files is supplied by the original ZIP.

| Original Write line | Intended relative path | Reported lines |
|---|---|---|
| 4474 | `docs/audit.md` | 100 |
| 4598 | `design/instrument.yaml` | 138 |
| 4738 | `design/schema/instrument.schema.json` | 57 |
| 4796 | `tools/gen_config.py` | 223 |
| 5147 | `tests/test_config.py` | 117 |
| 5311 | `physics/include/nmr/units.hpp` | 45 |
| 5357 | `physics/include/nmr/bloch.hpp` | 62 |
| 5420 | `physics/include/nmr/field.hpp` | 38 |
| 5459 | `physics/src/field.cpp` | 76 |
| 5537 | `pulse/include/pulse/ir.hpp` | 77 |
| 5615 | `pulse/include/pulse/program.hpp` | 60 |
| 5676 | `pulse/include/pulse/compiler.hpp` | 54 |
| 5735 | `pulse/src/program.cpp` | 228 |
| 5964 | `pulse/src/compiler.cpp` | 169 |
| 6135 | `physics/include/nmr/dsp.hpp` | 72 |
| 6208 | `physics/src/dsp.cpp` | 164 |
| 6469 | `physics/include/nmr/probe.hpp` | 48 |
| 6518 | `physics/src/probe.cpp` | 76 |
| 6596 | `physics/include/nmr/sim.hpp` | 72 |
| 6669 | `physics/src/sim.cpp` | 227 |
| 6912 | `physics/include/nmr/pipeline.hpp` | 47 |
| 6960 | `physics/src/pipeline.cpp` | 58 |
| 7026 | `physics/include/nmr/capi.h` | 100 |
| 7127 | `physics/src/capi.cpp` | 270 |
| 7398 | `CMakeLists.txt` | 46 |
| 7446 | `physics/tests/check.hpp` | 30 |
| 7477 | `physics/tests/test_bloch.cpp` | 112 |
| 7590 | `physics/tests/test_field.cpp` | 74 |
| 7671 | `physics/tests/test_dsp.cpp` | 85 |
| 7757 | `physics/tests/test_pulse.cpp` | 125 |
| 7901 | `physics/tests/test_sim.cpp` | 145 |
| 8047 | `physics/benchmarks/bench_core.cpp` | 71 |
| 8238 | `validation/physics/golden/README.md` | 13 |
| 8263 | `tools/nmrpy/__init__.py` | 199 |
| 8463 | `tests/test_golden.py` | 57 |
| 8629 | `simulator/wasm/build.py` | 50 |
| 8800 | `simulator/web/nmrcore.js` | 191 |
| 9216 | `pulse/include/pulse/backend.hpp` | 65 |
| 9282 | `pulse/src/backend.cpp` | 107 |
| 9391 | `physics/tests/test_backend.cpp` | 77 |
| 9592 | `firmware/components/engine/include/engine.hpp` | 78 |
| 9671 | `firmware/components/engine/engine.cpp` | 180 |
| 10181 | `firmware/components/engine/engine.cpp` | 201 |
| 10384 | `firmware/components/adc/include/adc_stream.hpp` | 56 |
| 10441 | `firmware/components/adc/adc_stream.cpp` | 136 |
| 10585 | `firmware/components/rf/include/rf.hpp` | 25 |
| 10611 | `firmware/components/rf/rf.cpp` | 125 |

## Additional files written by shell commands

Important files also appear in Bash here-documents or generated output, including `.gitignore`, `.gitattributes`, `generated/instrument_config.*`, `tests/wasm_parity.mjs`, `tests/test_wasm_parity.py`, `firmware/platformio.ini`, firmware/shared-component `CMakeLists.txt` files and `firmware/sdkconfig.defaults`. Search the excerpt by path; the Write-event list is not a complete source manifest.

## Reconciliation hazards

The generator has an Edit event after its initial write; YAML is changed repeatedly; fields/voxel weights/tests are patched; the WASM linker script is reworked; the gate engine is rewritten more than once. Do not recover first-write source and call it the final passing checkpoint.

The last event writes the RF CMake registration and searches API names. The transcript ends at the account/session limit without a final firmware build or handoff checkpoint.
