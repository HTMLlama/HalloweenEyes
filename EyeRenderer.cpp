// EyeRenderer.cpp -- see EyeRenderer.h for the model.
//
// Coordinates: X right, Y up, in pixels from the eye centre. The eye's gaze
// direction g and its local right/up axes u, v come from yaw/pitch. The iris
// is a flat disc at depth Rs*cos(A) along g, so its texture coordinates (s,t)
// are an affine function of panel position -- cheap to step per pixel and
// naturally foreshortened as the eye turns. Sclera pixels are traced onto the
// sphere to get eyeball-local coordinates for the vein texture and lighting.
//
// Speed (Cortex-M4): the eyeball is shaded once for both eyes. Each column is
// split into runs -- pure sclera, pure iris and the thin limbus band -- by
// solving where the iris circle crosses it, so the hot loops carry no
// per-pixel branching. Colour maths is packed 0x00RRGGBB integer work, and
// exp/div come from per-frame tables. Each eye then only adds its lids.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif

#include "EyeRenderer.h"

#if defined(EYE_PROFILE) && defined(__arm__)
#include <Arduino.h>
uint32_t gEyeProf[6];
#define PROF_MARK(i) gEyeProf[i] = DWT->CYCCNT
#else
#define PROF_MARK(i) ((void)0)
#endif

