// The pulse-program intermediate representation (ADR-0004).
//
// A program is a flat array of 8-byte instructions executed by a deterministic timing engine. Time only advances
// in WAIT; every other instruction takes effect at the current time (tick). The IR is hardware-neutral: frequency in
// millihertz, phase in 1/65536 turn, amplitude as a Q16 fraction, time in engine ticks (design: timing.tick).
// The same program drives the ESP32 RMT backend, the simulator, and a future FPGA backend.
#pragma once

#include <cstdint>

namespace pulse {

enum class Op : uint8_t {
    END = 0,           // end of program
    WAIT = 1,          // advance time by arg ticks (arg > 0)
    SET_FREQ = 2,      // TX channel chan: frequency = arg mHz
    SET_PHASE = 3,     // TX channel chan: RF phase = arg (low 16 bits) / 65536 turn
    SET_AMPLITUDE = 4, // TX channel chan: amplitude = arg / 65536 of full scale (<= 65536)
    TX_GATE_ON = 5,    // TX channel chan: transmitter output enabled
    TX_GATE_OFF = 6,
    RX_BLANK = 7,      // receiver chan: protected / blanked
    RX_ENABLE = 8,     // receiver chan: un-blanked
    ACQ_ON = 9,        // receiver chan: start keeping samples (a window); arg = window id
    ACQ_OFF = 10,
    RX_PHASE = 11,     // receiver chan: phase the DSP must rotate this scan's data by (arg/65536 turn); metadata
    SCAN_BEGIN = 12,   // arg = scan index (for averaging / phase cycling bookkeeping)
    SCAN_END = 13,
    LOOP_BEGIN = 14,   // repeat the body up to the matching LOOP_END arg times (arg >= 1)
    LOOP_END = 15,
    MARKER = 16,       // arg = marker id; backends timestamp it (e.g. 1 = excitation pulse start)
    SYNC_WAIT = 17,    // wait for the external reference edge (chan = source); backends that cannot, reject it
};

constexpr const char* op_name(Op op) {
    switch (op) {
        case Op::END: return "END";
        case Op::WAIT: return "WAIT";
        case Op::SET_FREQ: return "SET_FREQ";
        case Op::SET_PHASE: return "SET_PHASE";
        case Op::SET_AMPLITUDE: return "SET_AMPLITUDE";
        case Op::TX_GATE_ON: return "TX_GATE_ON";
        case Op::TX_GATE_OFF: return "TX_GATE_OFF";
        case Op::RX_BLANK: return "RX_BLANK";
        case Op::RX_ENABLE: return "RX_ENABLE";
        case Op::ACQ_ON: return "ACQ_ON";
        case Op::ACQ_OFF: return "ACQ_OFF";
        case Op::RX_PHASE: return "RX_PHASE";
        case Op::SCAN_BEGIN: return "SCAN_BEGIN";
        case Op::SCAN_END: return "SCAN_END";
        case Op::LOOP_BEGIN: return "LOOP_BEGIN";
        case Op::LOOP_END: return "LOOP_END";
        case Op::MARKER: return "MARKER";
        case Op::SYNC_WAIT: return "SYNC_WAIT";
    }
    return "?";
}

struct Instr {
    Op op = Op::END;
    uint8_t chan = 0;
    uint16_t flags = 0;
    uint32_t arg = 0;
};
static_assert(sizeof(Instr) == 8, "instructions are 8 bytes on every platform");

inline constexpr uint32_t kTurn = 65536;          // phase units per turn
inline constexpr uint32_t kMaxInstructions = 8192; // fixed capacity: programs are bounded before they run
inline constexpr int kMaxLoopDepth = 4;
inline constexpr int kTxChannels = 2;             // RF transmit channels the IR can address (hardware: 1 today)
inline constexpr int kRxChannels = 2;

// marker ids with a meaning to the DSP
inline constexpr uint32_t kMarkerExcitation = 1;   // start of the excitation pulse of a scan
inline constexpr uint32_t kMarkerRefocus = 2;      // start of a refocusing pulse

}  // namespace pulse
