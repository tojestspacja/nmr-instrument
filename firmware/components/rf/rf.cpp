#include "rf.hpp"

#include <cmath>

#include "driver/i2c_master.h"
#include "driver/spi_master.h"

namespace fw {

uint32_t dds_ftw(double mclk, double f, double* actual) {
    const uint32_t w = static_cast<uint32_t>(std::llround(f / mclk * 268435456.0)) & 0x0FFFFFFF;
    if (actual) *actual = w * mclk / 268435456.0;
    return w;
}

esp_err_t dds_program(const DdsPins& p, double mclk, double f, uint32_t ph0, uint32_t ph1) {
    spi_bus_config_t bus = {};
    bus.sclk_io_num = p.sclk;
    bus.mosi_io_num = p.mosi;
    bus.miso_io_num = -1;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    esp_err_t e = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_DISABLED);
    if (e != ESP_OK) return e;
    spi_device_interface_config_t dev = {};
    dev.mode = 2;                        // AD9834: data clocked on SCLK falling edge, SCLK idles high
    dev.clock_speed_hz = 10 * 1000 * 1000;
    dev.spics_io_num = p.fsync;
    dev.queue_size = 1;
    spi_device_handle_t h;
    if ((e = spi_bus_add_device(SPI2_HOST, &dev, &h)) != ESP_OK) { spi_bus_free(SPI2_HOST); return e; }
    const auto w16 = [&](uint16_t v) {
        uint8_t b[2] = {static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
        spi_transaction_t t = {};
        t.length = 16;
        t.tx_buffer = b;
        return spi_device_polling_transmit(h, &t);
    };
    constexpr uint16_t B28 = 1u << 13, PINSW = 1u << 9, RESET = 1u << 8, OPBITEN = 1u << 5, DIV2 = 1u << 3;
    const uint32_t ftw = dds_ftw(mclk, f, nullptr);
    const uint16_t p0 = static_cast<uint16_t>((ph0 >> 4) & 0x0FFF), p1 = static_cast<uint16_t>((ph1 >> 4) & 0x0FFF);
    const uint16_t seq[] = {
        static_cast<uint16_t>(B28 | RESET),                          // hold the accumulator while loading
        static_cast<uint16_t>(0x4000 | (ftw & 0x3FFF)),              // FREQ0 LSBs
        static_cast<uint16_t>(0x4000 | ((ftw >> 14) & 0x3FFF)),      // FREQ0 MSBs
        static_cast<uint16_t>(0xC000 | p0),                          // PHASE0
        static_cast<uint16_t>(0xE000 | p1),                          // PHASE1
        static_cast<uint16_t>(B28 | PINSW | OPBITEN | DIV2),         // run; FSELECT/PSELECT/RESET from the pins
    };
    for (uint16_t v : seq)
        if ((e = w16(v)) != ESP_OK) break;
    spi_bus_remove_device(h);
    spi_bus_free(SPI2_HOST);
    return e;
}

namespace {
struct Ms { uint32_t p1, p2, p3; };
Ms ms_params(uint32_t a, uint32_t b, uint32_t c) {   // AN619 eq. for multisynth / PLL feedback a + b/c
    const uint32_t f = static_cast<uint32_t>(128ull * b / c);
    return {128 * a + f - 512, 128 * b - c * f, c};
}
void pack(uint8_t* r, Ms m, uint8_t extra) {
    r[0] = static_cast<uint8_t>(m.p3 >> 8); r[1] = static_cast<uint8_t>(m.p3);
    r[2] = static_cast<uint8_t>(((m.p1 >> 16) & 0x03) | extra); r[3] = static_cast<uint8_t>(m.p1 >> 8);
    r[4] = static_cast<uint8_t>(m.p1); r[5] = static_cast<uint8_t>(((m.p3 >> 12) & 0xF0) | ((m.p2 >> 16) & 0x0F));
    r[6] = static_cast<uint8_t>(m.p2 >> 8); r[7] = static_cast<uint8_t>(m.p2);
}
}  // namespace

esp_err_t si5351_program(int sda, int scl, uint8_t addr, double xtal, double clk0, double f_lo, double* actual_lo) {
    i2c_master_bus_config_t bc = {};
    bc.i2c_port = I2C_NUM_0;
    bc.sda_io_num = static_cast<gpio_num_t>(sda);
    bc.scl_io_num = static_cast<gpio_num_t>(scl);
    bc.clk_source = I2C_CLK_SRC_DEFAULT;
    bc.glitch_ignore_cnt = 7;
    bc.flags.enable_internal_pullup = false;   // 4.7 k on the board
    i2c_master_bus_handle_t bus;
    esp_err_t e = i2c_new_master_bus(&bc, &bus);
    if (e != ESP_OK) return e;
    if ((e = i2c_master_probe(bus, addr, 50)) != ESP_OK) { i2c_del_master_bus(bus); return e; }   // absent: report
    i2c_device_config_t dc = {};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = addr;
    dc.scl_speed_hz = 400000;
    i2c_master_dev_handle_t dev;
    if ((e = i2c_master_bus_add_device(bus, &dc, &dev)) != ESP_OK) { i2c_del_master_bus(bus); return e; }
    const auto wr = [&](uint8_t reg, const uint8_t* d, size_t n) {
        uint8_t b[10];
        b[0] = reg;
        for (size_t i = 0; i < n; ++i) b[1 + i] = d[i];
        return i2c_master_transmit(dev, b, n + 1, 50);
    };
    const auto w1 = [&](uint8_t reg, uint8_t v) { return wr(reg, &v, 1); };
    uint8_t r[8];
    w1(3, 0xFF);                               // outputs off while programming
    // PLLA = 36 x 25 MHz = 900 MHz; MS0 = 18 (integer) -> 50 MHz
    const uint32_t pa = static_cast<uint32_t>(std::lround(clk0 * 18 / xtal));
    pack(r, ms_params(pa, 0, 1), 0); wr(26, r, 8);
    pack(r, ms_params(static_cast<uint32_t>(std::lround(xtal * pa / clk0)), 0, 1), 0); wr(42, r, 8);
    // CLK1 = 4 f_lo: R divider so the multisynth output stays >= 500 kHz, even integer MS1, fractional PLLB
    const double f1 = 4 * f_lo;
    uint8_t rdiv = 0;
    double out = f1;
    while (out < 500e3 && rdiv < 7) { out *= 2; ++rdiv; }
    uint32_t ms1 = static_cast<uint32_t>(900e6 / out) & ~1u;
    if (ms1 > 1800) ms1 = 1800;
    const double pll_b = out * ms1;
    const double ratio = pll_b / xtal;
    const uint32_t a = static_cast<uint32_t>(ratio), c = 1048575, b = static_cast<uint32_t>(std::lround((ratio - a) * c));
    pack(r, ms_params(a, b, c), 0); wr(34, r, 8);
    pack(r, ms_params(ms1, 0, 1), static_cast<uint8_t>(rdiv << 4)); wr(50, r, 8);
    w1(16, 0x4F);                              // CLK0: PLLA, multisynth, 8 mA
    w1(17, 0x6F);                              // CLK1: PLLB, multisynth, 8 mA
    w1(177, 0xA0);                             // reset PLLA and PLLB
    w1(3, 0xFC);                               // CLK0, CLK1 on
    if (actual_lo) *actual_lo = xtal * (a + static_cast<double>(b) / c) / ms1 / (1u << rdiv) / 4.0;
    i2c_master_bus_rm_device(dev);
    i2c_del_master_bus(bus);
    return ESP_OK;
}

}  // namespace fw
