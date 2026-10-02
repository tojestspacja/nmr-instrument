// Backend planning: one scan of a validated program -> gate waveforms + acquisition windows + markers.
//
// This is the boundary between the hardware-neutral IR and a timing backend. The ESP32 backend packs the segments into
// RMT symbols; an FPGA backend would load the same segments into its sequencer. Fixed capacities: no allocation, so
// a plan can be built right before a scan runs.
//
// Gate channels (logic levels at the pins, 1 = asserted):
//   kTxGate  : TX_EN            (1 = transmit)
//   kRxOpen  : RX_BLANK pin     (1 = receive, 0 = blanked; the board's polarity)
//   kPhaseSel: DDS PSELECT      (0 = phase register 0, 1 = phase register 1)
#pragma once

#include <cstdint>
#include <vector>

#include "pulse/program.hpp"

namespace pulse {

inline constexpr int kGateChannels = 3;
inline constexpr int kTxGate = 0, kRxOpen = 1, kPhaseSel = 2;
inline constexpr int kMaxSegments = 96;
inline constexpr int kMaxWindows = 64;

struct Segment {
    uint8_t level;
    uint32_t ticks;   // > 0
};

struct Window {
    uint32_t id;
    uint64_t start, end;   // ticks from the scan start
};

struct ScanPlan {
    uint32_t scan = 0;
    uint64_t start = 0;        // absolute program tick of the scan start (SCAN_BEGIN)
    uint64_t length = 0;       // ticks to SCAN_END
    uint8_t initial[kGateChannels] = {};
    Segment seg[kGateChannels][kMaxSegments];
    uint16_t nseg[kGateChannels] = {};
    uint32_t phase_reg[2] = {}; // DDS phase-register contents (1/65536 turn) selected by kPhaseSel
    uint8_t nphase = 0;
    uint32_t freq_mhz = 0, amplitude = kTurn;
    uint32_t rx_phase = 0;      // RX_PHASE operand
    Window win[kMaxWindows];
    uint8_t nwin = 0;
    bool has_excitation = false;
    uint64_t excitation = 0;    // marker 1, ticks from the scan start
};

struct BackendCaps {
    int phase_registers = 2;    // AD9834: PHASE0/PHASE1 selected by the PSELECT pin
    uint32_t max_segment_ticks = 32767u * 2u;   // informative; packing splits longer segments
};

// Number of scans in an expanded program (SCAN_BEGIN count).
size_t count_scans(const std::vector<Event>& ev);

// Plan scan k (0-based). Fails if the scan needs more phase values than the backend has registers, exceeds a fixed
// capacity, or is malformed.
bool plan_scan(const std::vector<Event>& ev, size_t k, const BackendCaps& caps, ScanPlan& out, Diagnostic& diag);

}  // namespace pulse
