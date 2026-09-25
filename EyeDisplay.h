// EyeDisplay.h -- SSD1351 OLED with non-blocking DMA frame pushes.
//
// Adafruit_SSD1351 handles reset, init and rotation exactly as before; pixel
// data then streams straight from the framebuffer by DMA (the SAMD core's
// SPI.transfer(..., block=false)), so the CPU renders the other eye while this
// one transmits. Framebuffers must hold big-endian RGB565.
#pragma once

#include <Adafruit_SSD1351.h>
#include <Arduino.h>

class EyeDisplay {
 public:
  EyeDisplay(int8_t cs, int8_t dc, int8_t rst);

  void begin(uint32_t spiHz, uint8_t rotation);

  // Start sending rows y0..y1 (inclusive) of a 128x128 frame and return
  // immediately. Only one display may transfer at a time (shared SPI bus):
  // call finish() on the active one before starting another.
  void pushAsync(const uint16_t* frame, int y0, int y1);
  void finish();
  bool busy() const { return active_; }

  void setContrast(uint8_t level);  // 0..15
  void setGamma(bool on);
  void setEnabled(bool on);         // panel on / off (sleep)

 private:
  Adafruit_SSD1351 tft_;
  bool active_ = false;
};
