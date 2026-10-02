#include "pulse/backend.hpp"

namespace pulse {

size_t count_scans(const std::vector<Event>& ev) {
    size_t n = 0;
    for (const auto& e : ev) n += e.op == Op::SCAN_BEGIN;
    return n;
}

bool plan_scan(const std::vector<Event>& ev, size_t k, const BackendCaps& caps, ScanPlan& p, Diagnostic& diag) {
    p = ScanPlan{};
    // state carried in from before the scan (frequency, amplitude, the last phase)
    size_t i = 0, scans = 0;
    uint32_t phase_before = 0;
    for (; i < ev.size(); ++i) {
        const Event& e = ev[i];
        if (e.op == Op::SCAN_BEGIN && scans++ == k) break;
        if (e.op == Op::SET_FREQ && e.chan == 0) p.freq_mhz = e.arg;
        if (e.op == Op::SET_AMPLITUDE && e.chan == 0) p.amplitude = e.arg;
        if (e.op == Op::SET_PHASE && e.chan == 0) phase_before = e.arg;
    }
    if (i == ev.size()) { diag = {i, "scan index out of range"}; return false; }
    p.scan = ev[i].arg;
    p.start = ev[i].t;
    p.initial[kTxGate] = 0;
    p.initial[kRxOpen] = 0;      // compiler-generated scans start blanked
    p.initial[kPhaseSel] = 0;
    p.phase_reg[0] = phase_before;
    p.nphase = 1;
    uint8_t level[kGateChannels] = {0, 0, 0};
    uint64_t last[kGateChannels] = {p.start, p.start, p.start};
    bool phase0_used = false;
    const auto change = [&](int ch, uint8_t v, uint64_t t) -> bool {
        if (v == level[ch]) return true;
        if (t > last[ch]) {
            if (p.nseg[ch] >= kMaxSegments) { diag = {0, "gate channel exceeds kMaxSegments"}; return false; }
            p.seg[ch][p.nseg[ch]++] = {level[ch], static_cast<uint32_t>(t - last[ch])};
        }
        level[ch] = v;
        last[ch] = t;
        return true;
    };
    for (++i; i < ev.size(); ++i) {
        const Event& e = ev[i];
        const uint64_t rel = e.t - p.start;
        switch (e.op) {
            case Op::TX_GATE_ON: if (!change(kTxGate, 1, e.t)) return false; break;
            case Op::TX_GATE_OFF: if (!change(kTxGate, 0, e.t)) return false; break;
            case Op::RX_ENABLE: if (!change(kRxOpen, 1, e.t)) return false; break;
            case Op::RX_BLANK: if (!change(kRxOpen, 0, e.t)) return false; break;
            case Op::SET_PHASE: {
                // map phase values onto the registers in order of first use
                int reg = -1;
                for (int r = 0; r < p.nphase; ++r)
                    if (p.phase_reg[r] == e.arg) reg = r;
                if (reg < 0 && !phase0_used) { p.phase_reg[0] = e.arg; reg = 0; }
                if (reg < 0) {
                    if (p.nphase >= caps.phase_registers) { diag = {i, "scan uses more RF phases than the backend has phase registers"}; return false; }
                    p.phase_reg[p.nphase] = e.arg;
                    reg = p.nphase++;
                }
                if (reg == 0) phase0_used = true;
                if (!change(kPhaseSel, static_cast<uint8_t>(reg), e.t)) return false;
                break;
            }
            case Op::SET_FREQ:
                if (e.arg != p.freq_mhz) { diag = {i, "frequency change inside a scan is not supported by this backend"}; return false; }
                break;
            case Op::SET_AMPLITUDE:
                if (e.arg != p.amplitude) { diag = {i, "amplitude change inside a scan is not supported by this backend"}; return false; }
                break;
            case Op::RX_PHASE: p.rx_phase = e.arg; break;
            case Op::MARKER:
                if (e.arg == kMarkerExcitation && !p.has_excitation) { p.has_excitation = true; p.excitation = rel; }
                break;
            case Op::ACQ_ON:
                if (p.nwin >= kMaxWindows) { diag = {i, "too many acquisition windows in a scan"}; return false; }
                p.win[p.nwin++] = {e.arg, rel, rel};
                break;
            case Op::ACQ_OFF:
                for (int w = p.nwin - 1; w >= 0; --w)
                    if (p.win[w].id == e.arg && p.win[w].end == p.win[w].start) { p.win[w].end = rel; break; }
                break;
            case Op::SCAN_END:
                p.length = rel;
                for (int ch = 0; ch < kGateChannels; ++ch)
                    if (e.t > last[ch]) {
                        if (p.nseg[ch] >= kMaxSegments) { diag = {i, "gate channel exceeds kMaxSegments"}; return false; }
                        p.seg[ch][p.nseg[ch]++] = {level[ch], static_cast<uint32_t>(e.t - last[ch])};
                        last[ch] = e.t;
                    }
                if (level[kTxGate]) { diag = {i, "scan ends with TX on"}; return false; }
                return true;
            case Op::SCAN_BEGIN:
            case Op::END:
                diag = {i, "scan without SCAN_END"};
                return false;
            default: break;
        }
    }
    diag = {ev.size(), "scan without SCAN_END"};
    return false;
}

}  // namespace pulse
