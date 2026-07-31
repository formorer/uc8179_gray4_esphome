#pragma once

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

namespace esphome {
namespace uc8179_gray4 {

// How the 4-gray waveform is sourced:
//  - LUT_MODE_CUSTOM: upload the grayscale LUTs from Seeed_GFX via 0x20..0x24
//    (panel setting 0x00 = 0x3F, LUT from registers). Works on the original
//    reTerminal E1001 panel batches.
//  - LUT_MODE_OTP: use the panel-internal OTP grayscale waveform (panel
//    setting 0x00 = 0x1F plus cascade 0xE0=0x02 / forced temperature
//    0xE5=0x5F). Newer panel batches carry an OTP marker and expect this path.
// Seeed_GFX probes the panel OTP at runtime to pick a path; that requires
// reading back over the write-only SPI wiring, so here it is a config option.
// If gray levels come out wrong or washed out, switch to the other mode.
enum LutMode {
  LUT_MODE_CUSTOM = 0,
  LUT_MODE_OTP,
};

// 4-level grayscale driver for the Good Display GDEY075T7 800x480 panel
// (UC8179 controller), as used in the Seeed Studio reTerminal E1001.
//
// Framebuffer is 2 bits per pixel (0=black, 1=dark gray, 2=light gray,
// 3=white). Each refresh sends the two grayscale bit-planes: the LSB plane
// via DTM1 (0x10) and the MSB plane via DTM2 (0x13), then triggers a full
// refresh (0x12). Grayscale is full-refresh only; there is no partial mode.
//
// Command sequences ported from Seeed_GFX (TFT_Drivers/UC8179_Defines.h,
// board/screen combo 520 = reTerminal E1001), MIT licensed.
class UC8179Gray4
    : public display::DisplayBuffer,
      public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                            spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  static const uint16_t WIDTH = 800;
  static const uint16_t HEIGHT = 480;

  // A B/W full refresh takes ~3.5 s typ.; the grayscale waveform is longer and
  // both slow down markedly in the cold, so allow plenty of headroom.
  static const uint32_t IDLE_TIMEOUT_MS = 30000;

  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset) { this->reset_pin_ = reset; }
  void set_busy_pin(GPIOPin *busy) { this->busy_pin_ = busy; }
  void set_reset_duration(uint32_t reset_duration) {
    this->reset_duration_ = reset_duration;
  }
  void set_lut_mode(LutMode mode) { this->lut_mode_ = mode; }

  float get_setup_priority() const override;

  void setup() override;
  void update() override;
  void dump_config() override;
  void on_safe_shutdown() override;

  void fill(Color color) override;

  display::DisplayType get_display_type() override {
    return display::DisplayType::DISPLAY_TYPE_GRAYSCALE;
  }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return WIDTH; }
  int get_height_internal() override { return HEIGHT; }

  uint32_t get_buffer_length_() const {
    return WIDTH * HEIGHT / 4u;  // 2 bits per pixel
  }

  void command(uint8_t value);
  void data(uint8_t value);
  void start_data_();
  void end_data_();

  bool wait_until_idle_(const char *phase);
  void reset_();

  void display_();
  void init_gray_custom_lut_();
  void init_gray_otp_();
  void write_lut_bank_(uint8_t cmd, const uint8_t *lut);
  // Extract one bit per pixel from the 2bpp framebuffer and stream it as a
  // plane: bit_index 0 = LSB plane (DTM1/0x10), 1 = MSB plane (DTM2/0x13).
  void write_plane_(uint8_t cmd, uint8_t bit_index);
  bool refresh_();
  void deep_sleep_();

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
  uint32_t reset_duration_{10};
  LutMode lut_mode_{LUT_MODE_CUSTOM};
  // A panel in deep sleep holds BUSY low, so a second sleep sequence would
  // stall in wait_until_idle_ until the timeout. Track the state and skip it
  // (matters on battery devices, where on_safe_shutdown runs right after the
  // per-refresh sleep before the MCU itself sleeps).
  bool panel_asleep_{false};
};

}  // namespace uc8179_gray4
}  // namespace esphome
