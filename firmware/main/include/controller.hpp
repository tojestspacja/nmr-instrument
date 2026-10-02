// Instrument control plane: the one place that sequences RF setup, gate waveforms, acquisition and DSP for an
// experiment. It owns the state machine and the shared buses; the components (engine/adc/rf) and the shared pulse/DSP
// core do the work. Everything outside a running scan is control plane (SPI/I2C free to program the DDS and clock);
// inside a scan the gate engine and the I2S ADC own the pins and the CPU only waits and copies finished buffers.
//
// Safety is structural: the gate pins idle at their safe levels (TX off, receiver blanked), every failure drives them
// back there and latches Fault, and buffers are preallocated with explicit capacities so a scan never allocates and an
// over-long or lossy record is reported, never silently truncated.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "esp_err.h"

#include "adc_stream.hpp"
#include "engine.hpp"
#include "nmr/dsp.hpp"
#include "nmr/pipeline.hpp"
#include "pulse/backend.hpp"
#include "pulse/program.hpp"

namespace fw {

// Pin and channel map + derived rates, filled from generated/instrument_config.hpp in main.cpp so this unit does not
// depend on the generated header directly (and can be built against a test map).
struct Hardware {
    // gate engine
    int pin_tx_gate, pin_rx_open, pin_phase_sel;
    int pin_dds_sign, pin_lo_i;   // ECO-1 reference captures (-1 if unwired)
    int pin_capture_sync;         // firmware-internal common timebase reset
    // shared SPI bus (ADS8688 during a scan, AD9834 between scans)
    int pin_spi_sclk, pin_spi_mosi, pin_spi_miso, pin_adc_cs, pin_dds_fsync;
    // Si5351 I2C
    int pin_i2c_sda, pin_i2c_scl;
    uint8_t si5351_addr;
    // ADS8688 channels / range
    uint8_t adc_ch_i, adc_ch_q, adc_range_code;

    double tick_hz;               // pulse-program tick (must match engine)
    double raw_rate_hz;           // complex samples/s per channel before decimation
    double adc_full_scale_v;
    int adc_bits;
    size_t decimation;
    double f_if_hz;               // f_tx - f_lo
    double tx_freq_hz, lo_freq_hz, dds_mclk_hz, lo_xtal_hz;
    double repetition_s;          // scan start to scan start (>= T1)
    double gate_timeout_ms;       // RMT wait timeout per scan
};

enum class State : uint8_t { Idle, Configured, Armed, Running, Complete, Aborting, Fault };
const char* state_name(State s);

// Allowed state transitions. Pure and side-effect free so the table can be unit-tested without hardware.
bool transition_allowed(State from, State to);

// Per-scan acquisition outcome. A record is complete only if the expected frame count arrived with no DMA overflow and
// no capture-edge overflow; otherwise the record is flagged and never fed to the average as if it were whole.
struct ScanReport {
    uint32_t scan = 0;
    size_t frames_expected = 0;
    size_t frames_taken = 0;
    uint32_t adc_overflows = 0;
    size_t capture_edges = 0;
    bool capture_overflowed = false;
    double actual_lo_hz = 0;      // Si5351 CLK1/4 actually synthesised (first scan)
    bool complete = false;
};

struct RunReport {
    size_t scans_planned = 0;
    size_t scans_complete = 0;
    std::vector<ScanReport> scans;
    bool averaged = false;
    bool aligned = true;          // pipeline: every scan had the same sample timing vs excitation
    nmr::dsp::Peak peak{};        // peak of the averaged, windowed spectrum
    double out_fs_hz = 0;
};

class Controller {
public:
    // Allocate the preallocated record pool (PSRAM) and bring the gate pins to safe levels. On any failure the pins are
    // driven safe and the state latches Fault.
    esp_err_t init(const Hardware& hw, int max_scans, size_t max_frames_per_scan);

    // Decode/validate a program against the hardware limits and confirm it fits the preallocated capacities.
    esp_err_t configure(const pulse::Program& prog);

    // Control-plane RF: clock (Si5351) then DDS. Bus is owned here, between scans only.
    esp_err_t arm();

    // Run every scan: plan -> load gates -> program per-scan DDS phase -> acquire -> copy -> average -> spectrum/peak.
    // Stops and reports on the first unrecoverable error; incomplete records are flagged, not hidden.
    esp_err_t run(RunReport& out);

    // Force safe outputs immediately (TX off, receiver blanked, ADC stopped) and return to Idle. Safe to call anytime.
    void abort();

    State state() const { return state_; }
    esp_err_t last_error() const { return last_err_; }
    const char* last_where() const { return where_; }

private:
    bool set_state(State to);          // guarded; false (and Fault) on an illegal transition
    void safe_outputs();               // drive the three gate pins to their safe levels via GPIO
    esp_err_t fault(const char* where, esp_err_t e);   // safe outputs + latch Fault
    esp_err_t run_scan(size_t k, ScanReport& sr);

    Hardware hw_{};
    GateEngine engine_;
    AdcStream adc_;

    std::vector<pulse::Event> events_;
    size_t n_scans_ = 0;
    bool components_up_ = false;

    // preallocated per-scan de-interleaved I/Q (PSRAM), [max_scans][max_frames/2]
    int max_scans_ = 0;
    size_t cap_samples_ = 0;        // complex samples per scan a record may hold
    int16_t* pool_i_ = nullptr;
    int16_t* pool_q_ = nullptr;
    std::vector<nmr::RecordView> records_;

    double actual_lo_hz_ = 0;       // Si5351 CLK1/4 actually synthesised, set in arm()

    State state_ = State::Idle;
    esp_err_t last_err_ = ESP_OK;
    const char* where_ = "";
};

}  // namespace fw
