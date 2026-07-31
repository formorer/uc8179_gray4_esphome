#include "uc8179_gray4.h"

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uc8179_gray4 {

static const char *const TAG = "uc8179_gray4";

// Grayscale waveform LUTs, copied verbatim from Seeed_GFX
// TFT_Drivers/UC8179_Defines.h (LUT_*_GRAY tables, 42 bytes each).
static const uint8_t LUT_VCOM_GRAY[42] = {
    0x00, 0x00, 0x06, 0x08, 0x07, 0x01,  //
    0x00, 0x06, 0x0A, 0x0B, 0x0A, 0x01,  //
    0x00, 0x03, 0x03, 0x00, 0x00, 0x03,  //
    0x00, 0x05, 0x09, 0x06, 0x06, 0x01,  //
    0x00, 0x02, 0x02, 0x0A, 0x0A, 0x01,  //
    0x00, 0x0A, 0x11, 0x06, 0x07, 0x01,  //
    0x00, 0x02, 0x01, 0x02, 0x01, 0x01,
};
static const uint8_t LUT_WW_GRAY[42] = {
    0x15, 0x00, 0x06, 0x08, 0x07, 0x01,  //
    0x54, 0x06, 0x0A, 0x0B, 0x0A, 0x01,  //
    0x90, 0x03, 0x03, 0x00, 0x00, 0x03,  //
    0x2A, 0x05, 0x09, 0x06, 0x06, 0x01,  //
    0xAA, 0x02, 0x02, 0x0A, 0x0A, 0x01,  //
    0x00, 0x0A, 0x11, 0x06, 0x07, 0x01,  //
    0x28, 0x02, 0x01, 0x02, 0x01, 0x01,
};
static const uint8_t LUT_KW_GRAY[42] = {
    0x2A, 0x00, 0x06, 0x08, 0x07, 0x01,  //
    0x59, 0x06, 0x0A, 0x0B, 0x0A, 0x01,  //
    0x90, 0x03, 0x03, 0x00, 0x00, 0x03,  //
    0x5A, 0x05, 0x09, 0x06, 0x06, 0x01,  //
    0xA8, 0x02, 0x02, 0x0A, 0x0A, 0x01,  //
    0x45, 0x0A, 0x11, 0x06, 0x07, 0x01,  //
    0xA8, 0x02, 0x01, 0x02, 0x01, 0x01,
};
static const uint8_t LUT_WK_GRAY[42] = {
    0x16, 0x00, 0x06, 0x08, 0x07, 0x01,  //
    0xA0, 0x06, 0x0A, 0x0B, 0x0A, 0x01,  //
    0x90, 0x03, 0x03, 0x00, 0x00, 0x03,  //
    0x99, 0x05, 0x09, 0x06, 0x06, 0x01,  //
    0xA0, 0x02, 0x02, 0x0A, 0x0A, 0x01,  //
    0x40, 0x0A, 0x11, 0x06, 0x07, 0x01,  //
    0x20, 0x02, 0x01, 0x02, 0x01, 0x01,
};
static const uint8_t LUT_KK_GRAY[42] = {
    0x26, 0x00, 0x06, 0x08, 0x07, 0x01,  //
    0x6A, 0x06, 0x0A, 0x0B, 0x0A, 0x01,  //
    0x90, 0x03, 0x03, 0x00, 0x00, 0x03,  //
    0x65, 0x05, 0x09, 0x06, 0x06, 0x01,  //
    0x50, 0x02, 0x02, 0x0A, 0x0A, 0x01,  //
    0x10, 0x0A, 0x11, 0x06, 0x07, 0x01,  //
    0x10, 0x02, 0x01, 0x02, 0x01, 0x01,
};

float UC8179Gray4::get_setup_priority() const {
  return setup_priority::PROCESSOR;
}

void UC8179Gray4::setup() {
  this->init_internal_(this->get_buffer_length_());
  if (this->buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate framebuffer (%u bytes)",
             this->get_buffer_length_());
    this->mark_failed();
    return;
  }
  // Start out white (0b11 per pixel), the natural e-paper background.
  memset(this->buffer_, 0xFF, this->get_buffer_length_());

  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(true);
  this->busy_pin_->setup();
  this->spi_setup();
}

void UC8179Gray4::update() {
  this->do_update_();
  if (this->is_failed())
    return;
  this->display_();
}

void UC8179Gray4::on_safe_shutdown() { this->deep_sleep_(); }

// --- framebuffer ------------------------------------------------------------

