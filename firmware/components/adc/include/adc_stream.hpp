// Hardware-paced ADS8688 acquisition (ADR-0009).
//
// The ADS8688 is configured once over SPI (ranges, AUTO_SEQ on the I and Q channels, AUTO_RST), then the same four
// bus pins are handed to the I2S peripheral in full-duplex master mode: BCLK = SCLK, WS = /CS (low for the 32-bit
// left slot, high for the right slot), DOUT = SDI (zeros = NO_OP keeps the auto sequence running), DIN = SDO.
// One conversion per I2S frame at a crystal-derived rate (160 MHz / 10 = 16 MHz SCLK, 64 bits per frame =
// 250 kframes/s = 125 kS/s per channel), moved by DMA. The CPU only copies finished DMA buffers into a preallocated
// buffer; nothing paces samples in software. DMA queue overflows are counted, never silent.
#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace fw {

struct AdcPins { int sclk, mosi, miso, cs; };

struct AdcConfig {
    AdcPins pins;
    uint32_t frame_rate;     // I2S frames per second = conversions per second (all channels)
    uint8_t ch_i, ch_q;      // ADS8688 channel numbers (I and Q)
    uint8_t range_code;      // ADS8688 range register value (1 = +-1.25 Vref = +-5.12 V)
    size_t capacity_frames;  // preallocated (PSRAM) buffer, frames
};

class AdcStream {
public:
    esp_err_t init(const AdcConfig& c);
    // Program the ADC over SPI (bus owned by SPI during the call, then released).
    esp_err_t configure_adc();
    // Start I2S streaming into the buffer from frame 0. The first WS falling edge after start is frame 0's /CS edge.
    esp_err_t start();
    // Copy DMA data until `frames` frames have been taken or the timeout passes. Returns frames taken.
    size_t collect(size_t frames, uint32_t timeout_ms);
    esp_err_t stop();
    // Raw 16-bit codes in conversion order (the auto sequence alternates I/Q; see parity in the docs).
    const int16_t* data() const { return buf_; }
    size_t frames() const { return taken_; }
    uint32_t overflows() const { return ovf_; }
    double frame_period_s() const { return 1.0 / cfg_.frame_rate; }
    void on_overflow() { ovf_ = ovf_ + 1; }   // not ++ (deprecated on a volatile in C++20; ISR diagnostic counter)
private:
    AdcConfig cfg_{};
    void* tx_ = nullptr;
    void* rx_ = nullptr;
    int16_t* buf_ = nullptr;   // capacity_frames entries
    uint32_t* dma_tmp_ = nullptr;
    size_t taken_ = 0;
    volatile uint32_t ovf_ = 0;
    bool running_ = false;
};

}  // namespace fw
