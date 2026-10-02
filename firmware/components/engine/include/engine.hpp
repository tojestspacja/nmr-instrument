// Hard-real-time gate engine (ADR-0005).
//
// Each scan's gate waveforms (TX_EN, RX_BLANK, DDS PSELECT; pulse/backend.hpp) are packed into RMT symbols and
// started on three RMT TX channels at the same APB clock edge through an RMT sync manager. After start the CPU does
// nothing for the gates: the RMT peripheral produces every edge. All gate pins (and optional reference inputs) are
// timestamped by MCPWM capture channels on one 80 MHz capture timer, so the actual timing is measured, not assumed.
//
// Real-time rules: no allocation after init() (fixed symbol and edge buffers), no logging or locks in the ISR, and
// nothing here touches Wi-Fi, JSON or the filesystem.
#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "pulse/backend.hpp"

namespace fw {

inline constexpr int kMaxSymbols = 512;       // per gate channel and scan
inline constexpr int kMaxEdges = 256;         // timestamped edges per scan, all inputs together

enum Input : uint8_t { kInTxGate = 0, kInRxOpen = 1, kInPhaseSel = 2, kInDdsSign = 3, kInLoI = 4, kInWs = 5, kInputs = 6 };

struct Edge {
    uint32_t ticks;   // capture timer (80 MHz) count
    uint8_t input;    // Input
    uint8_t rising;
};

struct EngineConfig {
    int pin_tx_gate, pin_rx_open, pin_phase_sel;
    int pin_dds_sign = -1, pin_lo_i = -1, pin_ws = -1;   // -1 = not wired (ECO-1 / ADC ws)
    int pin_sync = -1;                                   // internal: resets both capture timers together
    uint32_t tick_hz;                                    // must equal the pulse-program tick
};

class GateEngine {
public:
    esp_err_t init(const EngineConfig& c);
    // Pack a plan into RMT symbols (fixed buffers). Fails if a channel needs more than kMaxSymbols.
    esp_err_t load(const pulse::ScanPlan& p);
    // Arm the capture log, then start the three gate channels on the same clock edge.
    esp_err_t start();
    esp_err_t wait_done(uint32_t timeout_ms);
    // Capture log of the last scan (valid after wait_done).
    const Edge* edges() const { return edges_; }
    size_t edge_count() const { return nedges_ > kMaxEdges ? kMaxEdges : nedges_; }
    bool edges_overflowed() const { return nedges_ > kMaxEdges; }
    uint32_t capture_hz() const { return cap_hz_; }
    // One-shot capture of the next edge on an input (e.g. the first ADC frame sync after I2S starts).
    void arm_single(Input in);

    // ISR side (public only for the C callback trampoline)
    void on_capture(Input in, uint32_t t, bool rising);

private:
    EngineConfig cfg_{};
    void* rmt_ch_[pulse::kGateChannels] = {};
    void* rmt_enc_ = nullptr;
    void* rmt_sync_ = nullptr;
    void* cap_timer_[2] = {};
    void* cap_sync_[2] = {};
    void* cap_ch_[kInputs] = {};
    uint32_t sym_[pulse::kGateChannels][kMaxSymbols] = {};
    size_t nsym_[pulse::kGateChannels] = {};
    uint8_t eot_[pulse::kGateChannels] = {};
    Edge edges_[kMaxEdges];
    volatile size_t nedges_ = 0;
    volatile uint32_t single_mask_ = 0;   // inputs that log exactly one more edge
    volatile uint32_t enabled_mask_ = 0;  // inputs that log every edge
    uint32_t cap_hz_ = 0;
};

// Pack a gate channel's segments into RMT symbol words (15-bit durations; long segments split; an odd count is
// completed by splitting the last segment). Exposed for host tests. Returns the symbol count, 0 on overflow.
size_t pack_segments(const pulse::Segment* seg, size_t n, uint32_t* out, size_t cap);

}  // namespace fw
