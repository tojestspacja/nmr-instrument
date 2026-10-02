#include "nmr/sim.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

#include "instrument_config.hpp"

namespace nmr {

namespace {

constexpr double kPi = 3.14159265358979323846;
using cplx = std::complex<double>;

struct Rng {   // xoshiro256** + Box-Muller: reproducible across native and wasm
    uint64_t s[4];
    explicit Rng(uint64_t seed) {
        uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        for (auto& x : s) {
            z += 0x9E3779B97F4A7C15ull;
            uint64_t y = z;
            y = (y ^ (y >> 30)) * 0xBF58476D1CE4E5B9ull;
            y = (y ^ (y >> 27)) * 0x94D049BB133111EBull;
            x = y ^ (y >> 31);
        }
    }
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t next() {
        const uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 45);
        return r;
    }
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    double gauss() {
        double u = uniform();
        while (u <= 0) u = uniform();
        return std::sqrt(-2.0 * std::log(u)) * std::cos(2 * kPi * uniform());
    }
};

}  // namespace

InstrumentModel design_model() {
    using namespace cfg;
    InstrumentModel m;
    m.gamma_bar = NUCLEUS_GAMMA_BAR_HZ_PER_T;
    m.b0_current = B0_CURRENT_A;
    m.t1 = SAMPLE_T1_S;
    m.t2 = SAMPLE_T2_S;
    m.proton_density = SAMPLE_PROTON_DENSITY_PER_M3;
    m.temperature = SAMPLE_TEMPERATURE_K;
    m.hbar = CONSTANTS_HBAR_J_S;
    m.k_b = CONSTANTS_K_BOLTZMANN_J_PER_K;
    m.tx_coil_current = TX_COIL_CURRENT_A;
    m.tank_f0 = TX_FREQUENCY_HZ;   // tuned at the coil to the transmitter (ADR-0007)
    m.tank_q = RF_COIL_QUALITY_FACTOR;
    m.rx_gain = RECEIVER_LNA1_GAIN * RECEIVER_LNA2_GAIN * RECEIVER_MIXER_CONVERSION_GAIN * RECEIVER_IF_GAIN;
    m.if_pole_hz = RECEIVER_IF_BANDWIDTH_HZ;
    m.noise_density = RECEIVER_NOISE_DENSITY_V_PER_RTHZ;
    m.lo_hz = LO_FREQUENCY_HZ;
    m.adc_full_scale = RECEIVER_ADC_FULL_SCALE_V;
    m.adc_bits = static_cast<int>(RECEIVER_ADC_BITS);
    m.adc_rate = ACQUISITION_RAW_RATE_HZ;
    return m;
}

