// Pulse programs: compilation, placement of protection timing, validation rules, codec, determinism.
#include <algorithm>
#include <cmath>

#include "check.hpp"
#include "instrument_config.hpp"
#include "pulse/compiler.hpp"

using namespace pulse;
using namespace nmr::cfg;

static Timing timing() { return {TIMING_TICK_HZ, TX_FREQUENCY_HZ, TIMING_PRE_BLANK_S, RECEIVER_DEAD_TIME_S, ACQUISITION_REPETITION_TIME_S}; }
static Limits limits() {
    Limits l;
    l.tick_hz = TIMING_TICK_HZ; l.tx_max_pulse_s = LIMITS_TX_MAX_PULSE_S; l.tx_max_duty = LIMITS_TX_MAX_DUTY;
    l.dead_time_s = RECEIVER_DEAD_TIME_S; l.pre_blank_s = TIMING_PRE_BLANK_S; l.tx_min_freq_hz = 0;
    l.tx_max_freq_hz = DDS_MAX_OUTPUT_FREQUENCY_HZ;
    return l;
}
static uint64_t ticks(double s) { return static_cast<uint64_t>(std::llround(s * TIMING_TICK_HZ)); }
static bool has(const std::vector<Diagnostic>& d, const char* s) {
    return std::any_of(d.begin(), d.end(), [&](const Diagnostic& x) { return x.message.find(s) != std::string::npos; });
}

