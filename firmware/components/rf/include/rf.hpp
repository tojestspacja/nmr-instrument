// RF sources: AD9834 DDS (transmit) and Si5351A (DDS clock CLK0, LO clock CLK1 = 4 f_LO). Control plane only:
// called between scans, never inside the timed gate/acquisition path (the SPI pins belong to the I2S ADC stream
// while a scan runs). Register sequences: AD9834 datasheet (Rev. D) and Skyworks AN619, as in the legacy drivers.
#pragma once

#include <cstdint>

#include "esp_err.h"

namespace fw {

struct DdsPins { int sclk, mosi, fsync; };

// AD9834 with PIN/SW = 1 so the PSELECT pin (driven by the gate engine) picks PHASE0/PHASE1 in real time, and
// OPBITEN = 1 with DIV2 = 1 so SIGN BIT OUT is the NCO MSB: a square wave at f_out for the beat reference (ECO-1).
esp_err_t dds_program(const DdsPins& p, double mclk_hz, double f_hz, uint32_t phase0_turn16, uint32_t phase1_turn16);

// Si5351A over I2C: CLK0 = 50 MHz (integer PLLA), CLK1 = 4 x f_lo (fractional PLLB). Returns the actual CLK1/4.
esp_err_t si5351_program(int sda, int scl, uint8_t addr, double xtal_hz, double clk0_hz, double f_lo_hz, double* actual_lo_hz);

// frequency word (28 bits) and its actual frequency
uint32_t dds_ftw(double mclk_hz, double f_hz, double* actual_hz);

}  // namespace fw