namespace {

struct LidGeom {
  float halfW;             // half the width of the opening
  float hu, hl;            // upper / lower lid height above / below the corners
  float cornerN, cornerT;  // nasal / temporal corner heights
  float eU, eL;            // arch exponents (lower = rounder)
  float skewU, skewL;      // shift the lid peak toward the nose / temple
  float trackU, trackL;    // how much the lids follow vertical gaze
  float shutterU, shutterL;  // flat shutter rest positions (LIDS_SHUTTER)
  float aoTop, aoBot;        // lid shadow strength
};

const LidGeom kLidGeom[3] = {
    // ALMOND (human)
    {61.0f, 37.0f, 33.0f, -5.0f, 1.0f, 0.58f, 0.80f, 0.14f, 0.12f, 0.80f,
     0.45f, 0.0f, 0.0f, 0.55f, 0.30f},
    // ROUND (creature)
    {62.0f, 51.0f, 47.0f, -3.0f, 3.0f, 0.52f, 0.58f, 0.05f, 0.05f, 0.55f,
     0.30f, 0.0f, 0.0f, 0.50f, 0.28f},
    // SHUTTER (machine): round housing, flat shutters
    {58.0f, 58.0f, 58.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.35f, 0.20f,
     47.0f, 53.0f, 0.35f, 0.25f},
};

// Light direction for the eyeball (up-left, toward the viewer).
const float kLx = -0.36f, kLy = 0.52f, kLz = 0.775f;

inline float fracf(float x) { return fastFrac(x); }

inline void fillCol(uint16_t* col, int y0, int y1, uint16_t v) {
  for (int y = y0; y <= y1; y++) col[y * EYE_W] = v;
}

inline uint32_t weight256(float f) {
  return f <= 0.0f ? 0u : (f >= 1.0f ? 256u : (uint32_t)(f * 256.0f));
}

// RGB565 -> packed 0x00RRGGBB via two byte-indexed tables. Indexing them with
// the bytes of a *byte-swapped* pixel converts framebuffer pixels directly.
uint32_t gHi[256], gLo[256];
bool gLutReady = false;

void initLut() {
  for (int h = 0; h < 256; h++) {
    uint32_t r5 = h >> 3, g3 = h & 7;
    gHi[h] = (((r5 << 3) | (r5 >> 2)) << 16) | (((g3 << 5) | (g3 >> 1)) << 8);
  }
  for (int l = 0; l < 256; l++) {
    uint32_t g3 = l >> 5, b5 = l & 31;
    gLo[l] = ((g3 << 2) << 8) | ((b5 << 3) | (b5 >> 2));
  }
  gLutReady = true;
}
inline uint32_t c32from565(uint16_t v) { return gHi[v >> 8] | gLo[v & 255]; }
inline uint32_t c32fromBE(uint16_t v) { return gHi[v & 255] | gLo[v >> 8]; }

// Parameters for the machine-eye overlays, which run out of line.
struct FancyCtx {
  float pupil, Ri;
  uint16_t ovl;
  uint32_t ovl32;
  float rot1, rot2, sweepPos, pulseR, pulseAmp;
  float swX, swY, swX2, swY2;  // sweep line and the trailing edge of its glow
};

// Rotating segments, radar sweep, reticle and pulse rings (additive).
__attribute__((noinline)) uint32_t applyOverlays(uint32_t c, float s, float t,
                                                 float r2, const FancyCtx& f) {
  const float r = fsqrt(r2);
  float amt = 0.0f;
  float a = -1.0f;  // angle in turns, computed on demand
  if ((f.ovl & OVL_SEG_RING) && r > 0.58f && r < 0.7f) {
    a = fastAtan2(t, s) * (1.0f / EYE_TAU);
    float seg = fracf((a + f.rot1) * 12.0f);
    float on = smoothstep(0.0f, 0.06f, seg) * smoothstep(0.64f, 0.58f, seg);
    float band = smoothstep(0.58f, 0.6f, r) * smoothstep(0.7f, 0.68f, r);
    amt += 0.85f * on * band;
  }
  if ((f.ovl & OVL_SEG_RING2) && r > 0.85f && r < 0.93f) {
    if (a < 0.0f) a = fastAtan2(t, s) * (1.0f / EYE_TAU);
    float seg = fracf((a + f.rot2) * 40.0f);
    float on = smoothstep(0.0f, 0.1f, seg) * smoothstep(0.5f, 0.4f, seg);
    float band = smoothstep(0.85f, 0.87f, r) * smoothstep(0.93f, 0.91f, r);
    amt += 0.7f * on * band;
  }
  if ((f.ovl & OVL_SWEEP) && r > f.pupil && r < 0.96f &&
      f.swX2 * t - f.swY2 * s >= 0.0f && s * f.swY - t * f.swX >= 0.0f) {
    if (a < 0.0f) a = fastAtan2(t, s) * (1.0f / EYE_TAU);
    float behind = fracf(f.sweepPos - a);
    float e = 1.0f - behind * 4.5f;  // ~exp(-9x) over the trailing wedge
    if (e > 0.0f) amt += 0.55f * e * e * smoothstep(0.96f, 0.9f, r);
  }
  if (f.ovl & OVL_RETICLE) {
    float dAxis = minf(fabsf(s), fabsf(t)) * f.Ri;
    if (r > 0.24f && r < 0.5f) amt += 0.9f * saturate(1.0f - dAxis * 0.9f);
    float arc = fabsf(r - 0.8f) * f.Ri;
    if (arc < 1.3f) {
      if (a < 0.0f) a = fastAtan2(t, s) * (1.0f / EYE_TAU);
      float da = fabsf(fracf(a * 4.0f) - 0.5f);
      if (da < 0.13f) amt += 0.9f * saturate(1.2f - arc) * smoothstep(0.13f, 0.1f, da);
    }
  }
  if (f.ovl & OVL_PULSE) {
    float dr = (r - f.pulseR) * f.Ri * (1.0f / 1.6f);
    if (dr > -2.5f && dr < 2.5f) {
      float e = 1.0f - dr * dr * 0.16f;  // gaussian-ish bump
      amt += 0.8f * f.pulseAmp * e * e;
    }
  }
  if (amt <= 0.0f) return c;
  return c32add(c, c32scale(f.ovl32, weight256(amt)));
}

// Everything the shading code needs for one frame.
struct Frame {
  const uint8_t* sclTex;
  const uint16_t* tex;
  int N;
  int texMax256;  // (N - 1) texels in 1/256 units
  float cyc;      // eye centre y minus half a pixel: Y = cyc - row
  float Ri;
  float sY, tY, fY, gY;  // per-row steps of s, t and texture x/y
  float texOff;
  float limIn2, limOut2;
  // Pupil
  uint8_t shape;
  bool dilate, shineOn;
  float pupil, pAA, pLim2, pupIn2, kLutScale;
  uint32_t pup32, acc32;
  // Sclera
  uint32_t scl32, sdet32;
  float shBase, shK, uzR, vyR, vzR;
  bool glowOn;
  // Effects
  uint32_t lid32;
  bool pulseOn, rowFx, ovlOn;
  uint32_t pulseG;
  int ovlBands;
  float ovlBand[5][2];  // r^2 ranges where overlays can draw
  FancyCtx fc;
  // Tables
  float kLut[256];
  uint32_t glowTab[64];
  uint16_t rowGain[EYE_H];
  uint32_t dith[16];
};

// Per-column values.
struct Column {
  float K;               // Rs^2 - X^2
  float sC, tC, fC, gC;  // s, t, tex x, tex y at Y = 0
  float laCol, lbCol, ndlCol;
  const uint32_t* dcol;
};

// Iris texel at fixed-point texture position (1/256 texel units).
inline uint32_t texelAt(const Frame& F, int fxi, int fyi) {
  fxi = clampi(fxi, 0, F.texMax256);
  fyi = clampi(fyi, 0, F.texMax256);
  return c32from565(F.tex[(fyi >> 8) * F.N + (fxi >> 8)]);
}

// Iris texture sample; fxb/fyb are texel offsets from the centre.
inline uint32_t irisTexel(const Frame& F, float fxb, float fyb, float r2) {
  const float k = F.dilate ? F.kLut[clampi((int)(r2 * F.kLutScale), 0, 255)] : 1.0f;
  return texelAt(F, (int)((fxb * k + F.texOff) * 256.0f), (int)((F.texOff - fyb * k) * 256.0f));
}

// Draws an organic pupil (any shape) over iris colour c.
inline uint32_t pupilShade(const Frame& F, uint32_t c, float s, float t, float r2) {
  float d, shine = 0.0f;
  const float as = fabsf(s), at = fabsf(t);
  const float pupil = F.pupil;
  switch (F.shape) {
    case PUPIL_SLIT: {
      const float h = 0.94f;
      if (as > pupil + F.pAA) return c;
      float q = 1.0f - (t * t) * (1.0f / (h * h));
      d = as - pupil * (q > 0.0f ? q : 0.0f);
      if (at > h) d = maxf(d, at - h);
      shine = saturate(1.0f - at * (1.0f / h)) * saturate(1.0f - as / (pupil + 0.02f));
      break;
    }
    case PUPIL_GOAT: {
      float hh = pupil * 0.55f, wl = minf(0.92f, pupil * 2.4f);
      if (at > hh + F.pAA) return c;
      float qx = maxf(as - (wl - hh), 0.0f);
      d = fsqrt(qx * qx + t * t) - hh;
      shine = saturate(-d / (hh + 1e-3f));
      break;
    }
    case PUPIL_SQUARE: {
      if (maxf(as, at) > pupil + F.pAA) return c;
      float rc = pupil * 0.35f, core = pupil - rc;
      float qx = maxf(as - core, 0.0f), qy = maxf(at - core, 0.0f);
      d = (qx > 0.0f || qy > 0.0f) ? fsqrt(qx * qx + qy * qy) - rc : maxf(as, at) - pupil;
      shine = saturate(-d / pupil);
      break;
    }
    case PUPIL_CUTTLE: {
      if (at > 0.62f || as > 0.84f) return c;
      float th = pupil * 0.62f;
      float xn = s * (1.0f / 0.8f);
      float topE = 0.02f + 0.22f * xn * xn;
      float dl = (as - 0.4f) * (1.0f / 0.167f);
      float lobe = dl * dl < 4.0f ? (1.0f - dl * dl * 0.25f) : 0.0f;
      float botE = topE - th - 0.3f * lobe * lobe;
      d = maxf(as - 0.8f, maxf(t - topE, botE - t));
      shine = saturate(-d / th);
      break;
    }
    case PUPIL_ROUND: {
      if (r2 > F.pLim2) return c;
      float r = fsqrt(r2);
      d = r - pupil;
      shine = saturate(1.0f - r / pupil);
      break;
    }
    default:
      return c;  // none, or baked into the texture
  }
  float cov = 0.5f - d * F.Ri;
  if (cov <= 0.0f) return c;
  uint32_t pc = F.pup32;
  if (F.shineOn) pc = c32lerp(F.pup32, F.acc32, weight256(shine * shine * 0.5f));  // eyeshine
  return c32lerp(c, pc, weight256(cov));
}

// Pulse, pupil and overlays on top of an iris texel.
inline uint32_t irisShade(const Frame& F, uint32_t c, float s, float t, float r2) {
  if (F.pulseOn) c = c32gain(c, F.pulseG);
  c = pupilShade(F, c, s, t, r2);
  if (F.ovlOn) {
    for (int i = 0; i < F.ovlBands; i++) {
      if (r2 >= F.ovlBand[i][0] && r2 <= F.ovlBand[i][1]) {
        c = applyOverlays(c, s, t, r2, F.fc);
        break;
      }
    }
  }
  return c;
}

// Iris colour at iris-plane position (s,t).
inline uint32_t irisPixel(const Frame& F, float s, float t, float r2, float fxb, float fyb) {
  if (F.shape == PUPIL_ROUND && r2 < F.pupIn2) {
    // Fully inside a round pupil: no texture needed.
    if (!F.shineOn) return F.pup32;
    float shine = 1.0f - fsqrt(r2) / F.pupil;
    return c32lerp(F.pup32, F.acc32, weight256(shine * shine * 0.5f));
  }
  return irisShade(F, irisTexel(F, fxb, fyb, r2), s, t, r2);
}

// Sclera colour at row offset Y (lighting, veins, bloom).
inline uint32_t scleraPixel(const Frame& F, const Column& C, float Y, float r2) {
  const float zz = C.K - Y * Y;
  if (zz < 1.0f) return F.lid32;  // off the side of the eyeball
  const float Zs = fsqrt(zz);
  const float la = C.laCol + Zs * F.uzR;
  const float lb = C.lbCol + Y * F.vyR + Zs * F.vzR;
  const int ix = clampi((int)(la * 64.0f + 64.0f), 0, 127);
  const int iy = clampi((int)(64.0f - lb * 64.0f), 0, 127);
  const uint32_t det = F.sclTex[(iy << 7) + ix];
  uint32_t c = det ? c32lerp(F.scl32, F.sdet32, det + (det >> 7)) : F.scl32;
  const float ndl = C.ndlCol + Y * kLy + Zs * kLz;
  c = c32scale(c, (uint32_t)(F.shBase + F.shK * maxf(ndl, 0.0f)));
  if (F.glowOn) {
    int gi = (int)((r2 - 1.0f) * 8.0f);
    if (gi < 64) c = c32add(c, F.glowTab[gi < 0 ? 0 : gi]);
  }
  return c;
}

inline uint16_t finish(const Frame& F, const Column& C, uint32_t c, int y) {
  if (F.rowFx) c = c32gain(c, F.rowGain[y]);
  return c32out(c, C.dcol[(y & 3) << 2]);
}

// The thin band where iris meets sclera: blend the two. Out of line (only a
// few hundred pixels a frame).
__attribute__((noinline)) uint16_t limbusPixel(const Frame& F, const Column& C, int y) {
  const float Y = F.cyc - y;
  const float s = C.sC + F.sY * Y, t = C.tC + F.tY * Y;
  const float r2 = s * s + t * t;
  uint32_t c;
  if (r2 >= F.limOut2) {
    c = scleraPixel(F, C, Y, r2);
  } else {
    c = irisPixel(F, s, t, r2, C.fC + F.fY * Y, C.gC + F.gY * Y);
    if (r2 > F.limIn2) {
      const uint32_t sc = scleraPixel(F, C, Y, r2);
      c = c32lerp(sc, c, weight256((1.0f - fsqrt(r2)) * F.Ri * 0.55f + 0.5f));
    }
  }
  return finish(F, C, c, y);
}

// Rows y (integers) where r2(y) = qa*y^2 + qb*y + qc < R2, clipped to [lo,hi].
inline void rowsInside(float qa, float qb, float qc, float R2, int lo, int hi,
                       int& a, int& b) {
  const float disc = qb * qb - 4.0f * qa * (qc - R2);
  if (disc <= 0.0f) {
    a = hi + 1;
    b = hi;
    return;
  }
  const float sq = fsqrt(disc), inv = 0.5f / qa;
  a = maxi((int)fastCeil((-qb - sq) * inv), lo);
  b = mini((int)fastFloor((-qb + sq) * inv), hi);
}

}  // namespace