int main() {
    const int navg = static_cast<int>(ACQUISITION_AVERAGES);
    // FID with CYCLOPS compiles, validates, and places events where the protection rules say
    {
        Program p;
        std::vector<Diagnostic> d;
        CHECK(compile(fid(TX_T90_S, ACQUISITION_START_DELAY_S, ACQUISITION_DURATION_S, navg, Cycle::Cyclops), timing(), p, d), "FID compiles");
        CHECK(validate(p, limits()).empty(), "FID validates");
        std::vector<Event> ev;
        Diagnostic e;
        CHECK(expand(p, ev, e), "FID expands");
        std::vector<uint64_t> on, off, acq, en;
        std::vector<uint32_t> ph;
        for (const auto& x : ev) {
            if (x.op == Op::TX_GATE_ON) on.push_back(x.t);
            if (x.op == Op::TX_GATE_OFF) off.push_back(x.t);
            if (x.op == Op::ACQ_ON) acq.push_back(x.t);
            if (x.op == Op::RX_ENABLE) en.push_back(x.t);
            if (x.op == Op::SET_PHASE) ph.push_back(x.arg);
        }
        CHECK(on.size() == static_cast<size_t>(navg) && acq.size() == on.size(), "one pulse and one window per scan");
        CHECK(on[0] == ticks(TIMING_PRE_BLANK_S), "first pulse after the pre-blank interval");
        CHECK(off[0] - on[0] == ticks(TX_T90_S), "pulse length = t90 in ticks");
        CHECK(en[0] - off[0] == ticks(RECEIVER_DEAD_TIME_S), "receiver enabled exactly one dead time after the pulse");
        CHECK(acq[0] - off[0] == ticks(ACQUISITION_START_DELAY_S), "acquisition starts at the configured delay");
        CHECK(on[1] - on[0] == ticks(ACQUISITION_REPETITION_TIME_S), "scans spaced by the repetition time");
        CHECK(ph[0] == 0 && ph[1] == kTurn / 4 && ph[2] == kTurn / 2 && ph[3] == 3 * kTurn / 4, "CYCLOPS pulse phases 0,90,180,270");
        // determinism: same input -> identical bytes
        Program p2;
        compile(fid(TX_T90_S, ACQUISITION_START_DELAY_S, ACQUISITION_DURATION_S, navg, Cycle::Cyclops), timing(), p2, d);
        CHECK(encode(p) == encode(p2), "compilation is deterministic");
        // codec round trip, and corruption is rejected
        auto b = encode(p);
        Program q;
        std::string err;
        CHECK(decode(b.data(), b.size(), q, err) && encode(q) == b, "encode/decode round trip");
        b[30] ^= 0x40;
        CHECK(!decode(b.data(), b.size(), q, err) && err == "CRC mismatch", "corrupted program rejected by CRC");
    }
    // Hahn echo, inversion recovery, CPMG: same compiler, valid programs
    {
        Program p;
        std::vector<Diagnostic> d;
        CHECK(compile(hahn_echo(TX_T90_S, TX_T180_S, 0.020, 0.020, navg, Cycle::Cyclops), timing(), p, d) && validate(p, limits()).empty(), "Hahn echo compiles and validates");
        std::vector<Event> ev;
        Diagnostic e;
        expand(p, ev, e);
        uint64_t on90 = 0, on180 = 0, acqon = 0, acqoff = 0;
        int n = 0;
        for (const auto& x : ev) {
            if (x.op == Op::TX_GATE_ON) { if (n == 0) on90 = x.t; if (n == 1) on180 = x.t; ++n; }
            if (x.op == Op::ACQ_ON && !acqon) acqon = x.t;
            if (x.op == Op::ACQ_OFF && !acqoff) acqoff = x.t;
        }
        const double c90 = (on90 + ticks(TX_T90_S) / 2.0), c180 = (on180 + ticks(TX_T180_S) / 2.0), echo = (acqon + acqoff) / 2.0;
        CHECK_NEAR((c180 - c90) / TIMING_TICK_HZ, 0.020, 1.5 / TIMING_TICK_HZ, 0, "180 centre at tau");
        CHECK_NEAR((echo - c90) / TIMING_TICK_HZ, 0.040, 1.5 / TIMING_TICK_HZ, 0, "acquisition centred on the echo at 2 tau");
        CHECK(compile(inversion_recovery(TX_T90_S, TX_T180_S, 0.5, ACQUISITION_START_DELAY_S, 0.2, 4, Cycle::Cyclops), timing(), p, d) && validate(p, limits()).empty(), "inversion recovery compiles and validates");
        CHECK(compile(cpmg(TX_T90_S, TX_T180_S, 0.010, 16, 0.005, 4, Cycle::Cyclops), timing(), p, d) && validate(p, limits()).empty(), "CPMG (16 echoes) compiles and validates");
    }
    // impossible sequences are rejected before they run
    {
        Program p;
        std::vector<Diagnostic> d;
        CHECK(!compile(fid(TX_T90_S, RECEIVER_DEAD_TIME_S / 2, 0.1, 1, Cycle::None), timing(), p, d) && has(d, "dead time"), "acquisition inside the dead time is rejected");
        CHECK(!compile(fid(TX_T90_S, ACQUISITION_START_DELAY_S, 5.0, 2, Cycle::None), timing(), p, d) && has(d, "repetition"), "scan longer than TR is rejected");
        // hand-written illegal programs (what a buggy client could send)
        const auto prog = [](std::vector<Instr> c) { Program q; q.tick_hz = TIMING_TICK_HZ; q.code = c; return q; };
        const uint32_t f = static_cast<uint32_t>(std::llround(TX_FREQUENCY_HZ * 1000));
        auto v = validate(prog({{Op::SET_FREQ, 0, 0, f}, {Op::RX_ENABLE}, {Op::WAIT, 0, 0, 1000}, {Op::TX_GATE_ON}, {Op::WAIT, 0, 0, 100}, {Op::TX_GATE_OFF}, {Op::END}}), limits());
        CHECK(has(v, "receiver is enabled") || has(v, "TX gate on while a receiver is enabled"), "TX while the receiver is enabled");
        v = validate(prog({{Op::WAIT, 0, 0, 1000}, {Op::TX_GATE_ON}, {Op::WAIT, 0, 0, 100}, {Op::TX_GATE_OFF}, {Op::WAIT, 0, 0, 10}, {Op::RX_ENABLE}, {Op::END}}), limits());
        CHECK(has(v, "dead time"), "receiver enabled inside the dead time");
        v = validate(prog({{Op::WAIT, 0, 0, 1000}, {Op::TX_GATE_ON}, {Op::WAIT, 0, 0, static_cast<uint32_t>(ticks(0.1))}, {Op::TX_GATE_OFF}, {Op::END}}), limits());
        CHECK(has(v, "longer than the hardware limit"), "over-long pulse");
        // the IR carries mHz in 32 bits (<= 4.29 MHz); check the range rule against a tighter backend limit
        Limits lo = limits();
        lo.tx_max_freq_hz = 2.0 * TX_FREQUENCY_HZ;
        v = validate(prog({{Op::SET_FREQ, 0, 0, 3u * f}, {Op::END}}), lo);
        CHECK(has(v, "frequency outside"), "frequency above the backend's range");
        v = validate(prog({{Op::WAIT, 0, 0, 10}, {Op::TX_GATE_ON}, {Op::WAIT, 0, 0, 100}, {Op::TX_GATE_OFF}, {Op::END}}), limits());
        CHECK(has(v, "pre-blank"), "TX gate before the pre-blank interval");
        v = validate(prog({{Op::WAIT, 0, 0, 1000}, {Op::TX_GATE_ON}, {Op::END}}), limits());
        CHECK(has(v, "ends with TX on"), "program ending with TX on");
        v = validate(prog({{Op::SYNC_WAIT}, {Op::END}}), limits());
        CHECK(has(v, "sync input"), "SYNC_WAIT on a backend without a sync input");
        v = validate(prog({{Op::LOOP_BEGIN, 0, 0, 3}, {Op::WAIT, 0, 0, 5}, {Op::END}}), limits());
        CHECK(has(v, "END inside a loop"), "unbalanced loop");
        v = validate(prog({{Op::WAIT}, {Op::END}}), limits());
        CHECK(has(v, "WAIT 0"), "zero wait");
    }
    // loops unroll with the right timing
    {
        Program p;
        p.tick_hz = TIMING_TICK_HZ;
        p.code = {{Op::LOOP_BEGIN, 0, 0, 3}, {Op::MARKER, 0, 0, 7}, {Op::LOOP_BEGIN, 0, 0, 2}, {Op::WAIT, 0, 0, 10}, {Op::LOOP_END}, {Op::LOOP_END}, {Op::END}};
        std::vector<Event> ev;
        Diagnostic e;
        CHECK(expand(p, ev, e), "nested loops expand");
        CHECK(ev.size() == 4 && ev[0].t == 0 && ev[1].t == 20 && ev[2].t == 40 && ev[3].t == 60, "nested loop timing (3 x 2 x 10 ticks)");
    }
    return finish("test_pulse");
}
