// Screens.h -- non-eye screens and CRT effects: the "PLEASE STANDBY" card,
// the boot card, text, and the CRT collapse/expand used between styles.
// Portable (no Arduino dependencies); writes big-endian RGB565 like the
// renderer.
#pragma once

#include <stdint.h>

struct StandbyInfo {
  const char* line1;      // big headline, e.g. "PLEASE"
  const char* line2;      // big headline, e.g. "STANDBY"
  const char* detail;     // small line under the headline (style name)
  int channel;            // shown as "CH 07" (0 hides it)
  int batteryPct;         // < 0 hides the battery
  float batteryV;
  bool lowBattery;
};

// Draws the CRT standby card. 'seed' varies per eye so the two panels jitter
// independently.
void screenStandby(uint16_t* fb, const StandbyInfo& info, uint32_t timeMs,
                   uint32_t seed);

// CRT power effects, applied in place to a finished frame.
//   squeeze: 1 = full picture, ~0 = collapsed to a line (with brightening).
void crtSqueeze(uint16_t* fb, float squeeze);
//   Bright horizontal line of the given width (px) and intensity, centred.
void crtLine(uint16_t* fb, float width, float intensity);

void screenFill(uint16_t* fb, uint32_t rgb);
void drawText(uint16_t* fb, int x, int y, const char* s, uint32_t rgb,
              int scale);
int textWidth(const char* s, int scale);