// ---------------------------------------------------------------------------

EyeRenderer::EyeRenderer() : style_(nullptr), assets_(nullptr), rng_(0xC0FFEEu) {
  for (int e = 0; e < 2; e++) {
    nasalLeft_[e] = (e == 0);
    offX_[e] = offY_[e] = 0;
    nictitate_[e] = 0.0f;
  }
  if (!gLutReady) initLut();
}

void EyeRenderer::setStyle(const EyeStyle* style, const EyeAssets* assets) {
  style_ = style;
  assets_ = assets;
  buildLidTables(0);
  buildLidTables(1);
  // Lid shadow. Glowing eyes are lit from within, so they get less of it.
  const LidGeom& LG = kLidGeom[style->lids];
  const float k = 1.0f - 0.7f * (style->emissive / 255.0f);
  // Tables are indexed in half pixels; beyond the last entry that darkens by
  // more than ~2% the shadow is dropped so those rows are plain copies.
  aoTopRows_ = aoBotRows_ = 0;
  for (int i = 0; i < 64; i++) {
    aoT_[i] = (uint16_t)weight256(1.0f - k * LG.aoTop * expf(-(i * 0.5f) / 4.5f));
    if (aoT_[i] < 251) aoTopRows_ = (uint8_t)(i + 1);
    else aoT_[i] = 256;
  }
  for (int i = 0; i < 32; i++) {
    aoB_[i] = (uint16_t)weight256(1.0f - k * LG.aoBot * expf(-(i * 0.5f) / 2.5f));
    if (aoB_[i] < 251) aoBotRows_ = (uint8_t)(i + 1);
    else aoB_[i] = 256;
  }
}

