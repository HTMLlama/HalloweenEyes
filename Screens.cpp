// Screens.cpp -- see Screens.h.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O2")
#endif

#include "Screens.h"

#include <stdio.h>

#include "EyeMath.h"

namespace {

// Classic 5x7 font, ASCII 32..95. Column-major, bit 0 = top row.
const uint8_t kFont[64][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},  //   !
    {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},  // " #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},  // $ %
    {0x36, 0x49, 0x56, 0x20, 0x50}, {0x00, 0x08, 0x07, 0x03, 0x00},  // & '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},  // ( )
    {0x2A, 0x1C, 0x7F, 0x1C, 0x2A}, {0x08, 0x08, 0x3E, 0x08, 0x08},  // * +
    {0x00, 0x80, 0x70, 0x30, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},  // , -
    {0x00, 0x00, 0x60, 0x60, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},  // . /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},  // 0 1
    {0x72, 0x49, 0x49, 0x49, 0x46}, {0x21, 0x41, 0x49, 0x4D, 0x33},  // 2 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},  // 4 5
    {0x3C, 0x4A, 0x49, 0x49, 0x31}, {0x41, 0x21, 0x11, 0x09, 0x07},  // 6 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x46, 0x49, 0x49, 0x29, 0x1E},  // 8 9
    {0x00, 0x00, 0x14, 0x00, 0x00}, {0x00, 0x40, 0x34, 0x00, 0x00},  // : ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},  // < =
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x59, 0x09, 0x06},  // > ?
    {0x3E, 0x41, 0x5D, 0x59, 0x4E}, {0x7C, 0x12, 0x11, 0x12, 0x7C},  // @ A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},  // B C
    {0x7F, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x49, 0x49, 0x49, 0x41},  // D E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x41, 0x51, 0x73},  // F G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},  // H I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},  // J K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x1C, 0x02, 0x7F},  // L M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},  // N O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},  // P Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x26, 0x49, 0x49, 0x49, 0x32},  // R S
    {0x03, 0x01, 0x7F, 0x01, 0x03}, {0x3F, 0x40, 0x40, 0x40, 0x3F},  // T U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F},  // V W
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x03, 0x04, 0x78, 0x04, 0x03},  // X Y
    {0x61, 0x59, 0x49, 0x4D, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},  // Z [
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00},  // \ ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},  // ^ _
};

inline void plotMax(uint16_t* fb, int x, int y, int r, int g, int b) {
  if ((unsigned)x >= EYE_W || (unsigned)y >= EYE_H) return;
  uint16_t* p = &fb[y * EYE_W + x];
  int pr, pg, pb;
  unpack565(swap16(*p), pr, pg, pb);
  if (r > pr) pr = r;
  if (g > pg) pg = g;
  if (b > pb) pb = b;
  *p = swap16(pack565d(pr, pg, pb, 0));
}

void textPass(uint16_t* fb, int x, int y, const char* s, int r, int g, int b,
              int scale, int dx, int dy) {
  for (int cx = x; *s; s++, cx += 6 * scale) {
    int c = (unsigned char)*s;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 95) c = '?';
    const uint8_t* gl = kFont[c - 32];
    for (int col = 0; col < 5; col++) {
      uint8_t bits = gl[col];
      for (int row = 0; row < 8; row++) {
        if (!(bits & (1 << row))) continue;
        for (int sy = 0; sy < scale; sy++)
          for (int sx = 0; sx < scale; sx++)
            plotMax(fb, cx + col * scale + sx + dx, y + row * scale + sy + dy, r, g, b);
      }
    }
  }
}

// Text with a soft phosphor glow.
void glowText(uint16_t* fb, int x, int y, const char* s, uint32_t rgb, int scale) {
  int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  const int gr = r / 5, gg = g / 5, gb = b / 5;
  textPass(fb, x, y, s, gr, gg, gb, scale, -1, 0);
  textPass(fb, x, y, s, gr, gg, gb, scale, 1, 0);
  textPass(fb, x, y, s, gr, gg, gb, scale, 0, -1);
  textPass(fb, x, y, s, gr, gg, gb, scale, 0, 1);
  textPass(fb, x, y, s, r, g, b, scale, 0, 0);
}

