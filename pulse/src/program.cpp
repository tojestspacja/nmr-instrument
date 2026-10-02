#include <cmath>
#include <cstring>
#include <sstream>

#include "pulse/program.hpp"

namespace pulse {

uint32_t crc32(const uint8_t* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= d[i];
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

bool expand(const Program& p, std::vector<Event>& out, Diagnostic& diag, uint64_t max_events) {
    out.clear();
    struct Frame { size_t body; uint32_t left; };
    Frame stack[kMaxLoopDepth];
    int depth = 0;
    uint64_t t = 0;
    size_t pc = 0, steps = 0;
    // structural pass: balanced loops, END present
    {
        int d = 0;
        bool end = false;
        for (size_t i = 0; i < p.code.size(); ++i) {
            const Op op = p.code[i].op;
            if (op == Op::LOOP_BEGIN) {
                if (++d > kMaxLoopDepth) { diag = {i, "loop nesting deeper than kMaxLoopDepth"}; return false; }
                if (p.code[i].arg == 0) { diag = {i, "LOOP_BEGIN with zero count"}; return false; }
            } else if (op == Op::LOOP_END) {
                if (--d < 0) { diag = {i, "LOOP_END without LOOP_BEGIN"}; return false; }
            } else if (op == Op::WAIT && p.code[i].arg == 0) {
                diag = {i, "WAIT 0"};
                return false;
            } else if (op == Op::END) {
                if (d != 0) { diag = {i, "END inside a loop"}; return false; }
                end = true;
                break;
            }
        }
        if (!end) { diag = {p.code.size(), "program has no END"}; return false; }
        if (p.code.size() > kMaxInstructions) { diag = {kMaxInstructions, "program exceeds kMaxInstructions"}; return false; }
    }
    while (pc < p.code.size()) {
        if (++steps > max_events * 4 + 1000) { diag = {pc, "execution step budget exceeded"}; return false; }
        const Instr& in = p.code[pc];
        switch (in.op) {
            case Op::END:
                out.push_back({t, Op::END, 0, 0});
                return true;
            case Op::WAIT:
                t += in.arg;
                ++pc;
                break;
            case Op::LOOP_BEGIN:
                stack[depth++] = {pc + 1, in.arg};
                ++pc;
                break;
            case Op::LOOP_END:
                if (--stack[depth - 1].left > 0) {
                    pc = stack[depth - 1].body;
                } else {
                    --depth;
                    ++pc;
                }
                break;
            default:
                if (out.size() >= max_events) { diag = {pc, "unrolled program exceeds the event budget"}; return false; }
                out.push_back({t, in.op, in.chan, in.arg});
                ++pc;
        }
    }
    diag = {pc, "fell off the end of the program"};
    return false;
}

std::vector<Diagnostic> validate(const Program& p, const Limits& lim) {
    std::vector<Diagnostic> d;
    if (std::fabs(p.tick_hz - lim.tick_hz) > 1e-9 * lim.tick_hz) d.push_back({0, "program tick differs from the engine tick"});
    std::vector<Event> ev;
    Diagnostic ed;
    if (!expand(p, ev, ed, lim.max_events)) {
        d.push_back(ed);
        return d;
    }
    const double tick = lim.tick_hz;
    const auto ticks = [&](double s) { return static_cast<uint64_t>(std::llround(s * tick)); };
    bool tx_on[kTxChannels] = {}, rx_blanked[kRxChannels], acq[kRxChannels] = {};
    uint64_t tx_since[kTxChannels] = {}, blank_since[kRxChannels] = {}, last_tx_off = 0;
    bool any_tx_yet = false;
    for (int i = 0; i < kRxChannels; ++i) rx_blanked[i] = true;   // engines start blanked
    uint64_t tx_total = 0;
    for (size_t i = 0; i < ev.size(); ++i) {
        const Event& e = ev[i];
        const auto bad = [&](const std::string& m) { d.push_back({i, m}); };
        const bool txc = e.chan < kTxChannels, rxc = e.chan < kRxChannels;
        switch (e.op) {
            case Op::SET_FREQ: {
                if (!txc) { bad("TX channel out of range"); break; }
                const double f = e.arg / 1000.0;
                if (f < lim.tx_min_freq_hz || f > lim.tx_max_freq_hz) bad("frequency outside the supported range");
                break;
            }
            case Op::SET_AMPLITUDE:
                if (!txc) bad("TX channel out of range");
                if (e.arg > kTurn) bad("amplitude above full scale");
                break;
            case Op::SET_PHASE:
                if (!txc) bad("TX channel out of range");
                if (e.arg >= kTurn) bad("phase operand must be < 65536");
                break;
            case Op::TX_GATE_ON:
                if (!txc) { bad("TX channel out of range"); break; }
                if (tx_on[e.chan]) bad("TX gate already on");
                for (int r = 0; r < kRxChannels; ++r) {
                    if (!rx_blanked[r]) bad("TX gate on while a receiver is enabled");
                    else if (e.t - blank_since[r] < ticks(lim.pre_blank_s)) bad("TX gate on before the pre-blank interval");
                }
                tx_on[e.chan] = true;
                tx_since[e.chan] = e.t;
                break;
            case Op::TX_GATE_OFF:
                if (!txc) { bad("TX channel out of range"); break; }
                if (!tx_on[e.chan]) { bad("TX gate off while off"); break; }
                if (e.t - tx_since[e.chan] > ticks(lim.tx_max_pulse_s)) bad("TX pulse longer than the hardware limit");
                tx_total += e.t - tx_since[e.chan];
                tx_on[e.chan] = false;
                last_tx_off = e.t;
                any_tx_yet = true;
                break;
            case Op::RX_BLANK:
                if (!rxc) { bad("RX channel out of range"); break; }
                if (acq[e.chan]) bad("receiver blanked inside an acquisition window");
                if (!rx_blanked[e.chan]) blank_since[e.chan] = e.t;
                rx_blanked[e.chan] = true;
                break;
            case Op::RX_ENABLE:
                if (!rxc) { bad("RX channel out of range"); break; }
                for (int c = 0; c < kTxChannels; ++c)
                    if (tx_on[c]) bad("receiver enabled while TX is on");
                if (any_tx_yet && e.t - last_tx_off < ticks(lim.dead_time_s)) bad("receiver enabled inside the dead time");
                rx_blanked[e.chan] = false;
                break;
            case Op::ACQ_ON:
                if (!rxc) { bad("RX channel out of range"); break; }
                if (rx_blanked[e.chan]) bad("acquisition window opened on a blanked receiver");
                if (acq[e.chan]) bad("acquisition window already open");
                acq[e.chan] = true;
                break;
            case Op::ACQ_OFF:
                if (!rxc) { bad("RX channel out of range"); break; }
                if (!acq[e.chan]) bad("acquisition window closed while closed");
                acq[e.chan] = false;
                break;
            case Op::SYNC_WAIT:
                if (!lim.backend_has_sync_input) bad("SYNC_WAIT but the backend has no sync input");
                break;
            case Op::END:
                for (int c = 0; c < kTxChannels; ++c)
                    if (tx_on[c]) bad("program ends with TX on");
                for (int r = 0; r < kRxChannels; ++r)
                    if (acq[r]) bad("program ends inside an acquisition window");
                if (e.t > 0 && static_cast<double>(tx_total) / static_cast<double>(e.t) > lim.tx_max_duty)
                    bad("TX duty cycle above the limit");
                break;
            default:
                break;
        }
    }
    return d;
}

static void put(std::vector<uint8_t>& b, const void* p, size_t n) {
    const auto* c = static_cast<const uint8_t*>(p);
    b.insert(b.end(), c, c + n);   // little-endian hosts only (ESP32, x86, wasm): asserted below
}
static_assert(sizeof(double) == 8, "binary format assumes IEEE-754 binary64");

std::vector<uint8_t> encode(const Program& p) {
    std::vector<uint8_t> b;
    const char magic[4] = {'N', 'M', 'R', 'P'};
    const uint16_t ver = 1, res = 0;
    const uint32_t n = static_cast<uint32_t>(p.code.size());
    const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(p.code.data()), n * sizeof(Instr));
    put(b, magic, 4);
    put(b, &ver, 2);
    put(b, &res, 2);
    put(b, &p.tick_hz, 8);
    put(b, &n, 4);
    put(b, &crc, 4);
    put(b, p.code.data(), n * sizeof(Instr));
    return b;
}

bool decode(const uint8_t* d, size_t n, Program& out, std::string& err) {
    if (n < 24 || std::memcmp(d, "NMRP", 4) != 0) { err = "bad magic"; return false; }
    uint16_t ver;
    std::memcpy(&ver, d + 4, 2);
    if (ver != 1) { err = "unsupported version"; return false; }
    uint32_t cnt, crc;
    std::memcpy(&out.tick_hz, d + 8, 8);
    std::memcpy(&cnt, d + 16, 4);
    std::memcpy(&crc, d + 20, 4);
    if (cnt > kMaxInstructions || n != 24 + static_cast<size_t>(cnt) * 8) { err = "bad size"; return false; }
    if (crc32(d + 24, cnt * 8u) != crc) { err = "CRC mismatch"; return false; }
    out.code.resize(cnt);
    std::memcpy(out.code.data(), d + 24, cnt * 8u);
    return true;
}

std::string disassemble(const Program& p) {
    std::ostringstream s;
    uint64_t t = 0;
    for (size_t i = 0; i < p.code.size(); ++i) {
        const Instr& in = p.code[i];
        s << i << "\t" << (static_cast<double>(t) / p.tick_hz * 1e6) << " us\t" << op_name(in.op) << " ch" << int(in.chan)
          << " " << in.arg << "\n";
        if (in.op == Op::WAIT) t += in.arg;   // loops not unrolled here
    }
    return s.str();
}

}  // namespace pulse