void EyeRenderer::setEye(int eye, bool nasalOnLeft, int offsetX, int offsetY) {
  nasalLeft_[eye] = nasalOnLeft;
  offX_[eye] = (int8_t)offsetX;
  offY_[eye] = (int8_t)offsetY;
  if (style_) buildLidTables(eye);
}

void EyeRenderer::buildLidTables(int eye) {
  const LidGeom& g = kLidGeom[style_->lids];
  const float cx = 64.0f + offX_[eye];
  for (int x = 0; x < EYE_W; x++) {
    float u = (x + 0.5f - cx) / g.halfW;
    float un = nasalLeft_[eye] ? u : -u;  // -1 at the nasal corner
    un_[eye][x] = un;
    if (fabsf(u) >= 1.0f) {
      inside_[eye][x] = 0;
      archU_[eye][x] = archL_[eye][x] = corner_[eye][x] = 0.0f;
      nasalW_[eye][x] = nasalLin_[eye][x] = 0.0f;
      continue;
    }
    inside_[eye][x] = 1;
    float w = 1.0f - u * u;
    archU_[eye][x] = powf(w, g.eU) * (1.0f - g.skewU * un);
    archL_[eye][x] = powf(w, g.eL) * (1.0f + g.skewL * un);
    corner_[eye][x] = lerpf(g.cornerN, g.cornerT, (un + 1.0f) * 0.5f);
    nasalLin_[eye][x] = (1.0f - un) * 0.5f;
    nasalW_[eye][x] = powf(nasalLin_[eye][x], 1.5f);
  }
}

void EyeRenderer::computeLids(int eye, const EyePose& P, float gazeYpx, Lids& out) {
  const LidGeom& g = kLidGeom[style_->lids];
  const float cy = 64.0f + offY_[eye];
  const bool shutter = style_->lids == LIDS_SHUTTER;
  const LidPose& m = P.mood;
  const float blink = clampf(P.blink, 0.0f, 1.0f);
  const float upMood = 0.55f * m.droop + 0.30f * m.squint - 0.22f * m.widen;
  const float loMood = 0.34f * m.squint + 0.10f * m.anger - 0.12f * m.widen;

  for (int x = 0; x < EYE_W; x++) {
    if (!inside_[eye][x]) {
      out.top[x] = out.bot[x] = cy;  // closed column
      continue;
    }
    const float aU = archU_[eye][x], aL = archL_[eye][x], c = corner_[eye][x];
    float up = c + g.hu * aU;
    float lo = c - g.hl * aL;
    // Lids ride along with vertical gaze; corners stay anchored.
    up += g.trackU * gazeYpx * aU;
    lo += g.trackL * gazeYpx * aL;
    // Expression.
    up -= g.hu * aU * (upMood + 0.62f * m.anger * nasalW_[eye][x]);
    lo += g.hl * aL * loMood;
    // Anatomical limits.
    up = minf(up, c + g.hu * 1.25f * aU);
    lo = maxf(lo, c - g.hl * 1.2f * aL);
    if (shutter) {
      float lvlU = g.shutterU + g.trackU * gazeYpx -
                   g.hu * (0.5f * m.droop + 0.3f * m.squint - 0.3f * m.widen) -
                   g.hu * 0.5f * m.anger * (nasalLin_[eye][x] - 0.3f);
      float lvlL = -g.shutterL + g.trackL * gazeYpx + g.hl * 0.3f * m.squint;
      up = minf(up, lvlU);
      lo = maxf(lo, lvlL);
    }
    if (blink > 0.0f) {
      float meet = shutter ? 0.5f * (up + lo) : lo + 0.22f * (up - lo);
      up = lerpf(up, meet, blink);
      lo = lerpf(lo, meet, blink);
    }
    if (up < lo) up = lo = 0.5f * (up + lo);
    out.top[x] = cy - up;
    out.bot[x] = cy - lo;
  }
  // Anti-aliasing widths: the vertical extent of a 1px band across the edge.
  for (int x = 0; x < EYE_W; x++) {
    int xl = x > 0 ? x - 1 : x, xr = x < EYE_W - 1 ? x + 1 : x;
    float st = (out.top[xr] - out.top[xl]) / (float)(xr - xl);
    float sb = (out.bot[xr] - out.bot[xl]) / (float)(xr - xl);
    out.wTop[x] = minf(fsqrt(1.0f + st * st), 4.0f);
    out.wBot[x] = minf(fsqrt(1.0f + sb * sb), 4.0f);
  }
}

