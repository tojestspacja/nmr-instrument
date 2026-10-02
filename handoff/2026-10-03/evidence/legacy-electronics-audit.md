# Historical Electronics Agent Handback

> Evidence class: TRANSCRIPT_REPORTED. The previous agent's report follows verbatim, decoded from the uploaded session record. It was not rerun or independently certified during handoff preparation. Estimates, uncertainties and unresolved issues remain in force; later source evidence may supersede it.

Source: `Pasted text.txt`, original line 4406. SHA-256: `c4f56a1a92747fb1d7b77dd57802df4431ef472958be1cf2dc8c7db71106579d`.

---

# Electronics audit: class-board-2026 (branch w1-yi-tsai, HEAD da56d2b)

I changed nothing in the repo; `git status` is clean. Most connectivity below comes from the netlist exported from the current schematic. Where the generator scripts or docs disagree with it, the netlist wins and the conflict is listed under §7.

## 1. ERC, DRC and netlist (KiCad 10)
No library variable was needed: the lib tables use `${KIPRJMOD}`. All runs succeeded.

| Run | Errors | Warnings | Detail |
|---|---|---|---|
| ERC main | 0 | 2 | Two `pin_to_pin` warnings: J13 (Unspecified pin type) to R933 / GND. Cosmetic. |
| ERC front panel | 0 | 0 | |
| DRC main | 0 violations, 0 unconnected | 21 parity warnings | 15 "Exclude from BOM differs" (FID1-3, H1-4, J1, J2, J6, J7, J8, J13, R811, R812) and 6 "DNP differs" (J1, J2, J6, J7, J8, J13). |
| DRC front panel | 0, 0 | 2 | LCSC field missing on J411 and J412. |

The project settings turn off several checks:
- **ERC:** global label used only once, four-way junctions, SPICE model, footprint filters.
- **DRC:** missing courtyard, track not centred on via, tuning profiles, footprint filters, component type vs pads.

The board is 4-layer (In1/In2 are "power" layers), routed (2634 segments), and has a single GND zone on all four layers.

