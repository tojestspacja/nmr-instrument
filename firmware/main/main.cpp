// Firmware entry point. Builds the hardware map and a default experiment from the generated instrument config, then
// drives one experiment through the control state machine: configure -> arm -> run. Results are printed over the
// console. No flashing, energising or bench claim is made here; this is the software integration of the components.
#include <cmath>
#include <cstdio>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "controller.hpp"
#include "instrument_config.hpp"
#include "pulse/compiler.hpp"

using namespace nmr::cfg;

namespace {
const char* TAG = "main";

constexpr uint8_t kSi5351Addr = 0x60;   // Si5351A default 7-bit I2C address (VERIFY against the board)

fw::Hardware make_hardware() {
    fw::Hardware hw{};
    hw.pin_tx_gate = static_cast<int>(PINS_TX_EN);
    hw.pin_rx_open = static_cast<int>(PINS_RX_BLANK);
    hw.pin_phase_sel = static_cast<int>(PINS_DDS_PSEL);
    hw.pin_dds_sign = static_cast<int>(PINS_CAP_DDS_SIGN);
    hw.pin_lo_i = static_cast<int>(PINS_CAP_LO_I);
    hw.pin_capture_sync = static_cast<int>(PINS_CAPTURE_SYNC);
    hw.pin_spi_sclk = static_cast<int>(PINS_SPI_SCLK);
    hw.pin_spi_mosi = static_cast<int>(PINS_SPI_MOSI);
    hw.pin_spi_miso = static_cast<int>(PINS_SPI_MISO);
    hw.pin_adc_cs = static_cast<int>(PINS_ADC_CS);
    hw.pin_dds_fsync = static_cast<int>(PINS_DDS_FSYNC);
    hw.pin_i2c_sda = static_cast<int>(PINS_I2C_SDA);
    hw.pin_i2c_scl = static_cast<int>(PINS_I2C_SCL);
    hw.si5351_addr = kSi5351Addr;
    hw.adc_ch_i = static_cast<uint8_t>(RECEIVER_ADC_CHANNEL_I);
    hw.adc_ch_q = static_cast<uint8_t>(RECEIVER_ADC_CHANNEL_Q);
    hw.adc_range_code = 1;   // +-2.5 x Vref (RECEIVER_ADC_FULL_SCALE_V)
    hw.tick_hz = TIMING_TICK_HZ;
    hw.raw_rate_hz = ACQUISITION_RAW_RATE_HZ;
    hw.adc_full_scale_v = RECEIVER_ADC_FULL_SCALE_V;
    hw.adc_bits = static_cast<int>(RECEIVER_ADC_BITS);
    hw.decimation = static_cast<size_t>(ACQUISITION_DECIMATION);
    hw.f_if_hz = RF_IF_FREQUENCY_HZ;
    hw.tx_freq_hz = TX_FREQUENCY_HZ;
    hw.lo_freq_hz = LO_FREQUENCY_HZ;
    hw.dds_mclk_hz = DDS_MCLK_HZ;
    hw.lo_xtal_hz = LO_CRYSTAL_HZ;
    hw.repetition_s = ACQUISITION_REPETITION_TIME_S;
    hw.gate_timeout_ms = 1000.0;
    return hw;
}

// A default water-FID experiment straight from the config: n_avg scans, CYCLOPS phase cycle.
bool make_program(pulse::Program& prog) {
    const std::vector<pulse::Scan> scans =
        pulse::fid(TX_T90_S, ACQUISITION_START_DELAY_S, ACQUISITION_DURATION_S,
                   static_cast<int>(ACQUISITION_AVERAGES), pulse::Cycle::Cyclops);
    pulse::Timing tm{TIMING_TICK_HZ, TX_FREQUENCY_HZ, TIMING_PRE_BLANK_S, RECEIVER_DEAD_TIME_S,
                     ACQUISITION_REPETITION_TIME_S};
    std::vector<pulse::Diagnostic> diag;
    if (!pulse::compile(scans, tm, prog, diag)) {
        for (const auto& d : diag) ESP_LOGE(TAG, "compile: [%u] %s", (unsigned)d.index, d.message.c_str());
        return false;
    }
    pulse::Limits lim{};
    lim.tick_hz = TIMING_TICK_HZ;
    lim.tx_max_pulse_s = LIMITS_TX_MAX_PULSE_S;
    lim.tx_max_duty = LIMITS_TX_MAX_DUTY;
    lim.dead_time_s = RECEIVER_DEAD_TIME_S;
    lim.pre_blank_s = TIMING_PRE_BLANK_S;
    lim.tx_max_freq_hz = DDS_MAX_OUTPUT_FREQUENCY_HZ;
    lim.tx_min_freq_hz = 0;
    lim.backend_has_sync_input = false;
    const auto v = pulse::validate(prog, lim);
    for (const auto& d : v) ESP_LOGE(TAG, "validate: [%u] %s", (unsigned)d.index, d.message.c_str());
    return v.empty();
}

void report(const fw::RunReport& r) {
    printf("\n=== run report ===\n");
    printf("scans: %u planned, %u complete\n", (unsigned)r.scans_planned, (unsigned)r.scans_complete);
    for (const auto& s : r.scans)
        printf("  scan %u: %u/%u frames, %u ovf, %u edges%s%s\n", (unsigned)s.scan, (unsigned)s.frames_taken,
               (unsigned)s.frames_expected, (unsigned)s.adc_overflows, (unsigned)s.capture_edges,
               s.capture_overflowed ? " CAP-OVF" : "", s.complete ? "" : " INCOMPLETE");
    if (r.averaged) {
        printf("averaged: fs=%.1f Hz, aligned=%s\n", r.out_fs_hz, r.aligned ? "yes" : "NO");
        printf("peak: f=%.2f Hz, |S|=%.4g, SNR=%.1f\n", r.peak.freq_hz, r.peak.amplitude, r.peak.snr);
    } else {
        printf("averaged: none (no complete records)\n");
    }
    printf("NOTE: software integration only. Not bench-validated; no measured FID/echo.\n");
}

}  // namespace

