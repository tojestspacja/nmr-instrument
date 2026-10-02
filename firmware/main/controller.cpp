#include "controller.hpp"

#include <algorithm>
#include <cmath>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "rf.hpp"

namespace fw {

static const char* TAG = "ctrl";

const char* state_name(State s) {
    switch (s) {
        case State::Idle: return "Idle";
        case State::Configured: return "Configured";
        case State::Armed: return "Armed";
        case State::Running: return "Running";
        case State::Complete: return "Complete";
        case State::Aborting: return "Aborting";
        case State::Fault: return "Fault";
    }
    return "?";
}

bool transition_allowed(State from, State to) {
    if (to == State::Fault) return true;              // any failure may fault
    if (to == State::Aborting) return true;           // abort from anywhere
    switch (from) {
        case State::Idle:       return to == State::Configured;
        case State::Configured: return to == State::Armed || to == State::Configured;  // reconfigure
        case State::Armed:      return to == State::Running || to == State::Configured; // re-arm via reconfigure
        case State::Running:    return to == State::Complete;
        case State::Complete:   return to == State::Configured || to == State::Armed;   // next experiment / re-run
        case State::Aborting:   return to == State::Idle;
        case State::Fault:      return false;          // latched: only a reset (new init) leaves Fault
    }
    return false;
}

bool Controller::set_state(State to) {
    if (!transition_allowed(state_, to)) {
        ESP_LOGE(TAG, "illegal transition %s -> %s", state_name(state_), state_name(to));
        fault("set_state", ESP_ERR_INVALID_STATE);
        return false;
    }
    state_ = to;
    return true;
}

void Controller::safe_outputs() {
    // TX off (0), receiver blanked (0), phase register 0. All-zero is the safe level for every gate pin.
    const int pins[3] = {hw_.pin_tx_gate, hw_.pin_rx_open, hw_.pin_phase_sel};
    for (int p : pins) {
        if (p < 0) continue;
        gpio_config_t gc = {};
        gc.pin_bit_mask = 1ull << p;
        gc.mode = GPIO_MODE_OUTPUT;
        gpio_config(&gc);
        gpio_set_level(static_cast<gpio_num_t>(p), 0);
    }
}

esp_err_t Controller::fault(const char* where, esp_err_t e) {
    where_ = where;
    last_err_ = e;
    adc_.stop();
    safe_outputs();
    state_ = State::Fault;
    ESP_LOGE(TAG, "FAULT at %s: %s", where, esp_err_to_name(e));
    return e;
}

esp_err_t Controller::init(const Hardware& hw, int max_scans, size_t max_frames_per_scan) {
    hw_ = hw;
    max_scans_ = max_scans;
    cap_samples_ = max_frames_per_scan / 2;   // two conversions (I and Q) per complex sample

    // Safe before any peripheral is configured, in case we faulted out of a previous run.
    safe_outputs();

    // Preallocated record pool: prefer PSRAM, fall back to internal RAM (will fail loudly if too large).
    const size_t n = static_cast<size_t>(max_scans_) * cap_samples_;
    const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    pool_i_ = static_cast<int16_t*>(heap_caps_malloc(n * sizeof(int16_t), caps));
    pool_q_ = static_cast<int16_t*>(heap_caps_malloc(n * sizeof(int16_t), caps));
    if (!pool_i_ || !pool_q_) return fault("init/pool", ESP_ERR_NO_MEM);
    records_.reserve(max_scans_);

    EngineConfig ec{};
    ec.pin_tx_gate = hw_.pin_tx_gate;
    ec.pin_rx_open = hw_.pin_rx_open;
    ec.pin_phase_sel = hw_.pin_phase_sel;
    ec.pin_dds_sign = hw_.pin_dds_sign;
    ec.pin_lo_i = hw_.pin_lo_i;
    ec.pin_ws = hw_.pin_adc_cs;             // the ADS8688 /CS is the I2S WS: its first edge marks frame 0
    ec.pin_sync = hw_.pin_capture_sync;
    ec.tick_hz = static_cast<uint32_t>(llround(hw_.tick_hz));
    esp_err_t e = engine_.init(ec);
    if (e != ESP_OK) return fault("init/engine", e);

    AdcConfig ac{};
    ac.pins = {hw_.pin_spi_sclk, hw_.pin_spi_mosi, hw_.pin_spi_miso, hw_.pin_adc_cs};
    ac.frame_rate = static_cast<uint32_t>(llround(hw_.raw_rate_hz * 2.0));   // I and Q conversions per second
    ac.ch_i = hw_.adc_ch_i;
    ac.ch_q = hw_.adc_ch_q;
    ac.range_code = hw_.adc_range_code;
    ac.capacity_frames = max_frames_per_scan;
    if ((e = adc_.init(ac)) != ESP_OK) return fault("init/adc", e);

    components_up_ = true;
    state_ = State::Idle;
    ESP_LOGI(TAG, "init ok: pool %u samples/scan x %d scans", (unsigned)cap_samples_, max_scans_);
    return ESP_OK;
}

esp_err_t Controller::configure(const pulse::Program& prog) {
    if (state_ == State::Fault) return ESP_ERR_INVALID_STATE;
    if (!components_up_) return fault("configure", ESP_ERR_INVALID_STATE);

    pulse::Diagnostic d{};
    events_.clear();
    if (!pulse::expand(prog, events_, d)) return fault("configure/expand", ESP_ERR_INVALID_ARG);
    n_scans_ = pulse::count_scans(events_);
    if (n_scans_ == 0) return fault("configure/no-scans", ESP_ERR_INVALID_ARG);
    if (static_cast<int>(n_scans_) > max_scans_) return fault("configure/too-many-scans", ESP_ERR_INVALID_SIZE);

    // Pre-arm check: every scan must plan, and its acquisition window must fit the preallocated record.
    for (size_t k = 0; k < n_scans_; ++k) {
        pulse::ScanPlan plan;
        pulse::Diagnostic pd;
        if (!pulse::plan_scan(events_, k, pulse::BackendCaps{}, plan, pd))
            return fault("configure/plan", ESP_ERR_INVALID_ARG);
        if (plan.nwin == 0) return fault("configure/no-window", ESP_ERR_INVALID_ARG);
        const double win_s = static_cast<double>(plan.win[0].end - plan.win[0].start) / hw_.tick_hz;
        const size_t samples = static_cast<size_t>(llround(win_s * hw_.raw_rate_hz));
        if (samples > cap_samples_) return fault("configure/window-too-long", ESP_ERR_INVALID_SIZE);
    }

    if (!set_state(State::Configured)) return last_err_;
    ESP_LOGI(TAG, "configured: %u scans", (unsigned)n_scans_);
    return ESP_OK;
}

esp_err_t Controller::arm() {
    if (state_ != State::Configured && state_ != State::Complete)
        return fault("arm/wrong-state", ESP_ERR_INVALID_STATE);

    // Control plane, bus free: bring up the Si5351 (CLK0 = DDS MCLK, CLK1 = 4 f_LO). A missing chip returns an error
    // from i2c_master_probe rather than running with no clock.
    actual_lo_hz_ = 0;
    esp_err_t e = si5351_program(hw_.pin_i2c_sda, hw_.pin_i2c_scl, hw_.si5351_addr, hw_.lo_xtal_hz,
                                 hw_.dds_mclk_hz, hw_.lo_freq_hz, &actual_lo_hz_);
    if (e != ESP_OK) return fault("arm/si5351", e);

    const double lo_err = std::fabs(actual_lo_hz_ - hw_.lo_freq_hz);
    if (lo_err > 1.0)   // Si5351 fractional synthesis should land within << 1 Hz; a large miss means a bad plan
        ESP_LOGW(TAG, "LO off by %.3f Hz (want %.1f, got %.3f)", lo_err, hw_.lo_freq_hz, actual_lo_hz_);

    if (!set_state(State::Armed)) return last_err_;
    ESP_LOGI(TAG, "armed: LO %.3f Hz, IF %.1f Hz", actual_lo_hz_, hw_.f_if_hz);
    return ESP_OK;
}

esp_err_t Controller::run_scan(size_t k, ScanReport& sr) {
    pulse::ScanPlan plan;
    pulse::Diagnostic pd;
    if (!pulse::plan_scan(events_, k, pulse::BackendCaps{}, plan, pd)) return fault("run/plan", ESP_ERR_INVALID_ARG);
    sr.scan = plan.scan;

    // Per-scan control plane (bus free): load this scan's DDS frequency + the two phase registers the PSELECT pin
    // chooses between in real time. Programmed before the SPI pins are handed to the I2S ADC.
    esp_err_t e = dds_program({hw_.pin_spi_sclk, hw_.pin_spi_mosi, hw_.pin_dds_fsync}, hw_.dds_mclk_hz,
                              hw_.tx_freq_hz, plan.phase_reg[0], plan.phase_reg[1]);
    if (e != ESP_OK) return fault("run/dds", e);

    if ((e = engine_.load(plan)) != ESP_OK) return fault("run/engine.load", e);

    const pulse::Window& w = plan.win[0];
    const double win_s = static_cast<double>(w.end - w.start) / hw_.tick_hz;
    sr.frames_expected = static_cast<size_t>(llround(win_s * hw_.raw_rate_hz)) * 2;   // I + Q conversions

    // Hand the SPI pins to the ADC: program the ADS8688, then start I2S streaming.
    if ((e = adc_.configure_adc()) != ESP_OK) return fault("run/adc.configure", e);
    if ((e = adc_.start()) != ESP_OK) return fault("run/adc.start", e);
    engine_.arm_single(kInWs);          // timestamp frame 0's WS edge on the common timebase

    if ((e = engine_.start()) != ESP_OK) { adc_.stop(); return fault("run/engine.start", e); }
    if ((e = engine_.wait_done(static_cast<uint32_t>(hw_.gate_timeout_ms))) != ESP_OK) {
        adc_.stop();
        return fault("run/wait_done", e);
    }

    const uint32_t acq_timeout = static_cast<uint32_t>(win_s * 1000.0 * 2.0 + 50.0);
    const size_t got = adc_.collect(sr.frames_expected, acq_timeout);
    adc_.stop();

    sr.frames_taken = got;
    sr.adc_overflows = adc_.overflows();
    sr.capture_edges = engine_.edge_count();
    sr.capture_overflowed = engine_.edges_overflowed();
    sr.actual_lo_hz = (k == 0) ? actual_lo_hz_ : 0;
    sr.complete = got >= sr.frames_expected && sr.adc_overflows == 0 && !sr.capture_overflowed;

    if (!sr.complete) {
        ESP_LOGW(TAG, "scan %u incomplete: %u/%u frames, %u ovf, edges%s", (unsigned)sr.scan, (unsigned)got,
                 (unsigned)sr.frames_expected, (unsigned)sr.adc_overflows, sr.capture_overflowed ? " OVERFLOW" : "");
        return ESP_OK;   // reported, not hidden; the average simply excludes this record
    }

    // De-interleave the frame stream into this scan's slot. ADS8688 AUTO_SEQ delivers channels in ascending order, so
    // with ch_q < ch_i the earlier conversion of each pair is Q and the later is I. The two are therefore sampled half
    // a frame period apart (~3.8 us at 125 kS/s) -> a fixed I/Q aperture skew. Correcting it belongs in the shared
    // pipeline (FW-IQ-001), applied once; it is NOT applied here, and no physical I/Q alignment is claimed.
    const bool q_first = hw_.adc_ch_q < hw_.adc_ch_i;
    const size_t n = std::min(got / 2, cap_samples_);
    int16_t* ip = pool_i_ + k * cap_samples_;
    int16_t* qp = pool_q_ + k * cap_samples_;
    const int16_t* buf = adc_.data();
    for (size_t m = 0; m < n; ++m) {
        const int16_t a = buf[2 * m], b = buf[2 * m + 1];
        qp[m] = q_first ? a : b;
        ip[m] = q_first ? b : a;
    }

    nmr::RecordView rv;
    rv.scan = plan.scan;
    rv.window = w.id;
    // Nominal timing from the plan (program clock). The measured WS/TX capture edges (engine_.edges()) are available
    // for a later measured-timestamp alignment; this build uses the planned times and assumes a coherent beat.
    rv.t_first = static_cast<double>(w.start) / hw_.tick_hz;
    rv.t_excitation = plan.has_excitation ? static_cast<double>(plan.excitation) / hw_.tick_hz : 0.0;
    rv.rx_phase_turns = static_cast<double>(plan.rx_phase) / 65536.0;
    rv.beat_phase = 0.0;
    rv.f_if = hw_.f_if_hz;
    rv.i = ip;
    rv.q = qp;
    rv.n = n;
    records_.push_back(rv);
    return ESP_OK;
}

esp_err_t Controller::run(RunReport& out) {
    if (state_ != State::Armed) return fault("run/wrong-state", ESP_ERR_INVALID_STATE);
    if (!set_state(State::Running)) return last_err_;

    out = RunReport{};
    out.scans_planned = n_scans_;
    out.scans.reserve(n_scans_);
    records_.clear();

    const TickType_t rep_ticks = pdMS_TO_TICKS(static_cast<uint32_t>(hw_.repetition_s * 1000.0));
    for (size_t k = 0; k < n_scans_; ++k) {
        const TickType_t t0 = xTaskGetTickCount();
        ScanReport sr;
        const esp_err_t e = run_scan(k, sr);
        if (e != ESP_OK) return e;       // run_scan already faulted + drove safe
        out.scans.push_back(sr);
        if (sr.complete) ++out.scans_complete;
        if (k + 1 < n_scans_) {          // T1 recovery before the next scan
            const TickType_t elapsed = xTaskGetTickCount() - t0;
            if (elapsed < rep_ticks) vTaskDelay(rep_ticks - elapsed);
        }
    }

    safe_outputs();   // acquisition finished: receiver blanked, TX off

    if (!records_.empty()) {
        nmr::PipelineConfig pc;
        pc.adc_rate = hw_.raw_rate_hz;
        pc.adc_full_scale = hw_.adc_full_scale_v;
        pc.adc_bits = hw_.adc_bits;
        pc.decimation = hw_.decimation;
        pc.fir_taps = 63;
        pc.fir_cutoff_hz = 0;      // 0.4 x output rate
        pc.offset_tail = 0.1;
        pc.correct_beat = true;
        // FW-IQ-001: the ADS8688 auto-sequence converts Q then I one conversion period apart (half a raw sample). The
        // later channel is sampled +0.5 raw samples after the earlier; realign it. Sign from the channel order. The
        // magnitude assumes one conversion per channel per frame (ADR-0009); VERIFY the aperture against the bus timing
        // at bring-up (no physical I/Q alignment is claimed from a build).
        pc.iq_skew_samples = (hw_.adc_ch_i > hw_.adc_ch_q) ? 0.5 : -0.5;
        const nmr::Processed proc = nmr::process(records_, pc);
        out.averaged = !proc.average.empty();
        out.aligned = proc.aligned;
        out.out_fs_hz = proc.fs;
        if (out.averaged) {
            const nmr::dsp::Spectrum sp = nmr::dsp::spectrum(proc.average.data(), proc.average.size(), proc.fs,
                                                            nmr::dsp::Window::Hann);
            out.peak = nmr::dsp::find_peak(sp, -proc.fs / 2, proc.fs / 2, 200.0);
        }
    }

    if (!set_state(State::Complete)) return last_err_;
    ESP_LOGI(TAG, "complete: %u/%u scans averaged, peak %.1f Hz SNR %.1f", (unsigned)out.scans_complete,
             (unsigned)out.scans_planned, out.peak.freq_hz, out.peak.snr);
    return ESP_OK;
}

void Controller::abort() {
    state_ = State::Aborting;
    adc_.stop();
    safe_outputs();
    state_ = State::Idle;
    ESP_LOGW(TAG, "aborted -> Idle");
}

}  // namespace fw
