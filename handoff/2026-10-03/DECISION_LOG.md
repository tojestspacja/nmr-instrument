# Decision Continuity Register

## Scope

This is a register of decisions/proposals visible in the supplied material, not a new set of approved ADRs. The transcript references ADR-0004 through ADR-0009, but the ZIP contains no `design/adr/` and the transcript does not establish that all those ADR files were written. Locate live ADRs before assigning approved status.

The user wants a from-scratch successor retaining useful work and progressing to real sample measurements. Account recovery is not a request for another restart of work already completed on v2.

## Recorded architectural direction

| Topic | Recorded direction | Evidence and limitation |
|---|---|---|
| Repository separation | Move old site/CAD/simulator under `legacy/` on `v2`, preserve main | Transcript L4457–4473. Not present in the ZIP. Do not replay against recovered v2. |
| Config | YAML quantities with value/unit/kind/source; generated C++/JSON/Python | L4598–5310, later amended. Canonical design values are not automatically measured facts. |
| Numerical core | C++20 physics/DSP, C ABI, Python wrapper, same sources built to WASM | L5311–9077. Keep reference tests and performance claims separately traceable. |
| Pulse system | Hardware-independent IR/compiler/validator/codec and backend planner | L5537–6134; L9216–9478. Read interfaces before adding experiments. |
| Real-time gates | RMT waveforms; MCPWM capture/timestamps | L9592–10383. Written source/header inspection is not measured determinism. |
| ADC | Proposed I2S/DMA framing instead of CPU-paced Arduino SPI | L9146–9208; L10384–10584. Protocol suitability and actual timing remain verification tasks. |
| Phase reference | Proposed DDS SIGN BIT and LO_I capture inputs, marked ECO-1 | L9182–9184; L10585–10610. Wiring must be checked, not assumed. |
| Firmware ownership | Experimental successor in `nmr-instrument/v2`; existing class-board implementation remains authoritative for the current board | Archive README/hardware README versus later rewrite. Document eventual integration separately. |
| Mechanical | STEP/BREP intended for successor; useful OpenSCAD/assets retained | User mission and historical audit. No completed successor BREP assembly is established. |

## Later numeric choices — do not revert by copying an early Write

These are historical design choices only. Inspect live YAML and provenance before using them.

| Quantity | Earlier value | Later recorded change | Meaning |
|---|---|---|---|
| t90 / t180 | 417 / 834 microseconds | 458 / 916 microseconds | L5111–5146, calculated with a proposed 0.7 V TX series-diode drop, not measured nutation calibration |
| Acquisition duration | Legacy firmware default 2 s | v2 target 0.5 s | M1 choice, not a second independent default to maintain |
| Raw rate / decimation | 100 kS/s per channel / 4 | 125 kS/s per channel / 5 | L9146–9208; both target 25 kS/s complex output; hardware operation unverified |
| ADC framing | CPU-paced 32-bit SPI | Proposed 16 MHz clock, 64-bit frame, one conversion per frame | Recorded rationale; actual framing and ADC timing require validation |
| Flash/PSRAM | Initial 8 MB flash config | Later reported N16R8, 16 MB flash / 8 MB octal PSRAM | L9547–9586; verify final configuration and actual board |
| Gyromagnetic ratio | Legacy rounded value | Source-qualified shielded-proton value in v2 YAML | Preserve convention/provenance; do not mix constants silently |

## Historical statements that need careful handling

The audits mix source observations, calculations, estimates and remedies. Preserve those evidence types. In particular, the early statement that a shared-coil problem necessarily causes firmware to abort is not a safe inherited fact: the later audit reports the relevant amplifier fault pins as unwired. Keep the safety concern without implying a working interlock.

The transcript distinguishes worst-case field range, record/window-limited FWHM and envelope decay. Do not revive earlier superseded linewidth estimates as golden targets.

Do not silently “correct” hardware topology, sign convention, ADC framing or safety requirements during handoff preparation. Resolve them explicitly against live sources and independent tests during continuation.

## Changing a decision after recovery

Record actual inputs, alternatives, rationale, affected interfaces/tests/hardware and validation evidence. Preserve superseded decisions. Use real live ADR paths rather than inventing approved ADR identifiers.
