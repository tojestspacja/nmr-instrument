# Exact Next Actions

## Entry condition

First recover and protect the v2 sources under `RECOVERY_PLAN.md`. With only the supplied main snapshot, source recovery is the next task; M4 implementation cannot truthfully be called resumed yet.

## 1. Establish the live baseline

Inspect current project instructions, build files, dependencies, generated representations and actual ADRs. Capture HEAD, WIP and config/source hashes. Verify config freshness, native build/tests, Python tests and native/WASM parity against the same configuration.

The later change to 125 kS/s and decimation 5 occurred after the first parity result. A stale WASM binary must not be accepted because it exists. Record unavailable toolchains as `NOT_RUN`, not `PASS` or an instrument failure. Inspect installed versions before installing anything.

## 2. Resume Milestone 4 at the firmware integration boundary

Read these actual paths first:

```text
firmware/platformio.ini
firmware/CMakeLists.txt
firmware/sdkconfig.defaults
firmware/main/
firmware/components/engine/include/engine.hpp
firmware/components/engine/engine.cpp
firmware/components/adc/include/adc_stream.hpp
firmware/components/adc/adc_stream.cpp
firmware/components/rf/include/rf.hpp
firmware/components/rf/rf.cpp
firmware/components/*/CMakeLists.txt
pulse/include/pulse/backend.hpp
pulse/src/backend.cpp
physics/include/nmr/pipeline.hpp
physics/src/pipeline.cpp
design/instrument.yaml
generated/instrument_config.hpp
```

Check whether newer live work already fills `firmware/main/`. Preserve it. Otherwise finish the main/control entry point around the existing components rather than writing new duplicate drivers.

### FW-RESUME-01 — Main/control integration

Implement and verify pre-arm configuration/program checks; safe idle/configured/armed/running/complete/abort/fault transitions; control-plane RF setup and bus ownership; gate/acquisition/capture coordination; preallocated buffers and explicit capacity limits; visible incomplete-record/overflow errors; and safe outputs after initialization/API/abort failures.

Map these requirements to any state model already implemented. Use the shared DSP pipeline and reliable record identity/metadata rather than another independent DSP or stale-frame path. These are acceptance conditions, not claims the partial firmware satisfies them.

### FW-RESUME-02 — Pending I/Q sampling-skew correction

The final session explicitly proposes a fractional-delay correction after estimating about 7.8 degrees of I/Q skew. Keep the number labeled as a prior estimate.

Establish conversion order, per-channel aperture times, frame layout and timestamp meaning from the recovered driver and applicable ADC documentation. Implement and test correction consistently in the shared pipeline and simulator, exactly once. Include positive/negative tone offsets, channel order/sign, transient boundaries and replay tests. Do not declare physical I/Q alignment validated without measurements.

### FW-RESUME-03 — Complete firmware build

Run the live project's declared build after integration, capturing the unfiltered exit code and log. The final I2C header lookup is not a complete build result.

Fix compile/link/configuration errors before claiming an executable firmware milestone. Inspect final flash/PSRAM settings: the transcript begins with an 8 MB default and later uses a reported N16R8 configuration. The first Write of a config file is not its final state.

### FW-RESUME-04 — Hardware verification plan, not invented results

Prepare finite gate-timing, capture/phase, ADC-framing/throughput, recovery and known-tone loopback tests. Check board connection afresh; COM6 is historical.

Do not flash or energize merely because the new session has access. Preserve existing board firmware and obtain explicit flashing authorization. GPIO values for proposed ECO inputs do not establish that wires exist.

## 3. Keep the other instrument blockers visible

T/R isolation/protection, B0 stability, fault wiring, receiver behavior and probe calibration remain separate from software compilation. Parallel reviewers may investigate these under disjoint write ownership.

## Do not redo indiscriminately

Do not repeat the full legacy audit, recreate the generator, replace the pulse IR, discard maps, or reorganize CAD/site paths merely because the new agent lacks the old chat. Rerun targeted checks when inputs changed, evidence is absent or contradictions exist.

A firmware build is not measured jitter, ADC throughput, TX/RX isolation, stable B0, calibrated probe, an FID, spectrum or echo. Preserve those experimental gates separately.