void fillRect(uint16_t* fb, int x, int y, int w, int h, uint32_t rgb) {
  int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) plotMax(fb, xx, yy, r, g, b);
}

void batteryIcon(uint16_t* fb, int x, int y, int pct, uint32_t rgb, uint32_t fillRgb) {
  // 15x8 body with a 2x4 nub.
  fillRect(fb, x, y, 15, 1, rgb);
  fillRect(fb, x, y + 7, 15, 1, rgb);
  fillRect(fb, x, y, 1, 8, rgb);
  fillRect(fb, x + 14, y, 1, 8, rgb);
  fillRect(fb, x + 15, y + 2, 2, 4, rgb);
  int w = (pct * 11 + 50) / 100;
  if (w > 0) fillRect(fb, x + 2, y + 2, w, 4, fillRgb);
}

}  // namespace

int textWidth(const char* s, int scale) {
  int n = (int)strlen(s);
  return n ? (n * 6 - 1) * scale : 0;
}

void drawText(uint16_t* fb, int x, int y, const char* s, uint32_t rgb, int scale) {
  textPass(fb, x, y, s, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, scale, 0, 0);
}

void screenFill(uint16_t* fb, uint32_t rgb) {
  uint16_t v = swap16(hex565(rgb));
  for (int i = 0; i < EYE_W * EYE_H; i++) fb[i] = v;
}

void screenStandby(uint16_t* fb, const StandbyInfo& info, uint32_t timeMs,
                   uint32_t seed) {
  const uint32_t kText = 0x46FF7A, kDim = 0x1E9C44, kWarn = 0xFFB020;
  // Jitter changes in steps, like a tracking-challenged CRT.
  uint32_t step = timeMs / 70;
  uint32_t h = hash32(step * 2654435761u ^ seed);
  int jx = (int)(h % 5) - 2, jy = (int)((h >> 8) % 3) - 1;
  if (((h >> 16) & 31) == 0) jx += ((h >> 21) & 1) ? 5 : -5;  // occasional big hop
  const float t = timeMs * 0.001f;
  const float barY = fastFrac(t * 0.6f) * 200.0f - 36.0f;
  const uint32_t frame = timeMs / 33;

  // Background: dark phosphor, vignette, scanlines, static, roll bar.
  for (int y = 0; y < EYE_H; y++) {
    float dy = (y - 63.5f) / 64.0f;
    float bar = (y - barY) / 14.0f;
    float rowK = ((y & 1) ? 0.7f : 1.0f) * (1.0f + 0.9f * expf(-bar * bar));
    for (int x = 0; x < EYE_W; x++) {
      float dx = (x - 63.5f) / 64.0f;
      float vig = 1.0f - 0.55f * (dx * dx + dy * dy);
      uint32_t n = hash2i(x, y, frame * 7919u + seed) & 31;
      float g = (9.0f + n * 0.55f) * vig * rowK;
      float r = g * 0.25f, b = g * 0.4f;
      const int d = kBayer4[((y & 3) << 2) | (x & 3)] >> 1;
      fb[y * EYE_W + x] = swap16(pack565d((int)r, (int)g, (int)b, d));
    }
  }

  char buf[24];
  // Battery (top left).
  if (info.batteryPct >= 0) {
    bool blinkOff = info.lowBattery && ((timeMs / 400) & 1);
    uint32_t c = info.lowBattery ? kWarn : kText;
    uint32_t fillC = info.batteryPct > 40 ? kText : (info.batteryPct > 15 ? 0xD8FF40 : kWarn);
    if (!blinkOff) batteryIcon(fb, 6 + jx, 6 + jy, info.batteryPct, c, fillC);
    snprintf(buf, sizeof(buf), "%d%%", info.batteryPct);
    glowText(fb, 27 + jx, 6 + jy, buf, c, 1);
  }
  // Channel (top right).
  if (info.channel > 0) {
    snprintf(buf, sizeof(buf), "CH %02d", info.channel);
    glowText(fb, 122 - textWidth(buf, 1) + jx, 6 + jy, buf, kText, 1);
  }
  // Headline.
  if (info.line1)
    glowText(fb, (EYE_W - textWidth(info.line1, 2)) / 2 + jx, 36 + jy, info.line1, kText, 2);
  if (info.line2)
    glowText(fb, (EYE_W - textWidth(info.line2, 2)) / 2 + jx, 56 + jy, info.line2, kText, 2);
  // Detail line with a blinking cursor.
  if (info.detail) {
    snprintf(buf, sizeof(buf), "> %s%s", info.detail, ((timeMs / 350) & 1) ? "_" : " ");
    int w = textWidth(buf, 1);
    glowText(fb, (EYE_W - w) / 2 + jx, 86 + jy, buf, kDim, 1);
  }
  // Footer: voltage, or a low battery warning.
  if (info.batteryPct >= 0) {
    if (info.lowBattery) {
      if (!((timeMs / 400) & 1))
        glowText(fb, (EYE_W - textWidth("LOW BATTERY", 1)) / 2 + jx, 110 + jy, "LOW BATTERY", kWarn, 1);
    } else {
      snprintf(buf, sizeof(buf), "%d.%02dV", (int)info.batteryV,
               (int)(info.batteryV * 100.0f + 0.5f) % 100);
      glowText(fb, (EYE_W - textWidth(buf, 1)) / 2 + jx, 110 + jy, buf, kDim, 1);
    }
  }

  // Occasional horizontal tear.
  if (((h >> 11) & 7) == 0) {
    uint16_t tmp[EYE_W];
    int y0 = 10 + (int)((h >> 3) % 100), shift = 3 + (int)((h >> 14) % 8);
    for (int y = y0; y < y0 + 4 && y < EYE_H; y++) {
      uint16_t* row = &fb[y * EYE_W];
      for (int x = 0; x < EYE_W; x++) tmp[x] = row[imod(x - shift, EYE_W)];
      memcpy(row, tmp, sizeof(tmp));
    }
  }
}