// Quantize to the panel's 4 levels: 0=black, 1=dark gray, 2=light gray,
// 3=white. Thresholds at 64/128/192 so Color(85,85,85) -> dark gray and
// Color(170,170,170) -> light gray.
static inline uint8_t color_to_gray2(Color color) {
  const uint32_t luma =
      (2126 * color.red + 7152 * color.green + 722 * color.blue) / 10000;
  return luma >> 6;
}

void UC8179Gray4::fill(Color color) {
  const uint8_t v = color_to_gray2(color);
  memset(this->buffer_, v * 0x55, this->get_buffer_length_());
}

void HOT UC8179Gray4::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= WIDTH || y >= HEIGHT || x < 0 || y < 0)
    return;

  const uint8_t v = color_to_gray2(color);
  const uint32_t pos = (x + y * WIDTH) >> 2;      // 4 pixels per byte
  const uint8_t shift = (3 - (x & 0x03)) << 1;    // MSB-first within the byte
  this->buffer_[pos] = (this->buffer_[pos] & ~(0x03 << shift)) | (v << shift);
}

// --- low-level SPI ----------------------------------------------------------

void UC8179Gray4::command(uint8_t value) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void UC8179Gray4::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void UC8179Gray4::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}

void UC8179Gray4::end_data_() { this->disable(); }

// UC8179 BUSY_N is active low: low = busy, high = idle. Configure the busy
// pin without `inverted: true` — a leftover inverted flag from a
// waveshare_epaper config flips the polarity: waits during actual idle then
// time out, while refreshes still complete (the panel runs them
// autonomously), so the image looks fine despite the errors.
bool UC8179Gray4::wait_until_idle_(const char *phase) {
  if (this->busy_pin_->digital_read())
    return true;

  const uint32_t start = millis();
  while (!this->busy_pin_->digital_read()) {
    if (millis() - start > IDLE_TIMEOUT_MS) {
      ESP_LOGE(TAG,
               "Timeout while waiting for the display to become idle (%s). If "
               "the image still renders fine, the busy pin is most likely "
               "configured with 'inverted: true' - remove that flag.",
               phase);
      return false;
    }
    App.feed_wdt();
    delay(1);
  }
  return true;
}

void UC8179Gray4::reset_() {
  this->reset_pin_->digital_write(false);
  delay(this->reset_duration_);  // NOLINT
  this->reset_pin_->digital_write(true);
  delay(10);
  this->panel_asleep_ = false;
  this->wait_until_idle_("after reset");
}

// --- grayscale init (ported from Seeed_GFX EPD_INIT_GRAY) --------------------

void UC8179Gray4::write_lut_bank_(uint8_t cmd, const uint8_t *lut) {
  this->wait_until_idle_("before LUT upload");
  this->command(cmd);
  this->start_data_();
  this->write_array(lut, 42);
  this->end_data_();
}

void UC8179Gray4::init_gray_custom_lut_() {
  this->command(0x01);  // POWER SETTING
  this->data(0x07);
  this->data(0x17);
  this->data(0x3F);
  this->data(0x3F);
  this->data(0x07);

  this->command(0x30);  // PLL CONTROL
  this->data(0x06);

  this->command(0x82);  // VCOM DC SETTING
  this->data(0x12);

  this->command(0x06);  // BOOSTER SOFT START
  this->data(0x27);
  this->data(0x27);
  this->data(0x28);
  this->data(0x17);

  this->command(0x04);  // POWER ON
  delay(100);           // NOLINT
  this->wait_until_idle_("after power on");

  this->command(0x00);  // PANEL SETTING: KW mode, LUT from registers
  this->data(0x3F);

  this->command(0xE3);  // POWER SAVING
  this->data(0x88);

  this->command(0x50);  // VCOM AND DATA INTERVAL SETTING
  this->data(0x10);
  this->data(0x07);

  this->command(0x52);
  this->data(0x00);

  this->command(0x61);  // RESOLUTION SETTING
  this->data(WIDTH >> 8);
  this->data(WIDTH & 0xFF);
  this->data(HEIGHT >> 8);
  this->data(HEIGHT & 0xFF);

  this->write_lut_bank_(0x20, LUT_VCOM_GRAY);
  this->write_lut_bank_(0x21, LUT_WW_GRAY);
  this->write_lut_bank_(0x22, LUT_KW_GRAY);
  this->write_lut_bank_(0x23, LUT_WK_GRAY);
  this->write_lut_bank_(0x24, LUT_KK_GRAY);
}