extern "C" void app_main() {
    ESP_LOGI(TAG, "nmr firmware: %s %s, config %s", INSTRUMENT_NAME, INSTRUMENT_REVISION, CONFIG_SHA256);

    const fw::Hardware hw = make_hardware();
    const int max_scans = static_cast<int>(ACQUISITION_AVERAGES);
    // Preallocated per-scan capacity: full-duration record (pre-decimation complex samples) x 2 conversions + margin.
    const size_t samples = static_cast<size_t>(std::llround(ACQUISITION_DURATION_S * ACQUISITION_RAW_RATE_HZ));
    const size_t max_frames = (samples + 256) * 2;

    static fw::Controller ctrl;
    if (ctrl.init(hw, max_scans, max_frames) != ESP_OK) {
        ESP_LOGE(TAG, "init failed at %s: %s", ctrl.last_where(), esp_err_to_name(ctrl.last_error()));
        return;
    }

    pulse::Program prog;
    if (!make_program(prog)) { ESP_LOGE(TAG, "program build/validate failed"); return; }

    if (ctrl.configure(prog) != ESP_OK) {
        ESP_LOGE(TAG, "configure failed at %s: %s", ctrl.last_where(), esp_err_to_name(ctrl.last_error()));
        return;
    }
    if (ctrl.arm() != ESP_OK) {
        ESP_LOGE(TAG, "arm failed at %s: %s (Si5351 present? LO clock up?)", ctrl.last_where(),
                 esp_err_to_name(ctrl.last_error()));
        return;
    }

    fw::RunReport r;
    if (ctrl.run(r) != ESP_OK) {
        ESP_LOGE(TAG, "run failed at %s: %s", ctrl.last_where(), esp_err_to_name(ctrl.last_error()));
        return;
    }
    report(r);

    ESP_LOGI(TAG, "state: %s", fw::state_name(ctrl.state()));
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