void crtSqueeze(uint16_t* fb, float squeeze) {
  squeeze = clampf(squeeze, 0.012f, 1.0f);
  const float boost = 1.0f + 1.6f * (1.0f - squeeze);
  const float inv = 1.0f / squeeze;
  const float c = 64.0f;
  // Work from the centre outward: sources are always further from the centre
  // than their destinations, so nothing is overwritten before it is read.
  for (int k = 0; k < 64; k++) {
    for (int side = 0; side < 2; side++) {
      int y = side ? 64 + k : 63 - k;
      float src = c + (y + 0.5f - c) * inv - 0.5f;
      int sy = (int)(src + 0.5f);
      uint16_t* dst = &fb[y * EYE_W];
      if (sy < 0 || sy >= EYE_H) {
        memset(dst, 0, EYE_W * 2);
        continue;
      }
      const uint16_t* s = &fb[sy * EYE_W];
      for (int x = 0; x < EYE_W; x++) {
        int r, g, b;
        unpack565(swap16(s[x]), r, g, b);
        if (boost > 1.01f) {
          r = (int)(r * boost) + 12;
          g = (int)(g * boost) + 12;
          b = (int)(b * boost) + 16;
        }
        dst[x] = swap16(pack565d(r, g, b, 0));
      }
    }
  }
}

void crtLine(uint16_t* fb, float width, float intensity) {
  memset(fb, 0, EYE_W * EYE_H * 2);
  const float half = width * 0.5f;
  for (int y = 60; y < 68; y++) {
    float dy = fabsf(y + 0.5f - 64.0f);
    float vy = expf(-dy * dy / 1.4f);
    for (int x = 0; x < EYE_W; x++) {
      float dx = fabsf(x + 0.5f - 64.0f) - half;
      float vx = dx < 0.0f ? 1.0f : expf(-dx * dx / 3.0f);
      float v = vx * vy * intensity;
      if (v < 0.01f) continue;
      fb[y * EYE_W + x] = swap16(pack565d((int)(235 * v), (int)(245 * v), (int)(255 * v), 0));
    }
  }
}