void EyeRenderer::renderShared(const EyePose& P, const EyePose& P1,
                               uint32_t timeMs, uint16_t* sh) {
  PROF_MARK(0);
  const EyeStyle& S = *style_;
  const EyeAssets& A = *assets_;
  const float tsec = timeMs * 0.001f;
  const float cx = 64.0f, cy = 64.0f;  // eyes are offset when composed
  const float Rs = S.eyeRadius, Ri = S.irisRadius;
  const float invRs = 1.0f / Rs, invRi = 1.0f / Ri;
  const float sinA = Ri * invRs, cosA = fsqrt(1.0f - sinA * sinA);

  // --- Gaze basis ------------------------------------------------------------
  const float cyw = cosf(P.yaw), syw = sinf(P.yaw);
  const float cpt = cosf(P.pitch), spt = sinf(P.pitch);
  const float gx = syw * cpt, gy = spt, gz = cyw * cpt;
  const float ux = cyw, uz = -syw;  // uy = 0
  const float vx = -syw * spt, vy = cpt, vz = -cyw * spt;

  // Iris-plane coordinates: s = s0 + sX*X + sY*Y, t = t0 + tX*X + tY*Y.
  const float D = Rs * cosA, ig = 1.0f / gz;
  const float sX = (ux - gx * uz * ig) * invRi;
  const float sY = (-gy * uz * ig) * invRi;
  const float s0 = D * uz * ig * invRi;
  const float tX = (vx - gx * vz * ig) * invRi;
  const float tY = (vy - gy * vz * ig) * invRi;
  const float t0 = D * vz * ig * invRi;
  const float irisCx = cx + D * gx, irisCy = cy - D * gy;

  // --- Lids for both eyes (composeEye uses them too) ------------------------------
  computeLids(0, P, D * gy, lids_[0]);
  computeLids(1, P1, D * gy, lids_[1]);
  nictitate_[0] = P.nictitate;
  nictitate_[1] = P1.nictitate;
  const float rimW = S.lidEdge ? 2.2f : 0.0f;
  PROF_MARK(1);

  // --- Per-frame setup -----------------------------------------------------------
  Frame F;
  F.sclTex = A.sclera;
  F.tex = A.iris;
  F.N = A.irisN;
  F.texMax256 = (A.irisN - 1) * 256 + 255;
  F.cyc = cy - 0.5f;
  F.Ri = Ri;

  const float pulseGain =
      1.0f + (S.pulse / 255.0f) * 0.45f * sinf(EYE_TAU * S.pulseHz * tsec);
  F.pulseG = (uint32_t)(pulseGain * 256.0f + 0.5f);
  F.pulseOn = S.emissive > 0 && F.pulseG != 256;

  // Pupil. Machine cores and apertures are baked into the texture; the
  // dilation remap resizes them.
  const float pupil = clampf(P.pupil, 0.02f, 0.95f);
  const bool baked = styleBakesPupil(S);
  F.shape = baked ? (uint8_t)PUPIL_NONE : (uint8_t)S.pupil;
  F.dilate = S.pupil == PUPIL_ROUND || baked;
  F.pupil = pupil;
  F.pAA = 2.0f * invRi;
  F.pLim2 = (pupil + F.pAA) * (pupil + F.pAA);
  const float pin = maxf(pupil - 0.5f * invRi, 0.0f);
  F.pupIn2 = pin * pin;
  F.pup32 = hexC32(S.pupilColor);
  F.acc32 = hexC32(S.pupilAccent);
  F.shineOn = (S.fx & FX_TAPETUM) != 0;

  // Dilation: map screen radius to texture radius so the iris fibres stretch
  // and compress with the pupil instead of the pupil just covering them.
  F.kLutScale = 256.0f / 1.2f;
  if (F.dilate) {
    const float p0 = S.pupilSize;
    for (int i = 0; i < 256; i++) {
      float r = fsqrt((i + 0.5f) / F.kLutScale);
      float rt = r <= pupil ? p0 * r / pupil
                            : p0 + (r - pupil) * (1.0f - p0) / (1.0f - pupil);
      F.kLut[i] = rt / r;
    }
  }

  // Bloom around the iris, as ready-to-add colours by r^2.
  F.glowOn = S.glow > 0;
  if (F.glowOn) {
    const uint32_t g32 = hexC32(S.glowColor);
    const float gs = S.glow / 255.0f * 1.1f * pulseGain;
    // Machine housings get a tight halo; organic eyes a broad bloom.
    const float fall = S.lids == LIDS_SHUTTER ? 11.0f : 3.0f;
    for (int i = 0; i < 64; i++) {
      float r = fsqrt(1.0f + i * 0.125f);
      F.glowTab[i] = c32scale(g32, weight256(expf(-(r - 1.0f) * fall) * gs));
    }
  }

  // Row gains for CRT-style effects.
  F.rowFx = (S.fx & (FX_SCANLINES | FX_ROLLBAR | FX_FLICKER)) != 0;
  if (F.rowFx) {
    float flick = (S.fx & FX_FLICKER) ? (0.9f + 0.1f * rng_.uniform()) : 1.0f;
    float barY = fracf(tsec * 0.23f) * 190.0f - 30.0f;
    for (int y = 0; y < EYE_H; y++) {
      float gn = flick;
      if ((S.fx & FX_SCANLINES) && (y & 1)) gn *= 0.62f;
      if (S.fx & FX_ROLLBAR) {
        float d = (y - barY) / 9.0f;
        gn *= 1.0f + 0.35f * expf(-d * d);
      }
      F.rowGain[y] = (uint16_t)(gn * 256.0f);
    }
  }

  // Colours and lighting.
  F.scl32 = hexC32(S.sclera);
  F.sdet32 = hexC32(S.scleraDetail);
  F.lid32 = hexC32(S.lidColor);
  const float shadeAmt = S.shade / 255.0f;
  F.shBase = (1.0f - shadeAmt) * 256.0f;
  F.shK = shadeAmt * 256.0f * invRs;  // n.L is computed unscaled
  F.uzR = uz * invRs;
  F.vyR = vy * invRs;
  F.vzR = vz * invRs;
  const uint16_t lidPix = c32out(F.lid32, 0);

  // Texture coordinates (with spin) are affine too; pre-scale to texels.
  const float halfN = A.irisN * 0.5f;
  F.texOff = halfN - 0.5f;
  float spinA = S.spin * tsec * EYE_TAU;
  if (S.pupil == PUPIL_APERTURE)  // blades twist as the aperture opens/closes
    spinA += 1.3f * (S.pupilSize - pupil) / (S.pupilMax - S.pupilMin + 1e-3f);
  const float ca = cosf(spinA) * halfN, sa = sinf(spinA) * halfN;
  const float fX = sX * ca - tX * sa, f0 = s0 * ca - t0 * sa;
  const float gXt = sX * sa + tX * ca, g0 = s0 * sa + t0 * ca;
  F.fY = sY * ca - tY * sa;
  F.gY = sY * sa + tY * ca;
  F.sY = sY;
  F.tY = tY;

  // Limbus anti-aliasing band (about one pixel wide).
  const float aaI = 0.9f * invRi;
  F.limIn2 = (1.0f - aaI) * (1.0f - aaI);
  F.limOut2 = (1.0f + aaI) * (1.0f + aaI);

  // Machine overlays (out of line).
  FancyCtx& fc = F.fc;
  fc.pupil = pupil;
  fc.Ri = Ri;
  fc.ovl = S.overlays;
  fc.ovl32 = hexC32(S.overlayColor);
  fc.rot1 = fracf(tsec * 0.15f);
  fc.rot2 = fracf(-tsec * 0.08f);
  fc.sweepPos = fracf(tsec * 0.45f);
  const float pulsePhase = fracf(tsec * 0.55f);
  fc.pulseR = pupil + pulsePhase * (1.0f - pupil);
  fc.pulseAmp = (1.0f - pulsePhase) * fsqrt(1.0f - pulsePhase);
  const float swA = fc.sweepPos * EYE_TAU, swA2 = swA - 0.222f * EYE_TAU;
  fc.swX = cosf(swA);
  fc.swY = sinf(swA);
  fc.swX2 = cosf(swA2);
  fc.swY2 = sinf(swA2);
  F.ovlOn = S.overlays != 0;
  // Radius bands where each overlay can draw, so other pixels skip the maths.
  F.ovlBands = 0;
  auto band = [&](float r0, float r1) {
    r0 = maxf(r0, 0.0f);
    F.ovlBand[F.ovlBands][0] = r0 * r0;
    F.ovlBand[F.ovlBands][1] = r1 * r1;
    F.ovlBands++;
  };
  if (S.overlays & OVL_SEG_RING) band(0.58f, 0.7f);
  if (S.overlays & OVL_SEG_RING2) band(0.85f, 0.93f);
  if (S.overlays & OVL_SWEEP) band(pupil, 0.96f);
  if (S.overlays & OVL_RETICLE) {
    band(0.24f, 0.5f);
    band(0.8f - 1.3f * invRi, 0.8f + 1.3f * invRi);
  }
  if ((S.overlays & OVL_PULSE) && F.ovlBands < 5)
    band(fc.pulseR - 4.0f * invRi, fc.pulseR + 4.0f * invRi);

  for (int i = 0; i < 16; i++) F.dith[i] = ditherWord(i);
  PROF_MARK(2);

  // --- Shade every row either eye can see, column by column --------------------
  for (int x = 0; x < EYE_W; x++) {
    uint16_t* col = sh + x;
    int u0 = EYE_H, u1 = -1;
    for (int e = 0; e < 2; e++) {
      const int xe = x + offX_[e];
      if (xe < 0 || xe >= EYE_W) continue;
      const Lids& L = lids_[e];
      if (!(L.bot[xe] > L.top[xe] + 0.02f)) continue;
      u0 = mini(u0, (int)fastFloor(L.top[xe] - 0.5f * L.wTop[xe] - rimW) - offY_[e]);
      u1 = maxi(u1, (int)fastCeil(L.bot[xe] + 0.5f * L.wBot[xe] + rimW) - offY_[e]);
    }
    u0 = maxi(u0, 0);
    u1 = mini(u1, EYE_H - 1);
    if (u0 > u1) {
      fillCol(col, 0, EYE_H - 1, lidPix);
      continue;
    }
    if (u0 > 0) fillCol(col, 0, u0 - 1, lidPix);
    if (u1 < EYE_H - 1) fillCol(col, u1 + 1, EYE_H - 1, lidPix);

    Column C;
    const float X = x + 0.5f - cx;
    C.K = Rs * Rs - X * X;
    C.sC = s0 + sX * X;
    C.tC = t0 + tX * X;
    C.fC = f0 + fX * X;
    C.gC = g0 + gXt * X;
    C.laCol = X * ux * invRs;
    C.lbCol = X * vx * invRs;
    C.ndlCol = X * kLx;
    C.dcol = &F.dith[x & 3];

    // Split the column at the iris circle: r2(y) is a quadratic in the row.
    const float sA = C.sC + sY * F.cyc, tA = C.tC + tY * F.cyc;  // at row 0
    const float qa = sY * sY + tY * tY;
    const float qb = -2.0f * (sA * sY + tA * tY);
    const float qc = sA * sA + tA * tA;
    int oA, oB, pA, pB;
    rowsInside(qa, qb, qc, F.limOut2 * 1.01f, u0, u1, oA, oB);  // iris or limbus
    rowsInside(qa, qb, qc, F.limIn2 * 0.99f, u0, u1, pA, pB);   // pure iris
    if (oA > oB) {
      oA = u1 + 1;
      oB = u1;
    }
    if (pA > pB) {
      pA = oB + 1;
      pB = oB;
    }

    // Pure sclera above and below the iris.
    for (int pass = 0; pass < 2; pass++) {
      const int ya = pass ? oB + 1 : u0, yb = pass ? u1 : oA - 1;
      float Y = F.cyc - ya;
      float s = C.sC + sY * Y, t = C.tC + tY * Y;
      for (int y = ya; y <= yb; y++, Y -= 1.0f, s -= sY, t -= tY)
        col[y * EYE_W] = finish(F, C, scleraPixel(F, C, Y, F.glowOn ? s * s + t * t : 2.0f), y);
    }
    // Limbus band.
    for (int y = oA; y < pA; y++) col[y * EYE_W] = limbusPixel(F, C, y);
    for (int y = pB + 1; y <= oB; y++) col[y * EYE_W] = limbusPixel(F, C, y);
    // Pure iris.
    float Y = F.cyc - pA;
    float s = C.sC + sY * Y, t = C.tC + tY * Y;
    if (!F.dilate) {
      // Texture position is affine here: step it in 16.16 fixed point.
      int32_t fx = (int32_t)((C.fC + F.fY * Y + F.texOff) * 65536.0f);
      int32_t fy = (int32_t)((F.texOff - (C.gC + F.gY * Y)) * 65536.0f);
      const int32_t dfx = (int32_t)(-F.fY * 65536.0f), dfy = (int32_t)(F.gY * 65536.0f);
      for (int y = pA; y <= pB; y++, s -= sY, t -= tY, fx += dfx, fy += dfy)
        col[y * EYE_W] = finish(F, C, irisShade(F, texelAt(F, fx >> 8, fy >> 8), s, t, s * s + t * t), y);
    } else {
      float fxb = C.fC + F.fY * Y, fyb = C.gC + F.gY * Y;
      for (int y = pA; y <= pB; y++, s -= sY, t -= tY, fxb -= F.fY, fyb -= F.gY)
        col[y * EYE_W] = finish(F, C, irisPixel(F, s, t, s * s + t * t, fxb, fyb), y);
    }
  }
  PROF_MARK(3);

  // --- Specular highlights (the lids cover them where needed) --------------------
  if (S.highlight) {
    const float hs = S.highlight / 255.0f;
    // The cornea bulges forward, so reflections travel about half as far as
    // the iris does.
    const float ccx = cx + 0.55f * (irisCx - cx);
    const float ccy = cy + 0.55f * (irisCy - cy);
    drawHighlight(sh, ccx - 0.30f * Ri, ccy - 0.34f * Ri, 0.19f * Ri, 0.14f * Ri,
                  0.9f * hs, s0, sX, sY, t0, tX, tY, cx, cy);
    drawHighlight(sh, ccx + 0.30f * Ri, ccy + 0.30f * Ri, 0.075f * Ri, 0.065f * Ri,
                  0.5f * hs, s0, sX, sY, t0, tX, tY, cx, cy);
  }
  PROF_MARK(4);
}

