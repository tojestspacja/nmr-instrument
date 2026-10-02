#include "engine.hpp"

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "driver/mcpwm_sync.h"
#include "driver/rmt_tx.h"
#include "esp_attr.h"

namespace fw {

namespace {

constexpr uint32_t kMaxDur = 32767;

inline uint32_t word(uint32_t d0, uint32_t l0, uint32_t d1, uint32_t l1) {
    return (d0 & 0x7FFF) | ((l0 & 1u) << 15) | ((d1 & 0x7FFF) << 16) | ((l1 & 1u) << 31);
}

struct CapCtx {
    GateEngine* eng;
    Input in;
};
CapCtx g_ctx[kInputs];

bool IRAM_ATTR on_cap(mcpwm_cap_channel_handle_t, const mcpwm_capture_event_data_t* d, void* user) {
    auto* c = static_cast<CapCtx*>(user);
    c->eng->on_capture(c->in, d->cap_value, d->cap_edge == MCPWM_CAP_EDGE_POS);
    return false;
}

}  // namespace

size_t pack_segments(const pulse::Segment* seg, size_t n, uint32_t* out, size_t cap) {
    // halves of (level, duration <= kMaxDur); the total must be even (two halves per symbol word)
    size_t halves = 0;
    for (size_t i = 0; i < n; ++i) halves += (seg[i].ticks + kMaxDur - 1) / kMaxDur;
    const bool fixup = halves % 2;
    if (fixup) ++halves;
    if (halves / 2 > cap) return 0;
    size_t w = 0;
    uint32_t pend_d = 0, pend_l = 0;
    bool have = false;
    for (size_t i = 0; i < n; ++i) {
        uint32_t left = seg[i].ticks;
        size_t parts = (left + kMaxDur - 1) / kMaxDur;
        if (fixup && i + 1 == n) ++parts;   // split the last segment once more
        for (size_t k = 0; k < parts; ++k) {
            const uint32_t d = left / static_cast<uint32_t>(parts - k);   // even split, every part >= 1 tick
            left -= d;
            if (!have) { pend_d = d; pend_l = seg[i].level; have = true; }
            else { out[w++] = word(pend_d, pend_l, d, seg[i].level); have = false; }
        }
    }
    return w;
}

esp_err_t GateEngine::init(const EngineConfig& c) {
    cfg_ = c;
    esp_err_t e;
    // 1. RMT gate outputs
    const int gpins[pulse::kGateChannels] = {c.pin_tx_gate, c.pin_rx_open, c.pin_phase_sel};
    for (int i = 0; i < pulse::kGateChannels; ++i) {
        rmt_tx_channel_config_t rc = {};
        rc.gpio_num = static_cast<gpio_num_t>(gpins[i]);
        rc.clk_src = RMT_CLK_SRC_DEFAULT;
        rc.resolution_hz = c.tick_hz;
        rc.mem_block_symbols = 48;
        rc.trans_queue_depth = 2;
        rc.flags.init_level = 0;   // TX off, receiver blanked, phase register 0
        rmt_channel_handle_t ch;
        if ((e = rmt_new_tx_channel(&rc, &ch)) != ESP_OK) return e;
        if ((e = rmt_enable(ch)) != ESP_OK) return e;
        rmt_ch_[i] = ch;
    }
    rmt_copy_encoder_config_t ec = {};
    rmt_encoder_handle_t enc;
    if ((e = rmt_new_copy_encoder(&ec, &enc)) != ESP_OK) return e;
    rmt_enc_ = enc;
    rmt_sync_manager_config_t sc = {};
    sc.tx_channel_array = reinterpret_cast<rmt_channel_handle_t*>(rmt_ch_);
    sc.array_size = pulse::kGateChannels;
    rmt_sync_manager_handle_t sync;
    if ((e = rmt_new_sync_manager(&sc, &sync)) != ESP_OK) return e;
    rmt_sync_ = sync;

    // 2. Capture: group 0 = the three gate loopbacks, group 1 = beat references and the ADC frame sync. Both timers
    //    at 80 MHz, phase-reset by one GPIO sync edge so every timestamp is on the same timebase.
    for (int g = 0; g < 2; ++g) {
        mcpwm_capture_timer_config_t tc = {};
        tc.group_id = g;
        tc.clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT;
        tc.resolution_hz = 80 * 1000 * 1000;
        mcpwm_cap_timer_handle_t timer;
        if ((e = mcpwm_new_capture_timer(&tc, &timer)) != ESP_OK) return e;
        cap_timer_[g] = timer;
        if (c.pin_sync >= 0) {
            mcpwm_gpio_sync_src_config_t ssc = {};
            ssc.group_id = g;
            ssc.gpio_num = c.pin_sync;
            mcpwm_sync_handle_t s;
            if ((e = mcpwm_new_gpio_sync_src(&ssc, &s)) != ESP_OK) return e;
            cap_sync_[g] = s;
            mcpwm_capture_timer_sync_phase_config_t ph = {};
            ph.sync_src = s;
            ph.count_value = 0;
            ph.direction = MCPWM_TIMER_DIRECTION_UP;
            if ((e = mcpwm_capture_timer_set_phase_on_sync(timer, &ph)) != ESP_OK) return e;
        }
    }
    mcpwm_capture_timer_get_resolution(static_cast<mcpwm_cap_timer_handle_t>(cap_timer_[0]), &cap_hz_);
    const int pins[kInputs] = {c.pin_tx_gate, c.pin_rx_open, c.pin_phase_sel, c.pin_dds_sign, c.pin_lo_i, c.pin_ws};
    for (int i = 0; i < kInputs; ++i) {
        if (pins[i] < 0) continue;
        mcpwm_capture_channel_config_t cc = {};
        cc.gpio_num = pins[i];
        cc.prescale = 1;
        cc.flags.pos_edge = true;
        cc.flags.neg_edge = true;
        mcpwm_cap_channel_handle_t ch;
        if ((e = mcpwm_new_capture_channel(static_cast<mcpwm_cap_timer_handle_t>(cap_timer_[i < 3 ? 0 : 1]), &cc, &ch)) != ESP_OK) return e;
        g_ctx[i] = {this, static_cast<Input>(i)};
        mcpwm_capture_event_callbacks_t cb = {};
        cb.on_cap = on_cap;
        if ((e = mcpwm_capture_channel_register_event_callbacks(ch, &cb, &g_ctx[i])) != ESP_OK) return e;
        if ((e = mcpwm_capture_channel_enable(ch)) != ESP_OK) return e;
        cap_ch_[i] = ch;
        // the gate pins are RMT outputs: keep their input path on so the capture sees the pad
        gpio_input_enable(static_cast<gpio_num_t>(pins[i]));
    }
    for (int g = 0; g < 2; ++g) {
        if ((e = mcpwm_capture_timer_enable(static_cast<mcpwm_cap_timer_handle_t>(cap_timer_[g]))) != ESP_OK) return e;
        if ((e = mcpwm_capture_timer_start(static_cast<mcpwm_cap_timer_handle_t>(cap_timer_[g]))) != ESP_OK) return e;
    }
    if (c.pin_sync >= 0) {   // one rising edge: both timers restart from 0 together
        const auto p = static_cast<gpio_num_t>(c.pin_sync);
        gpio_set_direction(p, GPIO_MODE_INPUT_OUTPUT);
        gpio_set_level(p, 0);
        gpio_set_level(p, 1);
        gpio_set_level(p, 0);
    }
    return ESP_OK;
}

esp_err_t GateEngine::load(const pulse::ScanPlan& p) {
    for (int ch = 0; ch < pulse::kGateChannels; ++ch) {
        // a trailing segment need not be transmitted: the end-of-transmission level holds it
        size_t n = p.nseg[ch];
        const uint8_t final_level = n ? p.seg[ch][n - 1].level : p.initial[ch];
        if (n > 1) --n;
        if (n == 0) {
            const pulse::Segment s[1] = {{final_level, 2}};
            nsym_[ch] = pack_segments(s, 1, sym_[ch], kMaxSymbols);
        } else {
            nsym_[ch] = pack_segments(p.seg[ch], n, sym_[ch], kMaxSymbols);
        }
        if (nsym_[ch] == 0) return ESP_ERR_NO_MEM;
        eot_[ch] = final_level;
    }
    return ESP_OK;
}

esp_err_t GateEngine::start() {
    nedges_ = 0;
    enabled_mask_ = (1u << kInTxGate) | (1u << kInRxOpen) | (1u << kInPhaseSel) | (1u << kInDdsSign) | (1u << kInLoI);
    esp_err_t e = rmt_sync_reset(static_cast<rmt_sync_manager_handle_t>(rmt_sync_));
    if (e != ESP_OK) return e;
    for (int ch = 0; ch < pulse::kGateChannels; ++ch) {
        rmt_transmit_config_t tc = {};
        tc.loop_count = 0;
        tc.flags.eot_level = eot_[ch];
        // with a sync manager nothing starts until all three channels are queued; then they start on one clock edge
        if ((e = rmt_transmit(static_cast<rmt_channel_handle_t>(rmt_ch_[ch]), static_cast<rmt_encoder_handle_t>(rmt_enc_),
                              sym_[ch], nsym_[ch] * 4, &tc)) != ESP_OK) return e;
    }
    return ESP_OK;
}

esp_err_t GateEngine::wait_done(uint32_t timeout_ms) {
    for (int ch = 0; ch < pulse::kGateChannels; ++ch) {
        const esp_err_t e = rmt_tx_wait_all_done(static_cast<rmt_channel_handle_t>(rmt_ch_[ch]), static_cast<int>(timeout_ms));
        if (e != ESP_OK) return e;
    }
    enabled_mask_ = 0;
    return ESP_OK;
}

void GateEngine::arm_single(Input in) { single_mask_ = single_mask_ | (1u << in); }

void IRAM_ATTR GateEngine::on_capture(Input in, uint32_t t, bool rising) {
    const uint32_t bit = 1u << in;
    if (!(enabled_mask_ & bit)) {
        if (!(single_mask_ & bit)) return;
        single_mask_ = single_mask_ & ~bit;
    }
    const size_t k = nedges_;
    if (k < kMaxEdges) edges_[k] = {t, static_cast<uint8_t>(in), static_cast<uint8_t>(rising)};
    nedges_ = k + 1;
}

}  // namespace fw