SimResult simulate(const pulse::Program& prog, const std::vector<Voxel>& vox, const InstrumentModel& m) {
    SimResult res;
    std::vector<pulse::Event> ev;
    pulse::Diagnostic d;
    if (!pulse::expand(prog, ev, d)) return res;
    const size_t nv = vox.size();
    Rng rng(m.seed);

    // per-voxel invariants
    std::vector<double> bmag(nv), m0(nv);
    for (size_t v = 0; v < nv; ++v) {
        const Vec3 b = vox[v].b0_hat * (vox[v].b0_per_amp * m.b0_current) + m.earth_field;
        bmag[v] = b.norm();
        m0[v] = curie_magnetization(m.proton_density, m.gamma_bar, bmag[v], m.temperature, m.hbar, m.k_b);
    }
    std::vector<Vec3> mag(nv, Vec3{0, 0, 1});
    std::vector<double> df(nv);
    std::vector<cplx> amp(nv);   // complex weight: emf x tank x IF response, for conj(m) e^{+i phi}

    double f_tx = 0, rf_phase = 0, rf_amp = 1.0, rx_phase = 0, t_exc = 0;
    bool tx_on = false;
    struct Win { bool open = false; uint32_t id = 0; double t0 = 0; };
    Win win;
    uint32_t scan = 0;
    double beat_true = 0, beat_meas = 0;
    const double tick = prog.tick_hz;
    const double dt_adc = 1.0 / m.adc_rate;
    const double lsb = m.adc_full_scale / std::ldexp(1.0, m.adc_bits - 1);
    const double sigma = m.noise_density * m.rx_gain * std::sqrt(0.5 * kPi * m.if_pole_hz);
    size_t clipped = 0, total = 0;

    const auto refresh = [&]() {   // offsets and receive weights depend on f_tx
        for (size_t v = 0; v < nv; ++v) {
            df[v] = m.gamma_bar * bmag[v] - f_tx;
            const double f = f_tx + df[v], omega = 2 * kPi * f;
            const double emf = omega * vox[v].b1_perp * m0[v] * vox[v].volume;
            const double r = f / m.tank_f0;
            const cplx tank = 1.0 / cplx(1.0 - r * r, r / m.tank_q);
            const double fif = f - m.lo_hz;
            const cplx ifr = 1.0 / cplx(1.0, fif / m.if_pole_hz);
            amp[v] = emf * tank * ifr * m.rx_gain * std::polar(1.0, vox[v].b1_phi);
        }
    };

    std::vector<cplx> zt(nv), step(nv);
    // baseband phasor P(t) = sum_v amp_v conj(m_xy,v(t)) at a time t within a free-precession interval starting at ta
    const auto precess_all = [&](double dtv) {
        for (size_t v = 0; v < nv; ++v) mag[v] = precess(mag[v], df[v], dtv, m.t1, m.t2);
    };
    const auto sample_interval = [&](double ta, double tb, Record& rec) {
        // ADC grid points t_n = adc_t0 + n dt in [ta, tb)
        const double n0 = std::ceil((ta - m.adc_t0) / dt_adc - 1e-9), n1 = std::ceil((tb - m.adc_t0) / dt_adc - 1e-9);
        if (n1 <= n0) return;
        const size_t ns = static_cast<size_t>(n1 - n0);
        const double t_first = m.adc_t0 + n0 * dt_adc;
        const int k = std::max(1, m.interp);
        const double dg = k * dt_adc;
        const size_t ng = (ns + k - 1) / k + 3;   // coarse grid covering the interval, one point before, two after
        std::vector<cplx> pg(ng);
        // coarse grid times: t_first + (j-1) dg; state at ta
        for (size_t v = 0; v < nv; ++v) {
            const double rate_re = -1.0 / m.t2, rate_im = -2 * kPi * df[v];
            const cplx lam(rate_re, rate_im);
            const double tstart = t_first - dg - ta;
            zt[v] = cplx(mag[v].x, mag[v].y) * std::exp(lam * tstart);
            step[v] = std::exp(lam * dg);
        }
        for (size_t j = 0; j < ng; ++j) {
            cplx p = 0;
            for (size_t v = 0; v < nv; ++v) {
                p += amp[v] * std::conj(zt[v]);
                zt[v] *= step[v];
            }
            pg[j] = p;
            res.emf_peak = std::max(res.emf_peak, std::abs(p));
        }
        // Catmull-Rom between coarse points (exact at integer u, i.e. when k == 1 and the fractional part is 0).
        const auto at = [&](double uu) -> cplx {
            const size_t j = static_cast<size_t>(uu);
            const double f = uu - static_cast<double>(j);
            if (k == 1 && f == 0.0) return pg[j];
            const cplx p0 = pg[j - 1], p1 = pg[j], p2 = pg[j + 1], p3 = pg[j + 2];
            return 0.5 * ((2.0 * p1) + (p2 - p0) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * (f * f) +
                          (3.0 * p1 - p0 - 3.0 * p2 + p3) * (f * f * f));
        };
        const double skew = m.iq_skew_samples;                 // I sampled this many raw samples after Q
        const double beat_phasor_hz = f_tx - m.lo_hz;
        rec.i.reserve(rec.i.size() + ns);
        rec.q.reserve(rec.q.size() + ns);
        for (size_t s = 0; s < ns; ++s) {
            const double t = t_first + s * dt_adc;             // Q sample time (and the complex-sample grid time)
            const double u = static_cast<double>(s) / k + 1.0;
            // I is sampled skew raw samples later; at skew == 0 this is exactly the old single-sample behaviour.
            const cplx p_i = skew == 0.0 ? at(u) : at(u + skew / k);
            const double t_i = t + skew * dt_adc;
            const cplx z_i = p_i * std::polar(1.0, 2 * kPi * beat_phasor_hz * t_i + beat_true);
            const double vi = z_i.real() + m.offset_i + sigma * rng.gauss();   // I noise first (RNG order preserved)
            const cplx z_q = at(u) * std::polar(1.0, 2 * kPi * beat_phasor_hz * t + beat_true);
            const double vq = z_q.imag() + m.offset_q + sigma * rng.gauss();
            const auto code = [&](double vlt) {
                double c = std::round(vlt / lsb);
                const double lim = std::ldexp(1.0, m.adc_bits - 1);
                if (c > lim - 1 || c < -lim) ++clipped;
                return static_cast<int16_t>(std::clamp(c, -lim, lim - 1));
            };
            rec.i.push_back(code(vi));
            rec.q.push_back(code(vq));
            total += 2;
        }
    };

    double tc = 0;
    Record* cur = nullptr;
    for (const auto& e : ev) {
        const double te = static_cast<double>(e.t) / tick;
        if (te > tc) {
            if (tx_on) {
                for (size_t v = 0; v < nv; ++v) {
                    const double b1 = 0.5 * vox[v].b1_perp * m.tx_coil_current * rf_amp;
                    mag[v] = pulse(mag[v], m.gamma_bar, b1, rf_phase + vox[v].b1_phi, df[v], te - tc);
                }
            } else {
                if (win.open && cur) sample_interval(tc, te, *cur);
                precess_all(te - tc);
            }
            tc = te;
        }
        switch (e.op) {
            case pulse::Op::SET_FREQ: f_tx = e.arg / 1000.0; refresh(); break;
            case pulse::Op::SET_PHASE: rf_phase = 2 * kPi * e.arg / pulse::kTurn; break;
            case pulse::Op::SET_AMPLITUDE: rf_amp = static_cast<double>(e.arg) / pulse::kTurn; break;
            case pulse::Op::TX_GATE_ON: tx_on = true; break;
            case pulse::Op::TX_GATE_OFF: tx_on = false; break;
            case pulse::Op::RX_PHASE: rx_phase = static_cast<double>(e.arg) / pulse::kTurn; break;
            case pulse::Op::MARKER: if (e.arg == pulse::kMarkerExcitation) t_exc = te; break;
            case pulse::Op::SCAN_BEGIN:
                scan = e.arg;
                beat_true = m.lo_coherent ? 0.0 : 2 * kPi * rng.uniform();
                beat_meas = beat_true + m.beat_measurement_sigma * rng.gauss();
                break;
            case pulse::Op::ACQ_ON: {
                win = {true, e.arg, te};
                Record r;
                r.scan = scan;
                r.window = e.arg;
                r.t_excitation = t_exc;
                r.rx_phase_turns = rx_phase;
                r.beat_phase_true = beat_true;
                r.beat_phase_measured = beat_meas;
                r.f_tx = f_tx;
                const double n0 = std::ceil((te - m.adc_t0) / dt_adc - 1e-9);
                r.t_first = m.adc_t0 + n0 * dt_adc;
                res.records.push_back(std::move(r));
                cur = &res.records.back();
                break;
            }
            case pulse::Op::ACQ_OFF: win.open = false; cur = nullptr; break;
            default: break;
        }
    }
    res.adc_clip_fraction = total ? static_cast<double>(clipped) / total : 0;
    return res;
}

}  // namespace nmr
