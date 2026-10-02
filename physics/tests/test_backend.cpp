// Backend planning: per-scan gate waveforms, phase registers, windows, markers.
#include "check.hpp"
#include "instrument_config.hpp"
#include "pulse/backend.hpp"
#include "pulse/compiler.hpp"

using namespace pulse;
using namespace nmr::cfg;

static uint32_t ticks(double s) { return static_cast<uint32_t>(std::llround(s * TIMING_TICK_HZ)); }

static std::vector<Event> build(const std::vector<Scan>& s) {
    Timing tm{TIMING_TICK_HZ, TX_FREQUENCY_HZ, TIMING_PRE_BLANK_S, RECEIVER_DEAD_TIME_S, ACQUISITION_REPETITION_TIME_S};
    Program p;
    std::vector<Diagnostic> d;
    compile(s, tm, p, d);
    std::vector<Event> ev;
    Diagnostic e;
    expand(p, ev, e);
    return ev;
}

int main() {
    // FID, CYCLOPS: scan 2 has phase 180 degrees
    {
        const auto ev = build(fid(TX_T90_S, ACQUISITION_START_DELAY_S, 0.1, 4, Cycle::Cyclops));
        CHECK(count_scans(ev) == 4, "four scans");
        ScanPlan p;
        Diagnostic d;
        CHECK(plan_scan(ev, 2, BackendCaps{}, p, d), "plan scan 2");
        CHECK(p.scan == 2 && p.start == 2ull * ticks(ACQUISITION_REPETITION_TIME_S), "scan start time");
        // TX: low for pre_blank, high for t90, low for the rest
        CHECK(p.nseg[kTxGate] == 3, "TX gate: three segments");
        CHECK(p.seg[kTxGate][0].level == 0 && p.seg[kTxGate][0].ticks == ticks(TIMING_PRE_BLANK_S), "TX: pre-blank");
        CHECK(p.seg[kTxGate][1].level == 1 && p.seg[kTxGate][1].ticks == ticks(TX_T90_S), "TX: pulse length");
        // RX: blanked until pulse end + dead time, then open until the window closes, then blanked
        CHECK(p.seg[kRxOpen][0].level == 0 && p.seg[kRxOpen][0].ticks == ticks(TIMING_PRE_BLANK_S) + ticks(TX_T90_S) + ticks(RECEIVER_DEAD_TIME_S), "RX: blanked through the dead time");
        CHECK(p.seg[kRxOpen][1].level == 1, "RX: then open");
        CHECK(p.nwin == 1 && p.win[0].start == ticks(TIMING_PRE_BLANK_S) + ticks(TX_T90_S) + ticks(ACQUISITION_START_DELAY_S), "window start");
        CHECK(p.win[0].end - p.win[0].start == ticks(0.1), "window length");
        CHECK(p.has_excitation && p.excitation == ticks(TIMING_PRE_BLANK_S), "excitation marker at the pulse start");
        CHECK(p.nphase == 1 && p.phase_reg[0] == kTurn / 2, "one phase register holding 180 degrees");
        CHECK(p.rx_phase == kTurn / 2, "receiver phase 180 degrees");
        uint64_t sum = 0;
        for (int s = 0; s < p.nseg[kTxGate]; ++s) sum += p.seg[kTxGate][s].ticks;
        CHECK(sum == p.length, "TX segments cover the whole scan");
    }
    // Hahn echo: 90x then 180y -> two phase registers, PSELECT switches between the pulses
    {
        const auto ev = build(hahn_echo(TX_T90_S, TX_T180_S, 0.02, 0.02, 1, Cycle::None));
        ScanPlan p;
        Diagnostic d;
        CHECK(plan_scan(ev, 0, BackendCaps{}, p, d), "plan echo");
        CHECK(p.nphase == 2 && p.phase_reg[0] == 0 && p.phase_reg[1] == kTurn / 4, "phase registers 0 and 90 degrees");
        CHECK(p.nseg[kPhaseSel] >= 2 && p.seg[kPhaseSel][0].level == 0 && p.seg[kPhaseSel][1].level == 1, "PSELECT 0 -> 1 before the 180");
        CHECK(p.nseg[kTxGate] == 5, "TX: two pulses (5 segments)");
    }
    // three different RF phases in one scan exceed the AD9834's two registers
    {
        std::vector<Scan> s(1);
        Element a, b, c, dly;
        a.kind = b.kind = c.kind = Element::Pulse;
        a.duration_s = b.duration_s = c.duration_s = 1e-4;
        a.phase_turns = 0; b.phase_turns = 0.25; c.phase_turns = 0.5;
        dly.kind = Element::Delay; dly.duration_s = 1e-3;
        s[0].elements = {a, dly, b, dly, c};
        const auto ev = build(s);
        ScanPlan p;
        Diagnostic d;
        CHECK(!plan_scan(ev, 0, BackendCaps{}, p, d) && d.message.find("phase registers") != std::string::npos, "three phases rejected");
        BackendCaps more;
        more.phase_registers = 4;   // e.g. a future FPGA backend
        CHECK(!plan_scan(ev, 0, more, p, d) || p.nphase == 3, "a backend with more registers accepts it");
    }
    return finish("test_backend");
}