void EyeRenderer::composeEye(int eye, uint32_t timeMs, const uint16_t* sh,
                             uint16_t* fb, RowSpan* span) {
  const EyeStyle& S = *style_;
  const Lids& L = lids_[eye];
  const LidGeom& LG = kLidGeom[S.lids];
  const uint32_t lid32 = hexC32(S.lidColor), edge32 = hexC32(S.lidEdge);
  const uint16_t lidPix = c32out(lid32, 0);
  const bool hasEdge = S.lidEdge != 0;
  const float rimW = hasEdge ? 2.2f : 0.0f;
  const int ox = offX_[eye], oy = offY_[eye];
  const uint32_t mem32 = hexC32(0xC4D6CC);
  const bool nictOn = (S.fx & FX_NICTITATE) && nictitate_[eye] > 0.001f;
  const float nictEdge = -1.0f + 2.3f * nictitate_[eye];
  uint32_t dith[16];
  for (int i = 0; i < 16; i++) dith[i] = ditherWord(i);
  int16_t rowFirst = EYE_H, rowLast = -1;

  for (int x = 0; x < EYE_W; x++) {
    uint16_t* col = fb + x;
    const float T = L.top[x], Bt = L.bot[x];
    const int xs = x - ox;
    if (!(Bt > T + 0.02f) || xs < 0 || xs >= EYE_W) {
      fillCol(col, 0, EYE_H - 1, lidPix);
      continue;
    }
    const float hwT = 0.5f * L.wTop[x], hwB = 0.5f * L.wBot[x];
    const int y0 = maxi(clampi((int)fastFloor(T - hwT - rimW), 0, EYE_H - 1), oy);
    const int y1 = mini(clampi((int)fastCeil(Bt + hwB + rimW), 0, EYE_H - 1), EYE_H - 1 + oy);
    if (y0 > y1) {
      fillCol(col, 0, EYE_H - 1, lidPix);
      continue;
    }
    if (y0 > 0) fillCol(col, 0, y0 - 1, lidPix);
    if (y1 < EYE_H - 1) fillCol(col, y1 + 1, EYE_H - 1, lidPix);
    rowFirst = (int16_t)mini(rowFirst, y0);
    rowLast = (int16_t)maxi(rowLast, y1);

    const uint16_t* src = sh + xs - oy * EYE_W;  // src[y * EYE_W] = shared(xs, y - oy)
    const uint32_t* dcol = &dith[x & 3];
    const float iwT = 1.0f / L.wTop[x], iwB = 1.0f / L.wBot[x];
    const int fT = (int)fastFloor(1.0f - 2.0f * T), fB = (int)fastFloor(2.0f * Bt - 1.0f);
    // Rows between ia..ib need no lid anti-aliasing; between ca..cb there is
    // no lid shadow either, so the shared pixel is copied as is.
    const int ia = (int)fastCeil(T + hwT - 0.5f), ib = (int)fastFloor(Bt - hwB - 0.5f);
    int ca = maxi(ia, (aoTopRows_ - fT + 1) >> 1);
    int cb = mini(ib, (fB - aoBotRows_) >> 1);

    uint32_t memA = 0, memEdge = 0;
    if (nictOn) {
      float du = (nictEdge - un_[eye][x]) * LG.halfW;
      memA = weight256(0.6f * saturate(du * (1.0f / 3.0f)));
      float e = du - 1.0f;
      memEdge = e * e < 9.0f ? c32scale(mem32, weight256(0.35f * (1.0f - e * e / 9.0f))) : 0;
      if (memA || memEdge) {
        ca = ib + 1;  // every row needs the membrane blend
        cb = ib;
      }
    }

    // Lid shadow + optional membrane for one row (no lid coverage).
    auto shade = [&](int y) -> uint16_t {
      uint32_t c = c32fromBE(src[y * EYE_W]);
      const uint32_t ao = ((uint32_t)aoT_[clampi(2 * y + fT, 0, 63)] * aoB_[clampi(fB - 2 * y, 0, 31)]) >> 8;
      if (ao < 256) c = c32scale(c, ao);
      if (memA || memEdge) c = c32add(c32lerp(c, mem32, memA), memEdge);
      return c32out(c, dcol[(y & 3) << 2]);
    };
    // Rows crossing a lid edge: anti-aliased blend with the lid (and rim).
    auto edge = [&](int y) -> uint16_t {
      const float yc = y + 0.5f;
      const uint32_t covI = weight256(saturate((yc - T) * iwT + 0.5f) * saturate((Bt - yc) * iwB + 0.5f));
      uint32_t lidC = lid32;
      if (hasEdge) {
        // Glowing rim: 1 inside the opening, fading out into each lid.
        float eT = yc < T ? saturate(1.0f - (T - yc) / rimW) : 1.0f;
        float eB = yc > Bt ? saturate(1.0f - (yc - Bt) / rimW) : 1.0f;
        lidC = c32lerp(lid32, edge32, weight256(minf(eT, eB)));
      }
      if (!covI) return c32out(lidC, dcol[(y & 3) << 2]);
      uint32_t c = c32fromBE(src[y * EYE_W]);
      const uint32_t ao = ((uint32_t)aoT_[clampi(2 * y + fT, 0, 63)] * aoB_[clampi(fB - 2 * y, 0, 31)]) >> 8;
      if (ao < 256) c = c32scale(c, ao);
      if (memA || memEdge) c = c32add(c32lerp(c, mem32, memA), memEdge);
      if (covI < 256) c = c32lerp(lidC, c, covI);
      return c32out(c, dcol[(y & 3) << 2]);
    };

    const int a0 = maxi(ia, y0), b0 = mini(ib, y1);
    if (a0 > b0) {
      for (int y = y0; y <= y1; y++) col[y * EYE_W] = edge(y);
      continue;
    }
    for (int y = y0; y < a0; y++) col[y * EYE_W] = edge(y);
    for (int y = b0 + 1; y <= y1; y++) col[y * EYE_W] = edge(y);
    ca = clampi(ca, a0, b0 + 1);
    cb = clampi(cb, ca - 1, b0);
    for (int y = a0; y < ca; y++) col[y * EYE_W] = shade(y);
    for (int y = ca; y <= cb; y++) col[y * EYE_W] = src[y * EYE_W];  // plain copy
    for (int y = cb + 1; y <= b0; y++) col[y * EYE_W] = shade(y);
  }

  RowSpan sp;
  sp.first = rowFirst;
  sp.last = rowLast;
  if (S.fx & FX_GLITCH) glitch(fb, sp, timeMs);
  if (S.fx & FX_PIXELATE) pixelate(fb, sp);
  if (span) *span = sp;
  PROF_MARK(5);
}

