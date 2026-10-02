// Programs, their expansion into an absolute-time timeline, validation, and the binary codec.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pulse/ir.hpp"

namespace pulse {

struct Program {
    double tick_hz = 0.0;            // engine tick the WAIT operands are counted in
    std::vector<Instr> code;         // control-plane container; backends copy into fixed buffers
};

// One timed effect after loops are unrolled: what happens on which channel at which absolute tick.
struct Event {
    uint64_t t = 0;   // ticks from program start
    Op op = Op::END;
    uint8_t chan = 0;
    uint32_t arg = 0;
};

// Hardware/protection limits the validator enforces. All times in seconds, frequencies in Hz.
struct Limits {
    double tick_hz = 0;
    double tx_max_pulse_s = 0;        // longest continuous TX gate
    double tx_max_duty = 0;           // TX on-time / program length
    double dead_time_s = 0;           // TX gate off -> receiver may be enabled
    double pre_blank_s = 0;           // receiver blanked -> TX gate may rise
    double tx_max_freq_hz = 0;
    double tx_min_freq_hz = 0;
    uint64_t max_events = 200000;     // unrolled event budget (backend buffer size)
    bool backend_has_sync_input = false;
};

struct Diagnostic {
    size_t index = 0;   // instruction index (or event index for timeline rules)
    std::string message;
};

// Expand loops into an absolute-time event list. Fails (returns false, diag set) on structural errors:
// unbalanced loops, depth > kMaxLoopDepth, missing END, zero WAIT, event budget exceeded.
bool expand(const Program& p, std::vector<Event>& out, Diagnostic& diag, uint64_t max_events = 200000);

// Check a program against the hardware rules. Returns all violations (empty = valid).
std::vector<Diagnostic> validate(const Program& p, const Limits& lim);

// Binary form: "NMRP", u16 version, u16 reserved, f64 tick_hz, u32 count, u32 crc32(code), then count x 8 bytes,
// little-endian. decode() rejects bad magic, version, size, or CRC.
std::vector<uint8_t> encode(const Program& p);
bool decode(const uint8_t* data, size_t n, Program& out, std::string& err);

std::string disassemble(const Program& p);

uint32_t crc32(const uint8_t* data, size_t n);

}  // namespace pulse
