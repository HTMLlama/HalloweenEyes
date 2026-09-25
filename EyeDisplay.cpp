// EyeDisplay.cpp -- see EyeDisplay.h.
#include "EyeDisplay.h"

#include <SPI.h>

namespace {

// The SERCOM behind the default SPI object, for the end-of-transfer check.
Sercom* spiSercom() {
  SERCOM* s = &PERIPH_SPI;
  if (s == &sercom0) return SERCOM0;
  if (s == &sercom1) return SERCOM1;
  if (s == &sercom2) return SERCOM2;
  if (s == &sercom3) return SERCOM3;
  if (s == &sercom4) return SERCOM4;
  if (s == &sercom5) return SERCOM5;
  return nullptr;
}

// Grey-scale pulse widths for GS1..GS63: a gentle gamma curve (about 1.8)
// from the panel vendor's reference init code.
const uint8_t kGammaTable[63] = {
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
    0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x15, 0x17, 0x19, 0x1B,
    0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x2A, 0x2D, 0x30, 0x33, 0x36,
    0x39, 0x3C, 0x3F, 0x42, 0x45, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C,
    0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7D, 0x82, 0x87, 0x8C,
    0x91, 0x96, 0x9B, 0xA0, 0xA5, 0xAA, 0xAF, 0xB4};

}  // namespace

EyeDisplay::EyeDisplay(int8_t cs, int8_t dc, int8_t rst)
    : tft_(128, 128, &SPI, cs, dc, rst) {}

void EyeDisplay::begin(uint32_t spiHz, uint8_t rotation) {
#if defined(__SAMD51__)
  // The default 48 MHz SPI clock only divides to 24 or 12 MHz. For anything
  // in between, run the SERCOM from the 100 MHz clock: 100 / 6 = 16.7 MHz.
  // (Requesting exactly 16.67 MHz would round the divider down to 25 MHz.)
  if (spiHz > 12000000 && spiHz < 24000000) {
    SPI.setClockSource(SERCOM_CLOCK_SOURCE_100M);
    spiHz = 16000000;
  }
#endif
  tft_.begin(spiHz);
  tft_.setRotation(rotation);
  tft_.fillScreen(0);
}

void EyeDisplay::pushAsync(const uint16_t* frame, int y0, int y1) {
  if (y0 < 0) y0 = 0;
  if (y1 > 127) y1 = 127;
  if (y1 < y0) return;
  tft_.startWrite();
  tft_.setAddrWindow(0, y0, 128, y1 - y0 + 1);
  SPI.transfer(&frame[y0 * 128], nullptr, (size_t)(y1 - y0 + 1) * 256, false);
  active_ = true;
}

void EyeDisplay::finish() {
  if (!active_) return;
  SPI.waitForTransfer();
  // DMA is done when the last byte enters the SPI data register; wait for it
  // to leave the shift register before releasing chip select, then drop the
  // bytes the receiver collected so the next command transfer starts clean.
  Sercom* hw = spiSercom();
  if (hw) {
    while (!hw->SPI.INTFLAG.bit.TXC) {
    }
    while (hw->SPI.INTFLAG.bit.RXC) (void)hw->SPI.DATA.reg;
    hw->SPI.STATUS.reg = SERCOM_SPI_STATUS_BUFOVF;
  }
  tft_.endWrite();
  active_ = false;
}

void EyeDisplay::setContrast(uint8_t level) {
  if (level > 15) level = 15;
  tft_.sendCommand(SSD1351_CMD_CONTRASTMASTER, &level, 1);
}

void EyeDisplay::setGamma(bool on) {
  if (on)
    tft_.sendCommand(SSD1351_CMD_SETGRAY, kGammaTable, sizeof(kGammaTable));
  else
    tft_.sendCommand(SSD1351_CMD_USELUT);
}

void EyeDisplay::setEnabled(bool on) { tft_.enableDisplay(on); }