void EyeRenderer::drawHighlight(uint16_t* sh, float hx, float hy, float rx,
                                float ry, float strength, float sBase, float sXc,
                                float sYc, float tBase, float tXc, float tYc,
                                float cx, float cy) {
  int x0 = clampi((int)fastFloor(hx - rx - 1.0f), 0, EYE_W - 1);
  int x1 = clampi((int)fastCeil(hx + rx + 1.0f), 0, EYE_W - 1);
  int y0 = clampi((int)fastFloor(hy - ry - 1.0f), 0, EYE_H - 1);
  int y1 = clampi((int)fastCeil(hy + ry + 1.0f), 0, EYE_H - 1);
  const float irx = 1.0f / rx, iry = 1.0f / ry;
  for (int x = x0; x <= x1; x++) {
    const float X = x + 0.5f - cx;
    for (int y = y0; y <= y1; y++) {
      const float yc = y + 0.5f;
      float dx = (x + 0.5f - hx) * irx, dy = (yc - hy) * iry;
      float d2 = dx * dx + dy * dy;
      if (d2 >= 1.0f) continue;
      // Only on the cornea (the bulge over the iris).
      const float Y = cy - yc;
      const float s = sBase + sXc * X + sYc * Y, t = tBase + tXc * X + tYc * Y;
      float mask = saturate((1.3f - (s * s + t * t)) * 4.0f);
      float a = saturate((1.0f - d2) * 1.8f);
      a = a * a * strength * mask;
      if (a <= 0.004f) continue;
      uint16_t* p = &sh[y * EYE_W + x];
      int r, g, b;
      unpack565(swap16(*p), r, g, b);
      r += (int)((255 - r) * a);
      g += (int)((252 - g) * a);
      b += (int)((245 - b) * a);
      *p = swap16(pack565d(r, g, b, 0));
    }
  }
}