void UC8179Gray4::init_gray_otp_() {
  this->command(0x01);  // POWER SETTING
  this->data(0x07);
  this->data(0x07);
  this->data(0x3F);
  this->data(0x3F);

  this->command(0x06);  // BOOSTER SOFT START
  this->data(0x27);
  this->data(0x27);
  this->data(0x18);
  this->data(0x17);

  this->command(0x04);  // POWER ON
  delay(100);           // NOLINT
  this->wait_until_idle_("after power on");

  this->command(0x00);  // PANEL SETTING: KW mode, LUT from OTP
  this->data(0x1F);

  this->command(0x61);  // RESOLUTION SETTING
  this->data(WIDTH >> 8);
  this->data(WIDTH & 0xFF);
  this->data(HEIGHT >> 8);
  this->data(HEIGHT & 0xFF);

  this->command(0x50);  // VCOM AND DATA INTERVAL SETTING
  this->data(0x10);
  this->data(0x07);

  this->command(0xE0);  // CASCADE SETTING
  this->data(0x02);
  this->command(0xE5);  // FORCED TEMPERATURE: selects the OTP gray waveform
  this->data(0x5F);
}

// --- refresh ----------------------------------------------------------------

// Plane bit mapping per pixel value (from Seeed_GFX EPD_PUSH_NEW_GRAY_COLORS):
//   value 3 (white):      DTM1=1 DTM2=1
//   value 2 (light gray): DTM1=0 DTM2=1
//   value 1 (dark gray):  DTM1=1 DTM2=0
//   value 0 (black):      DTM1=0 DTM2=0
// i.e. DTM1 (0x10) carries the LSB of each 2-bit value, DTM2 (0x13) the MSB.
void UC8179Gray4::write_plane_(uint8_t cmd, uint8_t bit_index) {
  static const uint16_t BYTES_PER_LINE = WIDTH / 8;  // 100
  uint8_t line[BYTES_PER_LINE];

  this->command(cmd);
  this->start_data_();
  for (uint32_t row = 0; row < HEIGHT; row++) {
    const uint8_t *src = this->buffer_ + row * (WIDTH / 4);
    for (uint32_t j = 0; j < BYTES_PER_LINE; j++) {
      const uint8_t b0 = src[j * 2];      // pixels 0..3
      const uint8_t b1 = src[j * 2 + 1];  // pixels 4..7
      uint8_t out = 0;
      out |= (((b0 >> (6 + bit_index)) & 0x01) << 7);
      out |= (((b0 >> (4 + bit_index)) & 0x01) << 6);
      out |= (((b0 >> (2 + bit_index)) & 0x01) << 5);
      out |= (((b0 >> (0 + bit_index)) & 0x01) << 4);
      out |= (((b1 >> (6 + bit_index)) & 0x01) << 3);
      out |= (((b1 >> (4 + bit_index)) & 0x01) << 2);
      out |= (((b1 >> (2 + bit_index)) & 0x01) << 1);
      out |= (((b1 >> (0 + bit_index)) & 0x01) << 0);
      line[j] = out;
    }
    this->write_array(line, BYTES_PER_LINE);
    App.feed_wdt();
  }
  this->end_data_();
}

bool UC8179Gray4::refresh_() {
  this->command(0x12);  // DISPLAY REFRESH
  delay(1);             // per vendor code: at least 200 us before polling busy
  if (!this->wait_until_idle_("during refresh")) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  return true;
}

void UC8179Gray4::display_() {
  // The panel is put into deep sleep after every refresh, so each cycle starts
  // with a hardware reset and a full grayscale re-init (Seeed EPD_WAKEUP_GRAY).
  this->reset_();
  if (this->lut_mode_ == LUT_MODE_OTP) {
    this->init_gray_otp_();
  } else {
    this->init_gray_custom_lut_();
  }

  this->write_plane_(0x10, 0);  // DTM1: LSB plane
  this->write_plane_(0x13, 1);  // DTM2: MSB plane
  this->refresh_();

  this->deep_sleep_();
}

void UC8179Gray4::deep_sleep_() {
  if (this->panel_asleep_)
    return;

  this->command(0x50);  // border floating
  this->data(0xF7);
  this->command(0x02);  // POWER OFF
  if (!this->wait_until_idle_("after power off"))
    return;
  this->command(0x07);  // DEEP SLEEP
  this->data(0xA5);
  this->panel_asleep_ = true;
}

void UC8179Gray4::dump_config() {
  LOG_DISPLAY("", "UC8179 4-Gray e-Paper", this)
  ESP_LOGCONFIG(TAG, "  Panel: GDEY075T7 800x480 (reTerminal E1001)");
  ESP_LOGCONFIG(TAG, "  LUT mode: %s",
                this->lut_mode_ == LUT_MODE_OTP ? "otp" : "custom");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace uc8179_gray4
}  // namespace esphome
