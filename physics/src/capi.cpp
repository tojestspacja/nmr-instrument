#include "nmr/capi.h"

#include <cstring>
#include <string>
#include <vector>

#include "instrument_config.hpp"
#include "nmr/pipeline.hpp"
#include "nmr/sim.hpp"
#include "pulse/compiler.hpp"

namespace {
std::string g_err;
std::vector<nmr::Voxel> g_vox;
nmr::SimResult g_sim;
std::vector<nmr::Record> g_loaded;   // records loaded from outside (measured data)
bool g_use_loaded = false;

const std::vector<nmr::Record>& records() { return g_use_loaded ? g_loaded : g_sim.records; }

nmr::InstrumentModel to_model(const nmr_model* c) {
    nmr::InstrumentModel m;
    m.gamma_bar = c->gamma_bar; m.b0_current = c->b0_current; m.t1 = c->t1; m.t2 = c->t2;
    m.proton_density = c->proton_density; m.temperature = c->temperature;
    m.hbar = nmr::cfg::CONSTANTS_HBAR_J_S; m.k_b = nmr::cfg::CONSTANTS_K_BOLTZMANN_J_PER_K;
    m.earth_field = {c->earth_x, c->earth_y, c->earth_z};
    m.tx_coil_current = c->tx_coil_current; m.tank_f0 = c->tank_f0; m.tank_q = c->tank_q; m.rx_gain = c->rx_gain;
    m.if_pole_hz = c->if_pole_hz; m.noise_density = c->noise_density; m.lo_hz = c->lo_hz;
    m.adc_full_scale = c->adc_full_scale; m.adc_rate = c->adc_rate; m.adc_t0 = c->adc_t0;
    m.offset_i = c->offset_i; m.offset_q = c->offset_q; m.adc_bits = c->adc_bits;
    m.lo_coherent = c->lo_coherent != 0; m.interp = c->interp; m.beat_measurement_sigma = c->beat_measurement_sigma;
    m.seed = c->seed;
    return m;
}
}  // namespace