void EyeRenderer::glitch(uint16_t* fb, const RowSpan& span, uint32_t timeMs) {
  if (span.first > span.last) return;
  // Glitches come in short bursts every second or two.
  if ((int32_t)(timeMs - glitchNext_) >= 0) {
    glitchUntil_ = timeMs + (uint32_t)rng_.irange(80, 320);
    glitchNext_ = timeMs + (uint32_t)rng_.irange(700, 2600);
  }
  if ((int32_t)(timeMs - glitchUntil_) >= 0 || !rng_.chance(0.7f)) return;
  uint16_t tmp[EYE_W];
  int bands = rng_.irange(1, 3);
  for (int b = 0; b < bands; b++) {
    int h = rng_.irange(2, 9);
    int y0 = rng_.irange(span.first, span.last);
    int shift = rng_.irange(3, 14) * (rng_.chance(0.5f) ? 1 : -1);
    bool swapRB = rng_.chance(0.5f);
    for (int y = y0; y < y0 + h && y < EYE_H; y++) {
      uint16_t* row = &fb[y * EYE_W];
      for (int x = 0; x < EYE_W; x++) tmp[x] = row[imod(x - shift, EYE_W)];
      if (swapRB) {
        for (int x = 0; x < EYE_W; x++) {
          uint16_t c = swap16(tmp[x]);
          c = (uint16_t)(((c & 0x1F) << 11) | (c & 0x07E0) | (c >> 11));
          tmp[x] = swap16(c);
        }
      }
      memcpy(row, tmp, sizeof(tmp));
    }
  }
}

void EyeRenderer::pixelate(uint16_t* fb, RowSpan& span) {
  if (span.first > span.last) return;
  const Col tint = colHex(style_->fxColor);
  const float shades[4] = {0.13f, 0.36f, 0.66f, 1.0f};
  uint16_t pal[4];
  for (int i = 0; i < 4; i++) pal[i] = swap16(col565(mulc(tint, shades[i]), 0));
  int yStart = span.first & ~3, yEnd = span.last | 3;
  if (yEnd > EYE_H - 1) yEnd = EYE_H - 1;
  for (int by = yStart; by <= yEnd; by += 4) {
    for (int bx = 0; bx < EYE_W; bx += 4) {
      int r, g, b;
      unpack565(swap16(fb[(by + 1) * EYE_W + bx + 1]), r, g, b);
      int lum = (r * 77 + g * 150 + b * 29) >> 8;
      int idx = lum < 30 ? -1 : (lum < 90 ? 0 : (lum < 150 ? 1 : (lum < 205 ? 2 : 3)));
      uint16_t v = idx < 0 ? swap16(hex565(style_->lidColor)) : pal[idx];
      for (int y = by; y < by + 4 && y < EYE_H; y++)
        for (int x = bx; x < bx + 4; x++) fb[y * EYE_W + x] = v;
    }
  }
  span.first = (int16_t)yStart;
  span.last = (int16_t)yEnd;
}
