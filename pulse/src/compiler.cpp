#include <algorithm>
#include <cmath>

#include "pulse/compiler.hpp"

namespace pulse {

namespace {

struct TimedInstr {
    uint64_t t;
    uint32_t seq;   // insertion order breaks ties: the compiler emits in the order the hardware needs
    Instr in;
};

uint32_t phase_units(double turns) {
    double f = turns - std::floor(turns);
    uint32_t u = static_cast<uint32_t>(std::llround(f * kTurn));
    return u % kTurn;
}

}  // namespace

bool compile(const std::vector<Scan>& scans, const Timing& tm, Program& out, std::vector<Diagnostic>& diag) {
    diag.clear();
    const auto ticks = [&](double s) { return static_cast<uint64_t>(std::llround(s * tm.tick_hz)); };
    std::vector<TimedInstr> ev;
    uint32_t seq = 0;
    const auto emit = [&](uint64_t t, Op op, uint8_t ch, uint32_t arg) { ev.push_back({t, seq++, Instr{op, ch, 0, arg}}); };

    const uint64_t tr = ticks(tm.repetition_s), pre = ticks(tm.pre_blank_s), dead = ticks(tm.dead_time_s);
    emit(0, Op::RX_BLANK, 0, 0);
    emit(0, Op::SET_FREQ, 0, static_cast<uint32_t>(std::llround(tm.tx_freq_hz * 1000.0)));
    emit(0, Op::SET_AMPLITUDE, 0, kTurn);
    uint64_t end_all = 0;
    for (size_t k = 0; k < scans.size(); ++k) {
        const uint64_t t0 = k * tr;
        uint64_t t = t0 + pre;   // the first pulse can come once the receiver has been blanked for pre_blank
        bool rx_enabled = false, acq_open = false;
        uint64_t last_tx_off = 0, last_event = t0;
        bool tx_yet = false;
        uint32_t amp = kTurn;
        emit(t0, Op::SCAN_BEGIN, 0, static_cast<uint32_t>(k));
        emit(t0, Op::RX_PHASE, 0, phase_units(scans[k].rx_phase_turns));
        for (size_t j = 0; j < scans[k].elements.size(); ++j) {
            const Element& e = scans[k].elements[j];
            const uint64_t d = ticks(e.duration_s);
            const auto fail = [&](const std::string& m) { diag.push_back({j, "scan " + std::to_string(k) + ": " + m}); };
            if (e.kind == Element::Pulse) {
                if (d == 0) { fail("pulse rounds to zero ticks"); continue; }
                if (acq_open) { fail("pulse inside an open acquisition window"); continue; }
                if (rx_enabled) {
                    if (t < last_event + pre) { fail("too little time before the pulse to blank the receiver"); continue; }
                    emit(t - pre, Op::RX_BLANK, 0, 0);
                    rx_enabled = false;
                }
                const uint32_t a = static_cast<uint32_t>(std::llround(std::clamp(e.amplitude, 0.0, 1.0) * kTurn));
                if (a != amp) { emit(t - pre, Op::SET_AMPLITUDE, 0, a); amp = a; }
                emit(t - pre, Op::SET_PHASE, 0, phase_units(e.phase_turns));
                if (e.marker) emit(t, Op::MARKER, 0, e.marker);
                emit(t, Op::TX_GATE_ON, 0, 0);
                t += d;
                emit(t, Op::TX_GATE_OFF, 0, 0);
                last_tx_off = t;
                tx_yet = true;
                last_event = t;
            } else if (e.kind == Element::Delay) {
                t += d;
            } else {  // Acquire
                if (d == 0) { fail("acquisition window rounds to zero ticks"); continue; }
                if (tx_yet && t < last_tx_off + dead) { fail("acquisition starts inside the receiver dead time"); continue; }
                if (!rx_enabled) {
                    emit(tx_yet ? last_tx_off + dead : t, Op::RX_ENABLE, 0, 0);
                    rx_enabled = true;
                }
                emit(t, Op::ACQ_ON, 0, e.window);
                t += d;
                emit(t, Op::ACQ_OFF, 0, e.window);
                last_event = t;
            }
        }
        if (rx_enabled) emit(t, Op::RX_BLANK, 0, 0);
        emit(t, Op::SCAN_END, 0, static_cast<uint32_t>(k));
        if (k + 1 < scans.size() && t + pre > (k + 1) * tr)
            diag.push_back({0, "scan " + std::to_string(k) + " is longer than the repetition time"});
        end_all = std::max(end_all, t);
    }
    if (!diag.empty()) return false;
    std::stable_sort(ev.begin(), ev.end(), [](const TimedInstr& a, const TimedInstr& b) {
        return a.t != b.t ? a.t < b.t : a.seq < b.seq;
    });
    out.tick_hz = tm.tick_hz;
    out.code.clear();
    uint64_t now = 0;
    for (const auto& x : ev) {
        while (x.t > now) {
            const uint64_t w = std::min<uint64_t>(x.t - now, 0xFFFFFFFFull);
            out.code.push_back({Op::WAIT, 0, 0, static_cast<uint32_t>(w)});
            now += w;
        }
        out.code.push_back(x.in);
    }
    out.code.push_back({Op::END, 0, 0, 0});
    if (out.code.size() > kMaxInstructions) {
        diag.push_back({0, "compiled program exceeds kMaxInstructions"});
        return false;
    }
    return true;
}

namespace {
double cyc(int k, Cycle c) { return c == Cycle::Cyclops ? 0.25 * (k % 4) : 0.0; }
Element P(double d, double ph, uint32_t marker = 0) { Element e; e.kind = Element::Pulse; e.duration_s = d; e.phase_turns = ph; e.marker = marker; return e; }
Element D(double d) { Element e; e.kind = Element::Delay; e.duration_s = d; return e; }
Element A(double d, uint32_t w = 0) { Element e; e.kind = Element::Acquire; e.duration_s = d; e.window = w; return e; }
}  // namespace

std::vector<Scan> fid(double t90, double acq_start, double acq_len, int n_avg, Cycle c) {
    std::vector<Scan> s;
    for (int k = 0; k < n_avg; ++k) {
        const double ph = cyc(k, c);
        s.push_back({{P(t90, ph, kMarkerExcitation), D(acq_start), A(acq_len)}, ph});
    }
    return s;
}

std::vector<Scan> hahn_echo(double t90, double t180, double tau, double acq_len, int n_avg, Cycle c) {
    // centres: 90 at 0, 180 at tau, echo at 2 tau; 180 phase +90 degrees (90x - 180y)
    std::vector<Scan> s;
    for (int k = 0; k < n_avg; ++k) {
        const double p90 = cyc(k, c), p180 = cyc(k, c) + 0.25;
        s.push_back({{P(t90, p90, kMarkerExcitation), D(tau - t90 / 2 - t180 / 2), P(t180, p180, kMarkerRefocus),
                      D(tau - t180 / 2 - acq_len / 2), A(acq_len)},
                     2 * p180 - p90});
    }
    return s;
}

std::vector<Scan> inversion_recovery(double t90, double t180, double ti, double acq_start, double acq_len, int n_avg,
                                     Cycle c) {
    std::vector<Scan> s;
    for (int k = 0; k < n_avg; ++k) {
        const double ph = cyc(k, c);
        s.push_back({{P(t180, ph), D(ti - t180 / 2 - t90 / 2), P(t90, ph, kMarkerExcitation), D(acq_start), A(acq_len)}, ph});
    }
    return s;
}

std::vector<Scan> cpmg(double t90, double t180, double tau, int n_echoes, double acq_len, int n_avg, Cycle c) {
    std::vector<Scan> s;
    for (int k = 0; k < n_avg; ++k) {
        const double p90 = cyc(k, c), p180 = cyc(k, c) + 0.25;
        Scan sc;
        sc.rx_phase_turns = 2 * p180 - p90;
        sc.elements.push_back(P(t90, p90, kMarkerExcitation));
        sc.elements.push_back(D(tau - t90 / 2 - t180 / 2));
        for (int n = 0; n < n_echoes; ++n) {
            sc.elements.push_back(P(t180, p180, kMarkerRefocus));
            sc.elements.push_back(D(tau - t180 / 2 - acq_len / 2));
            sc.elements.push_back(A(acq_len, static_cast<uint32_t>(n)));
            sc.elements.push_back(D(tau - acq_len / 2 - t180 / 2));
        }
        s.push_back(sc);
    }
    return s;
}

}  // namespace pulse
