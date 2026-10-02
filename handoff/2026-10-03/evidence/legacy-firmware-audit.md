# Historical Firmware Agent Handback

> Evidence class: TRANSCRIPT_REPORTED. The previous agent's report follows verbatim, decoded from the uploaded session record. It was not rerun or independently certified during handoff preparation. Estimates, uncertainties and unresolved issues remain in force; later source evidence may supersede it.

Source: `Pasted text.txt`, original line 4390. SHA-256: `c4f56a1a92747fb1d7b77dd57802df4431ef472958be1cf2dc8c7db71106579d`.

---

## Firmware audit: class-board-2026 (branch w1-yi-tsai), NMR path

Everything was read-only. I built both environments in a copy under `C:\Users\Liang\AppData\Local\Temp\claude\audit-firmware\fw\` and changed nothing in the repo. Paths below are relative to `class-board-2026/`. Anything marked **[est]** or **[analysis]** has not been measured on hardware; no hardware run is recorded anywhere.

### 1. How one scan runs today
- **Task:** `xTaskCreatePinnedToCore("nmrseq", stack 12288, priority 5, core 1)` (`Sequencer.cpp:47-49, 235`). It sleeps on `ulTaskNotifyTake` (`:249`) until `start()` (`:254-273`) calls `xTaskNotifyGive`. No timer interrupt and no ISR is used anywhere in the NMR path.
- **Timing primitive:** `waitUntil()` (`Sequencer.cpp:71-77`) polls `esp_timer_get_time()`. It busy-spins below 1.5 ms and calls `vTaskDelay` above that.
- **Set preparation** (`prepareHardware`, `:294-312`), once per scan set over I2C:
  - Si5351 CLK0 = 50 MHz with a PLLA reset.
  - `setLo` (`:314-321`): CLK1 = 4·f_lo, PLLB reset, Johnson-counter /CLR pulse through two TCA9535 I2C writes (`Tca9535.cpp:117-122`).
  - DDS: RESET, FREQ0, PHASE0 = 0, PHASE1 = 180, RESET cleared.
- **One scan** (`runOneScan`, `:386-500`):
  1. Optional polarize: digitalWrite FET/H-bridge, then `waitMs`.
  2. Phase: `dds.setPhase(0, φ)` and `setPhase(1, φ+90)`, PSEL = 0. Each is an SPI write that takes the SPI mutex (`:414-416`).
  3. RX_BLANK low, wait `t_blank_pre_us` (`:419-420`).
  4. Pulse: `on = esp_timer_get_time(); digitalWrite(TX_EN, HIGH); waitUntil(on + t90); digitalWrite(TX_EN, LOW)` (`:427-430`). This is a software-timed gate.
  5. Echo only: `waitUntil(pulseEnd + tau)`, then `dds.selectPhase(true)` (SPI write under the mutex), then the 180° gate (`:436-451`).
  6. Wait `t_dead_us`, unblank (`:456-457`). Wait `t_acq_start_us`, take `spibus::Guard g(200)` (`:472`), then `adc.burst()` (`:478`).
  7. Blank again, `readFlags()` (`:489-490`). This is a no-op on v0.7 because `EXP_BIT_IFLAG/TFLAG = -1` (`pins.h:96-97`).
- **ADC pacing** (`Ads8688.cpp:163-225`):
  - Reprograms AUTO_SEQ (an extra SPI transaction), opens one Arduino `SPI.beginTransaction`, sends AUTO_RST.
  - Per sample: `paceUntil(t0 + s·period)` busy-poll, then per channel CS low by register write, blocking `SPI.transfer32(NO_OP)`, CS high, flip the top bit, store to PSRAM.
  - If it falls behind it catches up back-to-back with no overrun flag. It reports only the average `achieved_hz` (`:220-223`).
- **Buffers:** `heap_caps_malloc(MALLOC_CAP_SPIRAM)` at `config` time, grow-only (`Sequencer.cpp:95-155`). Defaults: raw 800 kB, frame 400 kB, FFT 2×64 kB. Nothing is allocated per scan in the sequencer.
- **DSP per scan** (`processScan`, `:502-539`, in `Dsp.cpp`):
  - DC offset: mean of the last 10 % of the raw record if `t_acq_ms ≥ 1000`, else the whole record (`Dsp.cpp:36-40`).
  - Boxcar decimation, which also does code-to-volts (`:42-61`).
  - **No digital mixing:** the record stays at the IF.
  - CYCLOPS: rotate by −φ (`:63-74`), φ = 90·(s mod 4) (`Sequencer.cpp:573`).
  - Running mean in the frame payload under `frameLock_` (`:522-531`).
  - Status-only spectrum `dsp::analyse` (`:536`): first ≤16384 samples, Hann window, radix-2 FFT, parabolic peak, median/1.17741 noise (`Dsp.cpp:137-212`).
- **Network:**
  - The sequencer sets `recordReady_`. The main loop's `NmrBlock::loop` calls `consumeRecord` → `ws.binaryAll` (`NmrBlock.cpp:86-91`, `main.cpp:74-78`). That is the whole averaged record, a 28-byte header plus n×8 bytes (400 kB at defaults), after **every** scan.
  - JSON status goes out at 20 Hz. `get_record` resends the last record.
  - Header `block_id` is hard-coded to 9 (`Sequencer.cpp:547`); nothing reads it.

### 2. ADS8688 driver compared with the docs
**What the code does:** Arduino `SPI` (the global SPIClass on SPI2), `SPISettings(17 MHz, MSBFIRST, SPI_MODE1)` (`Ads8688.cpp:34, 181`), one 32-bit `transfer32` per conversion, CS by GPIO register writes (`:40-41`).

**Discrepancies:**

| # | Doc / comment (quote) | Code |
|---|---|---|
| 1 | NMR-FIRMWARE.md:139 "Uses `spi_device_polling_transmit` on a dedicated device handle at 17 MHz" | Arduino `SPI.transfer32` on the shared global object, no ESP-IDF device handle (`Ads8688.cpp:181-200`). |
| 2 | NMR-FIRMWARE.md:133 `begin()` "…NO_OP, AUTO_RST" | `begin` sends 2×NO_OP, 8 range writes and an AUTO_SEQ write. No AUTO_RST; that happens in `burst` (`:94-106, 185`). |
| 3 | NMR-FIRMWARE.md:138 "as fast as the loop allows … returns samples per channel achieved" | Paced to `rate_hz` and always returns `n_per_channel` (`:176, 225`). |
| 4 | Ads8688.h:33 "17 MHz" | **[est]** The ESP32 APB divider gives 80/5 = **16 MHz** effective. |
| 5 | NMR-FIRMWARE.md:192 "busy-waits inside a `portDISABLE_INTERRUPTS()`-free critical region no longer than the pulse itself" | There is no critical region at all. The task spins for the whole acquisition (up to 4 s) at priority 5. |
| 6 | NMR-FIRMWARE.md:179 "priority high"; Sequencer.h:13-14 "at priority 5 on core 1 nothing here gets preempted except by interrupts" | False. sdkconfig has `CONFIG_ARDUINO_EVENT_RUNNING_CORE 1` (arduino_events task, priority about 19). AsyncTCP is **priority 10, unpinned** (`AsyncTCP.h:37,50`). Both can preempt it. |
| 7 | Sequencer.cpp:410-411 "the PSEL bit, about 2 us of SPI" | The write first takes the SPI mutex (`Ad9834.cpp:41`, Guard 50 ms), then beginTransaction and digitalWrite FSYNC. **[est]** 5–15 µs normally, up to 50 ms worst case. |
| 8 | Sequencer.cpp:571 "the app offers multiples of four" (n_avg) | `nmr.js:77` uses `step="1"`. |
| 9 | Si5351.cpp:33-35 "well under a millihertz" | **[analysis]** About 9 mHz at CLK1 and about 2.2 mHz at the LO (23.8 Hz PLL step / (668·4) / 4). Harmless, but the comment is wrong. |
| 10 | Ad9834.cpp:82 sets PHASE1 = 90; Sequencer.cpp:309 sets 180; NMR-FIRMWARE §3.7 says 180, then each scan overwrites it with φ+90 | Three different claims. |

**Per-sample cost and maximum rate [est]:**
- One 32-bit frame at 16 MHz is 2.0 µs. Arduino `spiTransferLongNL` overhead is about 0.5–1 µs. So one channel costs about 2.7–3.2 µs and a pair about 6–7 µs, plus about 0.3 µs for `esp_timer_get_time`.
- That gives roughly **140–165 kS/s per channel sustained** for 2 channels.
- The default 100 kS/s (10 µs budget) is achievable. The accepted maximum of 250 kS/s (`Sequencer.cpp:176`) is not.
- When the rate is not met, the header still says `cfg.rate_hz/decim` (`Sequencer.h:81`, `:551`), so **the frequency axis is silently scaled wrong**. Only the status `rate_hz` shows the achieved value.
- **VERIFY:** CS-high time between back-to-back frames is only tens of ns (`:197-199`). Check this against the ADS8688 CS-high/conversion timing and the AUTO_RST data latency.

### 3. AD9834 and Si5351
- **AD9834** (`Ad9834.cpp`):
  - SPI mode 2 at 10 MHz (exact: 80/8), FSYNC through digitalWrite. Every word takes the mutex and opens its own transaction.
  - Control word: B28 = 1, PIN/SW = 0.
  - Frequency: control word, then FREQ0 LSB `0x4000|w&0x3FFF`, then MSB (`:85-107`). Resolution 50 MHz/2²⁸ = **0.186 Hz/LSB**.
  - Phase: `0xC000/0xE000 | round(deg·4096/360)`, which is **0.088°/LSB** (`:109-120`).
  - PSEL is a control-word bit (`:127-130`).
  - **Phase switching inside a sequence works** with 2 registers via PSEL: echo already does it, and CYCLOPS sets the phase per scan. It costs one locked SPI write each time.
  - Faster, deterministic switching would need PIN/SW = 1 and GPIO42 (`PIN_DDS_PSEL`). That loses the RESET and SLEEP bits, because those pins are tied low.
- **Si5351** (`Si5351.cpp`), 25 MHz crystal, I2C at 400 kHz:
  - CLK0: integer search gives PLLA 900 MHz / 18 = 50 MHz exact (`:147-161`).
  - CLK1: R-divider doubling up to ≥1 MHz, largest even integer MS, fractional PLLB with denominator 1048575 (`:197-238`).
  - The register encoding matches AN619. Control bytes 0x4F / 0x6F are correct.
  - Bug: if `setClk1` fails (f_lo below about 1.3 kHz), `config` does not check the return value. `actualLo()` then falls back to the *requested* value (`NmrBlock.cpp:39-42, 160`).
- **Core coherence problem [analysis, high impact]:**
  - TX (DDS on PLLA) and LO (PLLB, plus a Johnson counter cleared by I2C once per set) free-run at different frequencies. The demodulated IF phase is therefore set by **absolute time** modulo 1/f_IF (185 µs).
  - Nothing ties the pulse or the first ADC sample to that beat. Scan start comes from `vTaskDelay`/`esp_timer` on the ESP32's own crystal.
  - **Each scan's record should carry a random phase, so coherent averaging and CYCLOPS cannot work on hardware.**
  - SIM hides this: `simBurst` starts every record at phase φ_pulse (`Sequencer.cpp:627-629`).
  - The redesign needs a hardware-synchronised start (gate triggered from a TX/LO-derived edge, a reference channel, or a homodyne LO). **VERIFY first on hardware.**

### 4. Hard-real-time hazards
- **Software TX gate:** if anything preempts between `digitalWrite HIGH` and `LOW`, the transmitter stays on for the length of that preemption, possibly milliseconds. AsyncTCP (prio 10), arduino_events (core 1) and ISRs can all do this. I_FLAG/T_FLAG are not connected, so nothing catches an over-pulse. The bench `pulse` command does the same on the **loop task at prio 1** (`NmrBlock.cpp:189-191`, `Sequencer.cpp:343-357`), which is worse.
- **Mutex waits inside the sequence:** `spibus::Guard(200)` at acquisition start (`:472`), and the DDS Guard(50) inside tau for echo. b1's `readAll(0)` can hold the bus when the sequencer preempts it. **[est]** About 50–100 µs delay with priority inheritance.
- **Sample-time jitter:** FreeRTOS tick (1 kHz) and other ISRs on core 1, plus higher-priority tasks. Late samples are taken back-to-back with no detection. Flash writes stall both cores' cache: LittleFS alarm saves (`AlarmEngine.cpp:143`) and OTA, neither refused during a scan.
- **No heap, logging, JSON, String or WiFi** in the pulse or acquisition path. The 400 kB `ws.binaryAll` copy runs under `frameLock_` on the main task. `processScan` waits on it with `portMAX_DELAY` (`:522`), which jitters the repetition time but not the pulse.
- **Loop task starved:** the loop task shares core 1 and is blocked for the whole 2–4 s spin, so the status broadcast stalls every scan. The CPU1 idle task is not watchdogged (sdkconfig only checks CPU0), so it does not crash.
- **b1 `set_range`** is not refused during a scan. It can change the AI7/AI8 scaling between scans of an average (`InputsBlock.cpp:57-69`).
- **Validation gaps:** `t_acq_start_us < t_dead_us` is accepted (sampling while still blanked). `status()` reads `spec_` non-atomically.

### 5. Host side
- **instrument.py:**
  - `nmr` (`:323-369`): sends only f_tx, f_lo, sequence, n_avg; polls status; calls `get_record`.
  - **Bug:** `next_binary` (`:104-118`) returns the *oldest* parked kind-3 frame from the `deque(maxlen=8)` (`:82`). Per-scan frames are parked while waiting, so the CLI typically returns the **scan-1 record**, not the final average.
  - `nmr_spectrum` (`:287-302`): `np.hanning(n)` over the **entire** record (50 000 pts at defaults), no zero-padding, scale `2/sum(w)`.
  - **[est]** For T2* = 0.4 s over 2 s, a full Hann window gives about **10 dB less SNR** than a matched exponential (relative 22 vs 71). Firmware and JS window only the first 0.655 s and lose about 3 dB.
  - `nmr_peak`: 2 Hz guard, `skip = n/100`, `/1.177`, no interpolation. CSV time starts at the first sample, not the pulse.
- **nmr.js:** its own radix-2 FFT (`:266-306`), Hann with `(nUse-1)` (`:321`), `MAX_FFT = 16384` (`:37`), scale `2/wsum` (`:329`), guard `max(2, 2/df)` bins, `skip = nfft/100`, `/1.177`. It exposes only 8 settings, with defaults duplicated (`:26-35`).
- **Three different spectrum implementations:**
  - Firmware: periodic Hann, `1/(sumW·√2)` scaling (V rms per quadrature), skirt `nfft/64+4`, parabolic peak.
  - JS: symmetric Hann, `2/sum`, 16k points.
  - Python: symmetric Hann, `2/sum`, full record.
  - `2/sum` is the real-signal convention, so peak amplitudes differ by 2√2 (9 dB) between the board and the clients, and SNRs differ because of the window lengths.
- **Four simulators:** `simBurst`, the `Ads8688` SIM burst (`:204-211`), `dev-nmr.html:39-105`, and the doc.

### 6. Hard-coded constants (value, file:line; duplicates noted)

| Constant | Value | Locations |
|---|---|---|
| f_tx | 89400 Hz | Sequencer.h:35, :53 (sim_larmor); instrument.py:230; nmr.js:27; dev-nmr.html:45; NMR-FIRMWARE.md:169; PROTOCOL.md |
| f_lo | 84000 Hz | Sequencer.h:36; instrument.py:230; nmr.js:28; dev-nmr.html:45 |
| IF | 5400 Hz | Ads8688.cpp:209 (sim); dev-nmr.html:39; Dsp.h:39 (comment) |
| t90 / t180 | 417 / 834 µs | Sequencer.h:38-39; nmr.js:30, :91 |
| tau | 20 000 µs | Sequencer.h:40 |
| t_blank_pre / t_dead / t_acq_start | 20 / 1000 / 1200 µs | Sequencer.h:41-43 |
| t_acq | 2000 ms (max 4000) | Sequencer.h:44, Sequencer.cpp:175; nmr.js:31, :76 (min 10 vs firmware min 1) |
| rate_hz | 100 k (limits 1k–250k) | Sequencer.h:45, Sequencer.cpp:176 |
| decim | 4 (1–1024) | Sequencer.h:46, Sequencer.cpp:177 |
| n_avg | 1 (≤256) | Sequencer.h:47, Sequencer.cpp:178; instrument.py:325; nmr.js:77 |
| t_repeat | 3000 ms | Sequencer.h:49 |
| polarize settle | 5 ms | Sequencer.h:51 |
| pulse cmd | 1–5000 µs | NmrBlock.cpp:187; nmr.js:91 |
| Buffer caps | 1e6/ch; 131072; FFT 16384; margin 512 kB; chunk 256 | Sequencer.cpp:39-45; nmr.js:37 (16384 duplicated) |
| Task | prio 5, core 1, stack 12288 | Sequencer.cpp:47-49 |
| DDS MCLK | 50 MHz | Sequencer.cpp:298; NmrBlock.cpp:70-71; Ad9834.h:34, :53; Ad9834.cpp:61 |
| SPI clocks | ADC 17 MHz; DDS 10 MHz | Ads8688.cpp:34; Ad9834.cpp:24 |
| I2C / crystal | 400 kHz; 25 MHz | Si5351.cpp:43; Si5351.h:37 |
| VCO / denominator | 600–900 MHz; 1048575 | Si5351.cpp:30-36 |
| ADC ranges | AIN2/3 = ±5.12 V | Ads8688.h:68 |
| Sim signal | 18 mV, T2* 0.4 s, 3 mV noise, 2 mV 50 Hz hum, offsets 5/−4 mV | Sequencer.cpp:57-63; dev-nmr.html:41-42; Ads8688.cpp:205 (0.4 again) |
| Rayleigh median | 1.17741 | Dsp.cpp:19; instrument.py:318; nmr.js:361 |
| DC guard | 2 bins / 2 Hz / max(2, 2/df) bins | Dsp.cpp:23; instrument.py:305; nmr.js:342 |
| DC tail | last 10 % if ≥1000 ms | Dsp.cpp:36-40 |
| Status rate | 50 ms | main.cpp:52 |

### 7. Build and tests
- `pio run -e esp32s3-sim` **succeeded**, 0 warnings: RAM 15.6 % (51 208 / 327 680 B), Flash 14.4 % (941 193 / 6 553 600 B).
- I also built `esp32s3` (hardware): succeeded, 0 warnings, RAM 15.7 % (51 600 B), Flash 14.8 % (966 721 B).
- Platform espressif32 7.1.2, Arduino core 2.0.17.
- CI (`.github/workflows/build.yml`): build both envs, `buildfs`, and `ruff check` / `format --check` on `host/`. **There are no unit tests.** No PC test for `Dsp.cpp` exists, even though `Dsp.h:5-7` says it was designed for one. Nothing has run on hardware.

### 8. Classification and known problems

| Feature | Verdict | Why |
|---|---|---|
| Dsp decimate/rotate/mean/FFT (pure functions) | RETAIN + test | Clean and allocation-free; needs PC unit tests and a matched or half window. |
| Grow-only PSRAM buffers at config | RETAIN | Sound. |
| Record-in-frame, main-task send | RETAIN, change cadence | Per-scan 400 kB broadcast is heavy; send decimated or less often. |
| Si5351 driver | RETAIN / VERIFY | AN619-correct; check the setClk1 return; VERIFY on the board. |
| AD9834 driver | RETAIN / VERIFY | Correct sequences; take the mutex out of the timed path; consider PSEL by GPIO. |
| Software TX gate (digitalWrite + spin) | REIMPLEMENT | Preemption can over-pulse; use RMT/MCPWM/GPTimer hardware one-shot sequencing. |
| ADC burst (Arduino SPI, esp_timer polling) | REIMPLEMENT | Jitter, silent rate shortfall, 250k unreachable; use a hardware-timed trigger + SPI DMA (IDF `spi_device` queue), with overrun detection. |
| TX/LO/acquisition phase synchronisation | REIMPLEMENT (missing) | Averaging/CYCLOPS are incoherent without it [analysis]. |
| CYCLOPS sign, IF sign, Q-before-I order | VERIFY | Depends on mixer and counter wiring. |
| I_FLAG/T_FLAG handling | VERIFY / REIMPLEMENT | Dead code on v0.7; needs hardware wiring for TX safety. |
| SPI mutex shared with b1/b3 | REIMPLEMENT | No waits in timed sections; lock the bus for the whole set; refuse b1 `set_range` during a scan. |
| Three spectrum implementations | REMOVE duplicates | One reference (Python or firmware), with consistent scaling and window. |
| Four simulators | REMOVE duplicates | One physics model; it must include the absolute-time beat phase. |
| `pulse` command on the loop task | REIMPLEMENT | Same over-pulse hazard at priority 1. |
| instrument.py `next_binary` | REIMPLEMENT | Returns a stale record. |
| `block_id = 9` hard-coded | REMOVE / fix | Unused and inconsistent with the registry. |

Other problems with evidence: header rate ignores the achieved rate (`Sequencer.h:81`); `t_acq_start < t_dead` accepted (`Sequencer.cpp:160-219`); n_avg multiples of 4 not enforced (`nmr.js:77`); board `larmor_hz` uses the requested f_lo, not the actual (`NmrBlock.cpp:286`).

