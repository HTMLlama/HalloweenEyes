// EyeMath.h -- small portable helpers shared by the eye engine: fast math,
// a PRNG, hash/value noise and colour packing.
//
// Nothing in the eye engine (renderer, styles, baker, behaviour, screens)
// includes Arduino.h, so the exact same code also builds on a desktop for the
// preview tool in extras/preview.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#define EYE_PI 3.14159265f
#define EYE_TAU 6.28318531f

// Panel size. Both SSD1351 OLEDs are 128x128.
#define EYE_W 128
#define EYE_H 128

// ---------------------------------------------------------------------------
// Math
// ---------------------------------------------------------------------------

#if defined(__arm__) && defined(__ARM_FP)
// Single VSQRT instruction; avoids the errno path that sqrtf() drags in.
static inline float fsqrt(float x) {
  float r;
  __asm__("vsqrt.f32 %0, %1" : "=t"(r) : "t"(x));
  return r;
}
#else
static inline float fsqrt(float x) { return sqrtf(x); }
#endif

static inline float clampf(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
static inline int clampi(int x, int lo, int hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float saturate(float x) { return clampf(x, 0.0f, 1.0f); }
static inline float smoothstep(float e0, float e1, float x) {
  float t = saturate((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }

// Cortex-M4 has no rounding instruction, so floorf() is a library call.
static inline float fastFloor(float x) {
  int i = (int)x;
  return (float)(x < (float)i ? i - 1 : i);
}
static inline float fastFrac(float x) { return x - fastFloor(x); }
static inline float fastCeil(float x) {
  int i = (int)x;
  return (float)(x > (float)i ? i + 1 : i);
}

// atan2 approximation, max error ~0.0015 rad. Plenty for texture work.
static inline float fastAtan2(float y, float x) {
  float ax = fabsf(x), ay = fabsf(y);
  float mn = ax < ay ? ax : ay;
  float mx = ax < ay ? ay : ax;
  float a = mn / (mx + 1e-20f);
  float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0.0f) r = 3.14159274f - r;
  if (y < 0.0f) r = -r;
  return r;
}

// Minimum-jerk ease (the velocity profile real saccades follow).
static inline float easeMinJerk(float t) {
  t = saturate(t);
  return t * t * t * (10.0f + t * (-15.0f + 6.0f * t));
}
static inline float easeInQuad(float t) { t = saturate(t); return t * t; }
static inline float easeOutQuad(float t) {
  t = saturate(t);
  return 1.0f - (1.0f - t) * (1.0f - t);
}

// ---------------------------------------------------------------------------
// Random numbers
// ---------------------------------------------------------------------------

struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed = 0x2545F491u) : s(seed ? seed : 0x9E3779B9u) {}
  void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
  inline uint32_t next() {  // xorshift32
    uint32_t x = s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s = x;
  }
  inline float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); }
  inline float range(float a, float b) { return a + (b - a) * uniform(); }
  inline int irange(int lo, int hiInclusive) {
    return lo + (int)(next() % (uint32_t)(hiInclusive - lo + 1));
  }
  inline bool chance(float p) { return uniform() < p; }
};

static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}
static inline uint32_t hash2i(int32_t x, int32_t y, uint32_t seed) {
  return hash32((uint32_t)x * 0x8da6b343U ^ (uint32_t)y * 0xd8163841U ^
                seed * 0xcb1ab31fU);
}
static inline float hash2f(int32_t x, int32_t y, uint32_t seed) {
  return (hash2i(x, y, seed) >> 8) * (1.0f / 16777216.0f);
}

static inline int imod(int a, int m) {
  int r = a % m;
  return r < 0 ? r + m : r;
}

// 2D value noise in [0,1]. If periodX > 0 the noise wraps along x with that
// period (used for angular coordinates so irises have no seam).
static inline float valueNoise(float x, float y, uint32_t seed, int periodX) {
  float fx = floorf(x), fy = floorf(y);
  int ix = (int)fx, iy = (int)fy;
  float ux = x - fx, uy = y - fy;
  ux = ux * ux * (3.0f - 2.0f * ux);
  uy = uy * uy * (3.0f - 2.0f * uy);
  int ix1 = ix + 1;
  if (periodX > 0) {
    ix = imod(ix, periodX);
    ix1 = imod(ix1, periodX);
  }
  float a = hash2f(ix, iy, seed), b = hash2f(ix1, iy, seed);
  float c = hash2f(ix, iy + 1, seed), d = hash2f(ix1, iy + 1, seed);
  return lerpf(lerpf(a, b, ux), lerpf(c, d, ux), uy);
}

