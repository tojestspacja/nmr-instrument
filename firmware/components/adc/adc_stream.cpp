#include "adc_stream.hpp"

#include <cstring>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

namespace fw {

namespace {
constexpr size_t kDmaFrames = 500;     // frames per DMA buffer (2 ms at 250 kframes/s)
constexpr size_t kDmaBufs = 8;
constexpr size_t kTmpWords = kDmaFrames * 2;   // two 32-bit slots per frame

bool IRAM_ATTR on_ovf(i2s_chan_handle_t, i2s_event_data_t*, void* user) {
    static_cast<AdcStream*>(user)->on_overflow();
    return false;
}

// ADS8688 command words (datasheet: 16-bit command, then 16 SCLKs of data)
constexpr uint16_t kNoOp = 0x0000, kAutoRst = 0xA000, kRst = 0x8500;
constexpr uint16_t prog_write(uint8_t addr, uint8_t val) { return static_cast<uint16_t>((addr << 9) | (1u << 8) | val); }
}  // namespace

esp_err_t AdcStream::init(const AdcConfig& c) {
    cfg_ = c;
    buf_ = static_cast<int16_t*>(heap_caps_malloc(c.capacity_frames * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buf_) buf_ = static_cast<int16_t*>(heap_caps_malloc(c.capacity_frames * sizeof(int16_t), MALLOC_CAP_8BIT));
    dma_tmp_ = static_cast<uint32_t*>(heap_caps_malloc(kTmpWords * 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    return buf_ && dma_tmp_ ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t AdcStream::configure_adc() {
    spi_bus_config_t bus = {};
    bus.sclk_io_num = cfg_.pins.sclk;
    bus.mosi_io_num = cfg_.pins.mosi;
    bus.miso_io_num = cfg_.pins.miso;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
    if (e != ESP_OK) return e;
    spi_device_interface_config_t dev = {};
    dev.mode = 1;                       // ADS8688: CPOL 0, CPHA 1
    dev.clock_speed_hz = 4 * 1000 * 1000;
    dev.spics_io_num = cfg_.pins.cs;
    dev.queue_size = 1;
    spi_device_handle_t h;
    if ((e = spi_bus_add_device(SPI2_HOST, &dev, &h)) != ESP_OK) { spi_bus_free(SPI2_HOST); return e; }
    const auto xfer = [&](uint16_t cmd) {
        uint8_t tx[4] = {static_cast<uint8_t>(cmd >> 8), static_cast<uint8_t>(cmd), 0, 0}, rx[4];
        spi_transaction_t t = {};
        t.length = 32;
        t.tx_buffer = tx;
        t.rx_buffer = rx;
        return spi_device_polling_transmit(h, &t);
    };
    e = xfer(kRst);
    const uint8_t mask = static_cast<uint8_t>((1u << cfg_.ch_i) | (1u << cfg_.ch_q));
    if (e == ESP_OK) e = xfer(prog_write(0x01, mask));                       // AUTO_SEQ_EN
    if (e == ESP_OK) e = xfer(prog_write(0x02, static_cast<uint8_t>(~mask))); // power down the others
    if (e == ESP_OK) e = xfer(prog_write(static_cast<uint8_t>(0x05 + cfg_.ch_i), cfg_.range_code));
    if (e == ESP_OK) e = xfer(prog_write(static_cast<uint8_t>(0x05 + cfg_.ch_q), cfg_.range_code));
    if (e == ESP_OK) e = xfer(kAutoRst);
    spi_bus_remove_device(h);
    spi_bus_free(SPI2_HOST);
    return e;
}

esp_err_t AdcStream::start() {
    taken_ = 0;
    ovf_ = 0;
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = kDmaBufs;
    cc.dma_frame_num = kDmaFrames;
    cc.auto_clear_before_cb = true;   // TX sends zeros = NO_OP commands
    i2s_chan_handle_t tx, rx;
    esp_err_t e = i2s_new_channel(&cc, &tx, &rx);
    if (e != ESP_OK) return e;
    i2s_std_config_t sc = {};
    sc.clk_cfg.sample_rate_hz = cfg_.frame_rate;
    sc.clk_cfg.clk_src = I2S_CLK_SRC_PLL_160M;
    sc.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_128;   // MCLK = 32 MHz = 160/5, BCLK = 16 MHz: integer dividers
    sc.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    sc.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    sc.gpio_cfg.bclk = static_cast<gpio_num_t>(cfg_.pins.sclk);
    sc.gpio_cfg.ws = static_cast<gpio_num_t>(cfg_.pins.cs);
    sc.gpio_cfg.dout = static_cast<gpio_num_t>(cfg_.pins.mosi);
    sc.gpio_cfg.din = static_cast<gpio_num_t>(cfg_.pins.miso);
    // ADS8688 is SPI mode 1: SDI sampled and SDO changed... the inverted bit clock makes the I2S transmitter change
    // on the external rising edge and the receiver sample on the external falling edge (VERIFY at bring-up, T-ADC-1)
    sc.gpio_cfg.invert_flags.bclk_inv = true;
    if ((e = i2s_channel_init_std_mode(tx, &sc)) != ESP_OK) return e;
    if ((e = i2s_channel_init_std_mode(rx, &sc)) != ESP_OK) return e;
    i2s_event_callbacks_t cb = {};
    cb.on_recv_q_ovf = on_ovf;
    i2s_channel_register_event_callback(rx, &cb, this);
    tx_ = tx;
    rx_ = rx;
    if ((e = i2s_channel_enable(tx)) != ESP_OK) return e;
    if ((e = i2s_channel_enable(rx)) != ESP_OK) return e;
    running_ = true;
    return ESP_OK;
}

size_t AdcStream::collect(size_t frames, uint32_t timeout_ms) {
    if (!running_) return 0;
    const TickType_t t_end = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (taken_ < frames && taken_ < cfg_.capacity_frames && xTaskGetTickCount() < t_end) {
        size_t got = 0;
        if (i2s_channel_read(static_cast<i2s_chan_handle_t>(rx_), dma_tmp_, kTmpWords * 4, &got, 20) != ESP_OK && got == 0) continue;
        const size_t nf = got / 8;
        for (size_t k = 0; k < nf && taken_ < cfg_.capacity_frames; ++k) {
            // left slot (CS low): 16 command clocks then the 16-bit result, MSB first -> low half of the 32-bit word
            buf_[taken_++] = static_cast<int16_t>(static_cast<uint16_t>(dma_tmp_[2 * k] & 0xFFFF) ^ 0x8000);
        }
    }
    return taken_;
}

esp_err_t AdcStream::stop() {
    if (!running_) return ESP_OK;
    auto tx = static_cast<i2s_chan_handle_t>(tx_), rx = static_cast<i2s_chan_handle_t>(rx_);
    i2s_channel_disable(rx);
    i2s_channel_disable(tx);
    i2s_del_channel(rx);
    i2s_del_channel(tx);
    running_ = false;
    return ESP_OK;
}

}  // namespace fw
