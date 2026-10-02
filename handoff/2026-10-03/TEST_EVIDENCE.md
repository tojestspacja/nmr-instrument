# Test and Build Evidence Ledger

## Status

All instrument results below are **TRANSCRIPT_REPORTED**. Handoff preparation did not rerun NMR builds, numerical validation or hardware tests. New results must identify actual source/config hashes.

| Evidence | Recorded result | Original transcript lines | Does not prove |
|---|---|---|---|
| M1 configuration | 12 tests passed | 5265–5272 | Recovered live inputs match that run |
| M2/M3 native summary | Bloch 26, field 29, DSP 15, pulse 31, simulation 14; 115 native checks reported | 8567–8572; 8614–8617 | Firmware build or hardware operation |
| M2/M3 Python | 15 passed after duplicate-constant fix | 8583–8616 | All subsequent changes passed |
| Golden maps | 3555 tube / 4510 bottle voxels; max df 5.48e-4 Hz; relative B1 2.80e-6 / 4.95e-6; phi 5.00e-5; 3 tests passed | 8558–8563 | Measured magnet field or complete model adequacy |
| WASM | 393 KiB module reported | 9061–9062 | Binary reflects later config changes |
| Native/WASM parity | One test passed; sampled ADC differences I=0, Q=0 LSB | 9034–9065 | Universal bitwise agreement for all inputs or processed outputs |
| Later config update | 12 config tests passed after 125 kS/s / decimation 5 and pin-map change | 9146–9207 | Fresh parity after that change or actual ADC rate |
| Backend planner | 20 checks passed; selected pulse/sim/DSP suites also passed | 9469–9478 | One fresh run of every earlier suite |
| Final firmware | Components written/registered; headers inspected | 10181–10746 | Complete build, link, flash or bench acquisition |

The parity test checks selected record slices and uses tolerances for processing/peaks. Its surrounding prose is stronger than the actual assertions. Do not generalize one test into universal exactness.

## Preserve the failed-test history

The M2/M3 command piped pytest through `tail`, masking the producer's failure status. The transcript records a commit despite `1 failed, 14 passed`. The author fixed duplicate constants, reran, and amended to `de1a13f` (L8565–8616).

Always capture each producer's actual exit code and complete logs. Do not use a successful pipe consumer, an empty grep result or “no tests found” as test success. Record skips distinctly; a skipped WASM test is not parity validation.

## Baseline commands after v2 source recovery

Inspect the real files first. These paths do not exist in the supplied main snapshot.

```powershell
py tools/gen_config.py --check
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
py -m pytest -q tests
```

Confirm CTest discovers the declared tests. If it does not, inspect `CMakeLists.txt` and execute the actual test binaries separately.

For parity, once dependencies and the build script are reviewed:

```powershell
py simulator/wasm/build.py
py -m pytest -q -rs tests/test_wasm_parity.py
```

For the new firmware, after integration is ready:

```powershell
pio run -d firmware -e esp32s3
```

No upload/flash command is authorized by these build instructions. On PowerShell, inspect `$LASTEXITCODE` immediately after each external command; on a shell using pipelines, preserve the producer's exit status rather than only the final filter's.

## Environment continuity

The transcript records Windows/Git Bash, Python 3.11, MinGW C++/CMake/Ninja, Node, Clang/WASI tooling and `espressif32@7.1.2`. Installed header output reports ESP-IDF major 6/minor 1 at L9946–9953. These are historical environment observations, not statements about the latest available software or the receiving machine.

Inspect versions, paths and lockfiles. Do not silently upgrade or assume a tool is installed because an old path exists in a report.

## Missing raw artifacts

The uploaded ZIP has no v2 `validation/`. Historical command output retained in this handoff is not the original compiler logs, KiCad JSON/netlists or hardware traces. Recover required raw evidence from the live tree/local audit folders; rerun targeted checks when it is unavailable.

## New evidence record

```text
Command:
Working directory:
Source HEAD and dirty state:
Config SHA-256:
Tool/compiler versions:
Time with timezone:
Exit code:
Executed / passed / failed / skipped:
Raw log path:
Evidence class:
Limitation:
```

Simulation validation, firmware loopback, TX/RX bench validation, probe calibration, FID detection and echo validation remain separate gates.