// Fractal sum of value noise, normalised to [0,1].
static inline float fbm(float x, float y, uint32_t seed, int octaves,
                        int periodX) {
  float sum = 0.0f, amp = 0.5f, norm = 0.0f;
  for (int i = 0; i < octaves; i++) {
    sum += amp * valueNoise(x, y, seed + (uint32_t)i * 1013u, periodX);
    norm += amp;
    x *= 2.0f;
    y *= 2.0f;
    periodX *= 2;
    amp *= 0.5f;
  }
  return sum / norm;
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
// Styles use 0xRRGGBB literals. Framebuffers hold RGB565 already byte-swapped
// (big-endian) so DMA can stream them to the SSD1351 without CPU work.

struct Col {
  float r, g, b;  // 0..1 (may exceed 1 before clamping)
};

static inline Col colHex(uint32_t c) {
  Col o = {((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f,
           (c & 255) / 255.0f};
  return o;
}
static inline Col colRGB(float r, float g, float b) {
  Col o = {r, g, b};
  return o;
}
static inline Col mixc(Col a, Col b, float t) {
  Col o = {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
  return o;
}
static inline Col mulc(Col a, float k) {
  Col o = {a.r * k, a.g * k, a.b * k};
  return o;
}
static inline Col addc(Col a, Col b) {
  Col o = {a.r + b.r, a.g + b.g, a.b + b.b};
  return o;
}

static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

// 8-bit channels -> RGB565 with an ordered-dither offset d (0..7).
static inline uint16_t pack565d(int r, int g, int b, int d) {
  r += d;
  g += d >> 1;
  b += d;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  if (r < 0) r = 0;
  if (g < 0) g = 0;
  if (b < 0) b = 0;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t col565(Col c, int d) {
  return pack565d((int)(c.r * 255.0f), (int)(c.g * 255.0f), (int)(c.b * 255.0f),
                  d);
}
static inline uint16_t hex565(uint32_t c) {
  return (uint16_t)((((c >> 16) & 0xF8) << 8) | (((c >> 8) & 0xFC) << 3) |
                    ((c & 255) >> 3));
}

// Unpack native-order RGB565 to 8-bit channels.
static inline void unpack565(uint16_t c, int& r, int& g, int& b) {
  r = (c >> 11) & 31;
  g = (c >> 5) & 63;
  b = c & 31;
  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);
}

// 4x4 ordered dither matrix, values 0..15.
static const uint8_t kBayer4[16] = {0, 8,  2, 10, 12, 4, 14, 6,
                                    3, 11, 1, 9,  15, 7, 13, 5};

// ---------------------------------------------------------------------------
// Packed colour: 0x00RRGGBB in one register. All three channels are scaled
// with two multiplies, which is far cheaper on a Cortex-M4 than per-channel
// float maths. Weights are 0..256.
// ---------------------------------------------------------------------------

static inline uint32_t c32(int r, int g, int b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
static inline uint32_t c32scale(uint32_t c, uint32_t a) {
  uint32_t rb = ((c & 0xFF00FFu) * a) >> 8;
  uint32_t g = ((c & 0x00FF00u) * a) >> 8;
  return (rb & 0xFF00FFu) | (g & 0x00FF00u);
}
static inline uint32_t c32lerp(uint32_t c0, uint32_t c1, uint32_t a) {
  uint32_t ia = 256 - a;
  uint32_t rb = ((c0 & 0xFF00FFu) * ia + (c1 & 0xFF00FFu) * a) >> 8;
  uint32_t g = ((c0 & 0x00FF00u) * ia + (c1 & 0x00FF00u) * a) >> 8;
  return (rb & 0xFF00FFu) | (g & 0x00FF00u);
}
// Per-channel saturating add (one UQADD8 instruction on the M4).
static inline uint32_t c32add(uint32_t a, uint32_t b) {
#if defined(__arm__) && defined(__ARM_ARCH_7EM__)
  uint32_t r;
  __asm__("uqadd8 %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
  return r;
#else
  uint32_t r = 0;
  for (int sh = 0; sh < 24; sh += 8) {
    uint32_t v = ((a >> sh) & 255) + ((b >> sh) & 255);
    r |= (v > 255 ? 255 : v) << sh;
  }
  return r;
#endif
}
static inline uint32_t hexC32(uint32_t rgb) { return rgb & 0xFFFFFFu; }
// Brightness multiplier that may exceed 1: gain in 1/256 units.
static inline uint32_t c32gain(uint32_t c, uint32_t gain) {
  return gain <= 256 ? c32scale(c, gain) : c32add(c, c32scale(c, gain - 256));
}
// Dithered, byte-swapped RGB565 for the framebuffer. 'dith' holds the
// per-channel dither offsets packed like a colour.
static inline uint16_t c32out(uint32_t c, uint32_t dith) {
  c = c32add(c, dith);
  uint16_t v = (uint16_t)(((c >> 8) & 0xF800u) | ((c >> 5) & 0x07E0u) | ((c >> 3) & 0x001Fu));
  return swap16(v);
}
// Packed dither words for the 4x4 Bayer matrix (R/B: 0..7, G: 0..3).
static inline uint32_t ditherWord(int i) {
  uint32_t d = kBayer4[i] >> 1;
  return (d << 16) | ((d >> 1) << 8) | d;
}