Files written to `C:\Users\Liang\AppData\Local\Temp\claude\audit-electronics\`:
- `erc-main.json`, `erc-main.rpt`, `erc-front.json`, `erc-front.rpt`
- `drc-main.json`, `drc-main.rpt`, `drc-front.json`, `drc-front.rpt`
- `class-board.net` (KiCad format), `class-board.xml`, `front-panel.xml`

## 2. TX chain (sheet `/nmr_tx/`; generator `hardware/scripts/sheet_nmr_tx.py`)
- **DDS:** U801 AD9834BRUZ.
  - MCLK is the Si5351 CLK0 at 50 MHz through R703 33 Ω.
  - R801 6.8k sets 3.18 mA full scale. R802/R803 200 Ω loads give 0.636 Vpp. C807 20 pF.
  - FSYNC = GPIO41, PSELECT = GPIO42, SDATA/SCLK on the shared SPI bus.
  - **RESET and SLEEP only have 10k pull-downs (R817/R818) and are not driven by anything.** Firmware must reset the part through its control register.
- **Reconstruction filter:** 3rd-order Butterworth at 3 MHz: C808 390 pF, L801 15 µH, C809 130 pF. C810 1 µF couples into TX_IN.
- **Bias:** R820 47k from TX_VMID. The 2026-10-01 fix is in the netlist.
- **Power amp:** U802 OPA564AIDWPR on a single supply, +VEXT_TX through FB802; V− = GND.
  - Gain: R807 6.49k / R808 270 Ω gives G = 25 when header J10 1-2 is fitted. With no shunt, G = 1.
  - R808 returns to TX_VMID. C813 was removed, and R805/R806 are now 10k (they were 100k).
  - ISET: R809 11k, so I_LIM = 1.5 A.
  - E/S = TX_EN on GPIO40, with R810 10k pull-down.
  - **I_FLAG and T_FLAG are unconnected**: they end at the DNP pull-ups R811/R812. `pins.h:96-97` sets both to −1.
- **Output path:** TX_A goes to D801/D802 SS54 rail clamps, the R814 10 Ω + C817 10 nF snubber, then **R813 4.7 Ω 1 W** and **C818 10 µF**, into header J11. J11 1-2 gives full output; 2-3 selects the −20 dB pad (R815 910 Ω / R816 100 Ω). J11 pin 2 is net TX.
- **Route to the coil:** TX goes to link J6 pins 10/12, then panel J1 pins 10/12, then the TX SMA J20 and the **TX COIL terminal J32** pin 1 (pin 2 = GND).
- **Output limits:** design swing is 15.9 Vpp (±7.95 V) at +VEXT = 18 V. Current is limited to 1.5 A. At lower +VEXT (the input range goes down to 7 V) the stage will clip; I did not quantify where (VERIFY).

## 3. RX chain (sheet `/nmr_rx/`)
- **Input:** RX SMA (panel J13) goes to panel J1 pin 20, then main J6 pin 20, then net RX.
- **On the RX node:** crossed diodes D703/D704 1N4148W to GND, tank C710 1.2 nF + C711 100 pF + C712 22 pF (all fitted, 1.322 nF total), R711 1M to GND, then R710 100 Ω in series to the LNA input.
- **LNA stage 1:** U703A OPA1656 on ±12 V. R712 10k / R713 100 Ω gives G = 101; R714 1k is the alternative.
  - **Header J3: pin 1 = R712, pin 2 = LNA_FB1, pin 3 = R714. Without a shunt, stage 1 has no feedback.** See §7.
- **Blanking:** C720 10 nF and R721 10k (high-pass at 1.59 kHz) feed U704 DG419. S2 = signal and S1 = GND, so RX_BLANK low means blanked. RX_BLANK is GPIO8 with R724 10k pull-down.
- **LNA stage 2:** U703B, R722 9.1k / R723 1k gives G ≈ 10.1 via header J9 (no shunt gives G = 1). C721 100 pF.
- **Mixer:**
  - U705A unity-gain inverter (R901/R902 10k) makes S−.
  - C901-C904 10 nF couple the signal in. R906-R909 10k bias to V_MID = 1.65 V (R903/R904 divider, U705B buffer, R905 100 Ω). D905-D908 BAV99 clamp to 0 / +3V3A.
  - U901/U902 TS5A23157 form a double-balanced commutator.
  - The LO comes from Si5351 CLK1 (= 4 × f_LO) through R704/C706 into U702 74HC74, wired as a Johnson counter, giving LO_I and LO_Q. /CLR is on expander P1.4.
- **IF filtering and gain:**
  - Passive RC: R910-R913 1k with C910-C913 10 nF to V_MID, pole at 15.9 kHz.
  - Difference amplifiers U706 OPA1612: R914/R916/R918/R923 1k, R915/R917/R919/R924 20k, C914/C915 560 pF (pole 14.2 kHz). Outputs are COND_OUT1/COND_OUT2.
- **ADC channels:** COND_OUT1 goes through header J14 2-3, then R117 1k / C117 1 nF / D117 BAV99 to ±12 V, into ADS8688 **AIN_3P (pin 23, chip channel 3)**. COND_OUT2 goes through J15 and R118 into **AIN_2P (chip channel 2)**. This matches `pins.h:32` (kAinOfAi). The docs' "ch 7/8" refers to panel numbering AI7/AI8.

## 4. Shared coil, T/R switching, B0 drive, polarizer, power, grounding, clocks, ADC

**Is one coil shared for TX and RX?**
- TX (J20 / J32) and RX (J13) are separate connectors and separate nets.
- **There is no T/R switch or duplexer.** DG419 blanking sits after LNA stage 1 and does not isolate the coil.
- The docs never say "one coil" or "two coils". However, the TX level plan assumes "5.6 mA into the 1419 Ω coil" (`sheet_nmr_tx.py:145`, `design-decisions.md:405`), which is the same 2.53 mH coil the RX tank is designed for (`sheet_nmr_rx.py:253`).
- `docs/students/yi-tsai/PROGRESS.md:29` lists "TX/RX on one coil (RX clamp diodes across the transmitter, scan aborts)" as an open blocker.

**What happens if one coil is wired to both TX and RX (my calculation):**
- D703/D704 then clamp the TX node to about ±1-1.3 V. The OPA564 (±7.95 V pk) drives them through R813 4.7 Ω + C818 (0.18 Ω).
- Diode current ≈ (7.95 − ~1.2) / 4.9 ≈ **1.4 A peak**, which is essentially the 1.5 A I_LIM.
- Consequences:
  - The OPA564 runs in current limit, and the flag that would report it is not wired.
  - R813 dissipates about 4-5 W during the pulse (it is a 1 W part).
  - The 1N4148W diodes carry several times their typical repetitive rating. Check the vendor datasheet; I did not.
  - Coil current falls to about 1.2 V / 1419 Ω ≈ 0.85 mA instead of 5.6 mA. That makes B1 about 6-7× weaker, so a 90° pulse would take about 3 ms instead of 417 µs.

**B0 drive (J903):**
- U903 DRV8871DDAR with VM = +VEXT (7-18 V input), IN1 = GPIO9, IN2 = GPIO14, R921/R922 10k pull-downs. C921 100 µF + C920 100 nF, D920 SMBJ26A on VM.
- R920 32k on ILIM gives I_TRIP = 64 / 32 = **2.0 A** (`requirements.md:75`).
- **No current sensing of the B0 coil exists anywhere.**
- From memory of the datasheet (fairly confident, not re-checked): the DRV8871 regulates by fixed off-time chopping (about 25 µs) once current reaches I_TRIP. That is a current limit, not a settable closed-loop current source.
- The operating point is about 1.50 A. That figure comes from the student simulator's 59.7 kHz/A coil calibration, not a measurement. It is below I_TRIP, so B0 is effectively voltage/PWM-driven and drifts with coil resistance. PROGRESS.md reports 138 Hz/min.

**Polarizer (J905):**
- Pin 1 is +VCOIL, an external supply that only reaches D930. Pin 2 is COIL, going to Q904 AOD4184A drain. Pin 3 is GND.
- Q904 source goes to ISENSE_COIL, then R933 0 Ω to GND. The J13 sense header is DNP on the PCB, and **ISENSE_COIL does not reach the ADC**.
- Gate drive: U904 UCC27517 from GPIO47 via R931 100 Ω and R932 10 Ω; R930 / R950 10k pull-downs.
- **The driver's VDD comes only from a shunt on header J12** (+VEXT or +5V_RAW). Without the shunt the driver is unpowered.
- D930 SS54 freewheel is fitted; D934 SMBJ20A is DNP.
- There are **no relays** on either board; the b4_switching sheet was removed.

**Power rails:**
- J901 → F901 5 A fuse → D931 SMBJ26A → Q901 AOD4185 reverse-polarity P-FET (D932 12 V gate clamp) → +VEXT.
- USB-C J201 and DC jack J202 → 1.5 A polyfuses → SMF5.0A TVS → LM66100 ideal-diode OR → +5V_RAW. From there:
  - AMS1117 → +3V3.
  - Two B0512S-2WR3 2 W isolated modules → ±12 V.
  - 78L05 on +12 V → +5VA (ADS8688 AVDD, DG419 VL, DAC).
  - FB901 → +3V3A (mixer and divider).

**Grounding:**
- **There is one GND net on both boards and no AGND net or NT1 net-tie.** This was a deliberate change in commit 147f9cc ("one GND net on both boards", Decision #73).
- The AD9834 AGND/DGND, the receiver, the OPA564, the H-bridge and the 13 A polarizer return all share it.

**Clocks:**
- Y701 25 MHz crystal → U701 Si5351A at I²C 0x60, powered from +3V3 rather than +3V3A.
- CLK0 = 50 MHz to the DDS. CLK1 = 336 kHz (4 × 84 kHz LO). CLK2 goes to TP701.
- The DDS and the LO share one reference.

**ADC (ADS8688):**
- SPI2 on the IO_MUX pins: SCLK GPIO12 (via R1 33 Ω), MOSI GPIO11 (via R2 33 Ω), MISO GPIO13 (via R101 33 Ω), /CS GPIO10.
- The bus is shared with the AD9834 (SPI mode 2) and the DAC8563 (via 74HCT125). The ADC uses mode 1.
- /RST is pulled up with 10k and not driven. REFSEL = GND selects the internal reference; REFIO has 10 µF.
- The input range is set in software: firmware defaults ±5.12 V, code 1 (`NMR-FIRMWARE.md:147`).
- Sustained rate:
  - The ADS8688 tops out at 500 kSPS aggregate (from memory of the datasheet). Two channels at 250 kS/s would be the absolute limit.
  - Firmware targets 100 kS/s per channel with per-sample polling at 17 MHz (`NMR-FIRMWARE.md:139-147`). Per-transaction overhead on the ESP32 will likely set the real ceiling; this is speculation and needs measuring.
  - The ADC's ~15 kHz input filter acts as the IF anti-alias filter.

## 5. Critical parameters

| Name | Value | Unit | Source |
|---|---|---|---|
| B0 / Larmor / IF / LO | 2.1 mT / 89.4 / 5.4 / 84.0 | – / kHz / kHz / kHz | design-decisions.md:343-346; bring-up.md:52 |
| Si5351 crystal / CLK0 / CLK1 | 25 / 50.000 / 0.336 | MHz | netlist Y701; design-decisions.md (D-33) |
| DDS resolution | 0.186 | Hz | sheet_nmr_tx.py:140 |
| R801 / I_OUT full scale / DDS output | 6.8k / 3.18 / 0.636 | Ω / mA / Vpp | sheet_nmr_tx.py:140-141 |
| Reconstruction filter C808 / L801 / C809, f_c | 390 pF / 15 µH / 130 pF, 3 MHz | – | sheet_nmr_tx.py:142 |
| OPA564 gain R807/R808 | 6.49k / 270 → G 25.0 | – | netlist; sheet_nmr_tx.py:145 |
| TX swing / I_LIM (R809 11k) | 15.9 Vpp / 1.5 A | – | sheet_nmr_tx.py:145-146 |
| R813 / C818 | 4.7 Ω 1 W / 10 µF | – | netlist |
| t90 (design) | 417 | µs | sheet_nmr_tx.py:145 |
| Bias R805 / R806 / R820 | 10k / 10k / 47k | Ω | netlist (commit 147f9cc) |
| Coil L / R_DC / X_L / Q / R_p | 2.53 mH / 6.7 Ω / 1419 Ω / 10 / 14.2 kΩ | – | sheet_nmr_rx.py:253 |
| Tank as fitted | 1.322 nF → ~87.0 kHz (my calc; 1.25 nF needed) | – | netlist C710-C712 |
| Signal at the coil | 40 µV pk | – | sheet_nmr_rx.py:253 |
| LNA gain | 101 × 10.1 | – | sheet_nmr_rx.py:334-335 |
| Interstage high-pass | 1.59 kHz (10 nF / 10k) | – | design-review.md:197 |
| IF poles | 15.9 kHz (passive), 14.2 kHz (active), 15 kHz (ADC) | – | requirements.md:73 |
| Difference-amp gain, as drawn | 20 (doc) vs **≈10 (my calc)** | – | netlist R910-R924 |
| ADC input RC | 1k / 1 nF → 159 kHz | – | requirements.md:45 |
| ADC range / target rate | ±5.12 V / 100 kS/s per channel | – | NMR-FIRMWARE.md:146-147 |
| +VEXT range / fuse / TVS | 7-18 V / 5 A / SMBJ26A (clamps ~42 V) | – | requirements.md:77; design-review.md:198 |
| OPA564 absolute max supply | 26 V | – | design-decisions.md (D-41) |
| DRV8871 R920 / I_TRIP | 32k / 2.0 A | – | requirements.md:75 |
| B0 coil calibration (simulated) | 59.7 kHz/A → 1.50 A for 89.4 kHz | – | PROGRESS.md:28 |
| Polarizer FET limits | 40 V / 13 A; +VCOIL ≤ 24 V (snubber) or ≤ 12 V (TVS) | – | requirements.md:76 |

## 6. Classification

| Feature | Class | Why |
|---|---|---|
| Si5351 + Johnson-counter quadrature LO, one reference shared with the DDS | RETAIN | Sound, coherent architecture. |
| AD9834 + 3 MHz reconstruction filter | RETAIN | Values are consistent. |
| Wire AD9834 RESET/SLEEP to GPIOs | REIMPLEMENT | Currently not driven at all. |
| OPA564 power stage | RETAIN topology, REIMPLEMENT flags | IFLAG/TFLAG are unwired; no sequencing interlock. |
| Heterodyne I/Q receiver, blanking between LNA stages | RETAIN | |
| IF difference-amplifier resistor network | VERIFY / REIMPLEMENT | Source resistance gives G ≈ 10, not 20, and the P and N legs have unequal poles. |
| Crossed diodes directly on the RX node, no T/R switch | REIMPLEMENT | Single-coil use loads TX at about 1.4 A. Needs a proper T/R network (series crossed diodes plus a quarter-wave/lumped duplexer, or an analog switch). |
| Jumper-header gain selection (J3, J9, J10) | REIMPLEMENT | J3 without a shunt leaves the LNA open-loop. |
| Fixed 1.322 nF on-board tank | VERIFY | Off-tune as fitted (~87 kHz); tune at the coil. |
| DRV8871 voltage-mode B0 drive | REIMPLEMENT | Needs closed-loop constant current with a shunt plus ADC/feedback. |
| Polarizer AOD4184A + UCC27517 + SS54 | RETAIN, VERIFY | Make J12 a fixed connection; route the current sense to the ADC. |
| Single GND net, no AGND | VERIFY / REIMPLEMENT | Polarizer, H-bridge and TX returns share copper with a ×1000 receiver. |
| +VEXT input protection | REIMPLEMENT | The TVS clamp is above the OPA564's 26 V absolute max. |
| ADS8688 as the NMR digitizer | VERIFY | Sustained 2-channel rate on shared SPI is not demonstrated. |
| Relays / mains | REMOVE | Already absent from the design. |
| Stale documentation | REMOVE / UPDATE | See §7. |

## 7. Known problems and risks (with evidence)
1. **The LNA has no feedback unless J3 is shunted.** The netlist shows J3 pin 1 (R712) and pin 2 (LNA_FB1) on different nets. The schematic note "R712 permanent — J3 pin 1 tied to pin 2" (`sheets/nmr_rx.kicad_sch:7336`) is not reflected in the connectivity.
2. **A shared TX/RX coil drives the RX clamp diodes at about 1.4 A**, and there is no T/R switch (§4).
3. **The OPA564 I_FLAG/T_FLAG are unconnected**, so the firmware abort on current limit (`NMR-FIRMWARE.md:188`, `bring-up.md:66`) cannot work.
4. **No B0 current sensing**, and the DRV8871 only limits at 2.0 A. B0 drifts with coil temperature.
5. **Power sequencing:** VDIG must come up before V+ (`design-decisions.md` D-35). Only a silkscreen note and procedure enforce this. Also, the SMBJ26A (~42 V clamp) does not protect the OPA564 (26 V absolute max).
6. **The docs are stale versus the netlist:**
   - AGENTS.md:158, README.md:133 and design-decisions D-10/D-21 claim an AGND/NT1 split; there is one GND net.
   - D-35 claims R805/R806 100k and C813 present.
   - bring-up T-17 refers to J802/JP801, and T-18 to JP702.
   - `bring-up.md:61` channel table (AI7→4, AI8→5) contradicts the netlist and `pins.h` (AI7→3, AI8→2).
   - The TX link pins are 10/12 in the netlist, not 19/21 or 39.
7. **IF gain is roughly half the documented value** (≈10 vs 20, my calculation). The expected ~1.0 V pk at the ADC would be about 0.5 V.
8. **Tank fully stuffed at 1.322 nF** puts resonance about 2.4 kHz low (my calculation).
9. **Speculative:** the free-running DDS phase relative to the LO at pulse time may vary from shot to shot. Coherent averaging may need phase referencing. VERIFY in firmware.
10. **Minor:** U204 AMS1117 tab pad 4 has no net, and the J12 shunt is needed to power U904.