extern "C" {

const char* nmr_config_sha256(void) { return nmr::cfg::CONFIG_SHA256; }
int32_t nmr_abi_layout(int32_t* s) {
    s[0] = sizeof(nmr_timing); s[1] = sizeof(nmr_sequence); s[2] = sizeof(nmr_limits);
    s[3] = sizeof(nmr_model); s[4] = sizeof(nmr_record_meta); s[5] = sizeof(nmr_pipeline);
    return 6;
}
const char* nmr_last_error(void) { return g_err.c_str(); }

void nmr_default_timing(nmr_timing* t) {
    using namespace nmr::cfg;
    t->tick_hz = TIMING_TICK_HZ;
    t->tx_freq_hz = TX_FREQUENCY_HZ;
    t->pre_blank_s = TIMING_PRE_BLANK_S;
    t->dead_time_s = RECEIVER_DEAD_TIME_S;
    t->repetition_s = ACQUISITION_REPETITION_TIME_S;
}

void nmr_default_sequence(nmr_sequence* s, int32_t kind) {
    using namespace nmr::cfg;
    std::memset(s, 0, sizeof *s);
    s->kind = kind;
    s->t90 = TX_T90_S;
    s->t180 = TX_T180_S;
    s->tau = 0.020;
    s->ti = 1.0;
    s->acq_start = ACQUISITION_START_DELAY_S;
    s->acq_len = ACQUISITION_DURATION_S;
    s->n_echoes = 8;
    s->n_avg = static_cast<int32_t>(ACQUISITION_AVERAGES);
    s->cyclops = 1;
    if (kind == 1) s->acq_len = 0.020;   // echo window centred on the echo
    if (kind == 3) s->acq_len = 0.010;
}

void nmr_default_limits(nmr_limits* l) {
    using namespace nmr::cfg;
    l->tick_hz = TIMING_TICK_HZ;
    l->tx_max_pulse_s = LIMITS_TX_MAX_PULSE_S;
    l->tx_max_duty = LIMITS_TX_MAX_DUTY;
    l->dead_time_s = RECEIVER_DEAD_TIME_S;
    l->pre_blank_s = TIMING_PRE_BLANK_S;
    l->tx_min_freq_hz = 0;
    l->tx_max_freq_hz = DDS_MAX_OUTPUT_FREQUENCY_HZ;
    l->has_sync_input = 0;
}

int32_t nmr_compile(const nmr_timing* t, const nmr_sequence* s, uint8_t* out, int32_t cap) {
    const pulse::Cycle cyc = s->cyclops ? pulse::Cycle::Cyclops : pulse::Cycle::None;
    std::vector<pulse::Scan> scans;
    switch (s->kind) {
        case 0: scans = pulse::fid(s->t90, s->acq_start, s->acq_len, s->n_avg, cyc); break;
        case 1: scans = pulse::hahn_echo(s->t90, s->t180, s->tau, s->acq_len, s->n_avg, cyc); break;
        case 2: scans = pulse::inversion_recovery(s->t90, s->t180, s->ti, s->acq_start, s->acq_len, s->n_avg, cyc); break;
        case 3: scans = pulse::cpmg(s->t90, s->t180, s->tau, s->n_echoes, s->acq_len, s->n_avg, cyc); break;
        default: g_err = "unknown sequence kind"; return -1;
    }
    pulse::Timing tm{t->tick_hz, t->tx_freq_hz, t->pre_blank_s, t->dead_time_s, t->repetition_s};
    pulse::Program p;
    std::vector<pulse::Diagnostic> d;
    if (!pulse::compile(scans, tm, p, d)) {
        g_err.clear();
        for (auto& x : d) g_err += x.message + "\n";
        return -2;
    }
    const auto bytes = pulse::encode(p);
    if (static_cast<int32_t>(bytes.size()) > cap) { g_err = "output buffer too small"; return -3; }
    std::memcpy(out, bytes.data(), bytes.size());
    return static_cast<int32_t>(bytes.size());
}

static bool load_prog(const uint8_t* b, int32_t n, pulse::Program& p) {
    if (!pulse::decode(b, static_cast<size_t>(n), p, g_err)) return false;
    return true;
}

int32_t nmr_validate(const uint8_t* b, int32_t n, const nmr_limits* l) {
    pulse::Program p;
    if (!load_prog(b, n, p)) return -1;
    pulse::Limits lim;
    lim.tick_hz = l->tick_hz; lim.tx_max_pulse_s = l->tx_max_pulse_s; lim.tx_max_duty = l->tx_max_duty;
    lim.dead_time_s = l->dead_time_s; lim.pre_blank_s = l->pre_blank_s; lim.tx_min_freq_hz = l->tx_min_freq_hz;
    lim.tx_max_freq_hz = l->tx_max_freq_hz; lim.backend_has_sync_input = l->has_sync_input != 0;
    const auto d = pulse::validate(p, lim);
    g_err.clear();
    for (auto& x : d) g_err += "event " + std::to_string(x.index) + ": " + x.message + "\n";
    return static_cast<int32_t>(d.size());
}

int32_t nmr_disassemble(const uint8_t* b, int32_t n, char* out, int32_t cap) {
    pulse::Program p;
    if (!load_prog(b, n, p)) return -1;
    const std::string s = pulse::disassemble(p);
    if (static_cast<int32_t>(s.size()) + 1 > cap) { g_err = "output buffer too small"; return -3; }
    std::memcpy(out, s.c_str(), s.size() + 1);
    return static_cast<int32_t>(s.size());
}

void nmr_default_model(nmr_model* c) {
    const nmr::InstrumentModel m = nmr::design_model();
    std::memset(c, 0, sizeof *c);
    c->gamma_bar = m.gamma_bar; c->b0_current = m.b0_current; c->t1 = m.t1; c->t2 = m.t2;
    c->proton_density = m.proton_density; c->temperature = m.temperature;
    c->tx_coil_current = m.tx_coil_current; c->tank_f0 = m.tank_f0; c->tank_q = m.tank_q; c->rx_gain = m.rx_gain;
    c->if_pole_hz = m.if_pole_hz; c->noise_density = m.noise_density; c->lo_hz = m.lo_hz;
    c->adc_full_scale = m.adc_full_scale; c->adc_rate = m.adc_rate; c->adc_bits = m.adc_bits;
    c->lo_coherent = 1; c->interp = m.interp; c->seed = 1;
}

int32_t nmr_voxelize(double g) {
    g_vox = nmr::voxelize(nmr::design_probe(), nmr::design_sample(), g);
    return static_cast<int32_t>(g_vox.size());
}

int32_t nmr_voxels(double* o, int32_t cap) {
    const int32_t n = std::min<int32_t>(cap, static_cast<int32_t>(g_vox.size()));
    for (int32_t k = 0; k < n; ++k) {
        const auto& v = g_vox[k];
        const double row[7] = {v.p.x, v.p.y, v.p.z, v.volume, v.b0_per_amp, v.b1_perp, v.b1_phi};
        std::memcpy(o + 7 * k, row, sizeof row);
    }
    return n;
}

void nmr_fields_at(double x, double y, double z, double* b0, double* b1) {
    static const nmr::ProbeGeometry pg = nmr::design_probe();
    const nmr::Vec3 a = pg.b0.field_per_amp({x, y, z}, pg.mu0), b = pg.rf.field_per_amp({x, y, z}, pg.mu0);
    b0[0] = a.x; b0[1] = a.y; b0[2] = a.z;
    b1[0] = b.x; b1[1] = b.y; b1[2] = b.z;
}

int32_t nmr_simulate(const uint8_t* b, int32_t n, const nmr_model* c) {
    pulse::Program p;
    if (!load_prog(b, n, p)) return -1;
    if (g_vox.empty()) { g_err = "no voxels: call nmr_voxelize first"; return -2; }
    g_sim = nmr::simulate(p, g_vox, to_model(c));
    g_use_loaded = false;
    return static_cast<int32_t>(g_sim.records.size());
}

int32_t nmr_record(int32_t idx, nmr_record_meta* m, int16_t* i, int16_t* q, int32_t cap) {
    const auto& rs = records();
    if (idx < 0 || idx >= static_cast<int32_t>(rs.size())) { g_err = "record index out of range"; return -1; }
    const auto& r = rs[idx];
    m->scan = r.scan; m->window = r.window; m->t_first = r.t_first; m->t_excitation = r.t_excitation;
    m->rx_phase_turns = r.rx_phase_turns; m->beat_phase_true = r.beat_phase_true;
    m->beat_phase_measured = r.beat_phase_measured; m->f_tx = r.f_tx;
    m->n = static_cast<int32_t>(r.i.size());
    if (i && q) {
        const int32_t k = std::min<int32_t>(cap, m->n);
        std::memcpy(i, r.i.data(), k * sizeof(int16_t));
        std::memcpy(q, r.q.data(), k * sizeof(int16_t));
    }
    return m->n;
}

double nmr_sim_emf_peak(void) { return g_sim.emf_peak; }
double nmr_sim_clip_fraction(void) { return g_sim.adc_clip_fraction; }

void nmr_clear_records(void) { g_loaded.clear(); g_use_loaded = true; }

int32_t nmr_load_record(const nmr_record_meta* m, const int16_t* i, const int16_t* q) {
    nmr::Record r;
    r.scan = m->scan; r.window = m->window; r.t_first = m->t_first; r.t_excitation = m->t_excitation;
    r.rx_phase_turns = m->rx_phase_turns; r.beat_phase_measured = m->beat_phase_measured; r.f_tx = m->f_tx;
    r.i.assign(i, i + m->n);
    r.q.assign(q, q + m->n);
    g_loaded.push_back(std::move(r));
    g_use_loaded = true;
    return static_cast<int32_t>(g_loaded.size());
}

void nmr_default_pipeline(nmr_pipeline* p) {
    using namespace nmr::cfg;
    std::memset(p, 0, sizeof *p);
    p->adc_rate = ACQUISITION_RAW_RATE_HZ;
    p->adc_full_scale = RECEIVER_ADC_FULL_SCALE_V;
    p->adc_bits = static_cast<int32_t>(RECEIVER_ADC_BITS);
    p->decimation = static_cast<int32_t>(ACQUISITION_DECIMATION);
    p->fir_taps = 63;
    p->offset_tail = 0.1;
    p->lo_hz = LO_FREQUENCY_HZ;
    p->correct_beat = 1;
}

int32_t nmr_process(const nmr_pipeline* p, double* out, int32_t cap, double* fs, double* t0) {
    std::vector<nmr::RecordView> v;
    for (const auto& r : records()) {
        if (static_cast<int32_t>(r.window) != p->window_id) continue;
        nmr::RecordView rv;
        rv.scan = r.scan; rv.window = r.window; rv.t_first = r.t_first; rv.t_excitation = r.t_excitation;
        rv.rx_phase_turns = r.rx_phase_turns; rv.beat_phase = r.beat_phase_measured; rv.f_if = r.f_tx - p->lo_hz;
        rv.i = r.i.data(); rv.q = r.q.data(); rv.n = r.i.size();
        v.push_back(rv);
    }
    if (v.empty()) { g_err = "no records for this window"; return -1; }
    nmr::PipelineConfig c;
    c.adc_rate = p->adc_rate; c.adc_full_scale = p->adc_full_scale; c.adc_bits = p->adc_bits;
    c.decimation = static_cast<size_t>(p->decimation); c.fir_taps = static_cast<size_t>(p->fir_taps);
    c.fir_cutoff_hz = p->fir_cutoff_hz; c.offset_tail = p->offset_tail; c.correct_beat = p->correct_beat != 0;
    const nmr::Processed pr = nmr::process(v, c);
    if (!pr.aligned) { g_err = "scans are not sample-aligned relative to excitation"; return -2; }
    const int32_t n = std::min<int32_t>(cap, static_cast<int32_t>(pr.average.size()));
    for (int32_t k = 0; k < n; ++k) { out[2 * k] = pr.average[k].real(); out[2 * k + 1] = pr.average[k].imag(); }
    *fs = pr.fs;
    *t0 = pr.t0;
    return n;
}

int32_t nmr_spectrum(const double* x, int32_t n, double fs, int32_t w, double tc, int32_t pad, double* freq,
                     double* val, int32_t cap) {
    std::vector<nmr::dsp::cplx> z(n);
    for (int32_t k = 0; k < n; ++k) z[k] = {x[2 * k], x[2 * k + 1]};
    const auto s = nmr::dsp::spectrum(z.data(), n, fs, static_cast<nmr::dsp::Window>(w), tc, pad);
    const int32_t m = static_cast<int32_t>(s.value.size());
    if (m > cap) { g_err = "output buffer too small"; return -1; }
    for (int32_t k = 0; k < m; ++k) { freq[k] = s.freq_hz[k]; val[2 * k] = s.value[k].real(); val[2 * k + 1] = s.value[k].imag(); }
    return m;
}

int32_t nmr_peak(const double* freq, const double* val, int32_t n, double f_min, double f_max, double guard, double* o) {
    nmr::dsp::Spectrum s;
    s.freq_hz.assign(freq, freq + n);
    s.value.resize(n);
    for (int32_t k = 0; k < n; ++k) s.value[k] = {val[2 * k], val[2 * k + 1]};
    const auto p = nmr::dsp::find_peak(s, f_min, f_max, guard);
    o[0] = p.freq_hz; o[1] = p.amplitude; o[2] = p.noise_rms; o[3] = p.snr;
    return 0;
}

int32_t nmr_fft(double* x, int32_t n, int32_t inverse) {
    if (n <= 0 || (n & (n - 1))) { g_err = "FFT size must be a power of two"; return -1; }
    nmr::dsp::FftPlan(static_cast<size_t>(n)).execute(reinterpret_cast<nmr::dsp::cplx*>(x), inverse != 0);
    return 0;
}

}  // extern "C"
