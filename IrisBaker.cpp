// IrisBaker.cpp -- procedural iris and sclera textures.
//
// Each iris pattern is a function of polar position (normalised radius rn,
// angle in turns a) plus Cartesian s,t for non-radial detail. Radial noise is
// periodic in angle so irises never show a seam.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O2")
#endif

#include "IrisBaker.h"

#include "EyeMath.h"

namespace {

inline float sq(float x) { return x * x; }


struct BakeCtx {
  const EyeStyle* s;
  int N;
  float p0;  // resting pupil radius the pattern is laid out around
  uint32_t seed;
  Col A, B, C, L;
  float k;  // contrast 0..1
  int F;    // fibre count
};

// Radial fibres: periodic in angle, gently wavy along the radius.
inline float fibres(float a, float rn, int count, uint32_t seed, int octaves) {
  float warp = (valueNoise(a * 7.0f, rn * 3.0f, seed + 77, 7) - 0.5f) * 0.9f;
  return fbm(a * count + warp, rn * 4.0f, seed, octaves, count);
}

// Distances to the nearest and second-nearest jittered grid points.
inline void voronoi(float x, float y, uint32_t seed, float& f1, float& f2) {
  int ix = (int)floorf(x), iy = (int)floorf(y);
  float d1 = 1e9f, d2 = 1e9f;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int cx = ix + dx, cy = iy + dy;
      float px = cx + 0.1f + 0.8f * hash2f(cx, cy, seed);
      float py = cy + 0.1f + 0.8f * hash2f(cx, cy, seed + 1);
      float d = sq(px - x) + sq(py - y);
      if (d < d1) {
        d2 = d1;
        d1 = d;
      } else if (d < d2) {
        d2 = d;
      }
    }
  }
  f1 = fsqrt(d1);
  f2 = fsqrt(d2);
}

inline Col limbal(const BakeCtx& c, Col base, float rn, float start,
                  float strength) {
  float l = smoothstep(start, 1.0f, rn);
  return mixc(base, c.L, l * l * strength);
}

// --- Patterns ---------------------------------------------------------------

Col patHuman(const BakeCtx& c, float rn, float a) {
  // Wavy collarette splitting the pupillary and ciliary zones.
  float cr = 0.34f + 0.12f * (valueNoise(a * 9.0f, 0.5f, c.seed + 1, 9) - 0.5f);
  float zone = smoothstep(cr - 0.07f, cr + 0.07f, rn);
  Col inner = mixc(c.A, c.C, smoothstep(0.02f, cr, rn) * 0.22f);
  Col outer = mixc(mixc(c.B, c.C, 0.14f), c.B, smoothstep(cr, 0.95f, rn));
  Col base = mixc(inner, outer, zone);

  // Two scales of stromal fibres.
  float f1 = fibres(a, rn, c.F, c.seed + 2, 3);
  float f2 = valueNoise(a * c.F * 3.0f, rn * 9.0f, c.seed + 3, c.F * 3);
  float fib = 0.62f * f1 + 0.38f * f2;
  base = mulc(base, (1.0f - 0.55f * c.k) + 1.1f * c.k * fib);

  // Raised collarette ridge.
  float ridge = expf(-sq((rn - cr) / 0.045f));
  base = addc(base, mulc(c.C, 0.16f * ridge * (0.4f + f1)));

  // Crypts: dark lacunae just outside the collarette.
  float cn = valueNoise(a * 22.0f, rn * 7.0f, c.seed + 4, 22);
  float cmask = smoothstep(0.66f, 0.86f, cn) * smoothstep(cr - 0.08f, cr, rn) *
                (1.0f - smoothstep(cr + 0.22f, cr + 0.42f, rn));
  base = mulc(base, 1.0f - 0.42f * cmask);

  // Contraction furrows in the outer zone.
  float furrow = 0.5f + 0.5f * cosf((rn - cr) * 38.0f +
                                    valueNoise(a * 6.0f, 1.0f, c.seed + 5, 6) * 4.0f);
  base = mulc(base, 1.0f - 0.10f * furrow * smoothstep(cr + 0.1f, 0.9f, rn));

  // Pupillary ruff and limbal ring.
  base = mixc(base, colHex(0x140A04), smoothstep(0.07f, 0.0f, rn) * 0.85f);
  return limbal(c, base, rn, 0.74f, 0.97f);
}

Col patFeline(const BakeCtx& c, float rn, float a) {
  Col base = mixc(c.A, c.B, powf(smoothstep(0.0f, 1.0f, rn), 0.8f));
  float rays = fibres(a, rn, c.F, c.seed + 2, 3);
  base = mulc(base, (1.0f - 0.4f * c.k) + 0.8f * c.k * rays);
  float fine = valueNoise(a * c.F * 3.0f, rn * 2.0f, c.seed + 9, c.F * 3);
  base = mulc(base, 0.9f + 0.2f * fine);
  // Warm glow hugging the pupil.
  base = addc(base, mulc(c.C, 0.35f * expf(-rn / 0.12f)));
  // Sparse dark flecks.
  float sp = valueNoise(a * 90.0f, rn * 18.0f, c.seed + 11, 90);
  base = mulc(base, 1.0f - 0.45f * smoothstep(0.76f, 0.9f, sp) * smoothstep(0.15f, 0.4f, rn));
  base = mixc(base, colHex(0x0A0604), smoothstep(0.05f, 0.0f, rn) * 0.7f);
  return limbal(c, base, rn, 0.70f, 0.95f);
}

Col patReptile(const BakeCtx& c, float rn, float a, float s, float t) {
  Col base = mixc(c.A, c.B, powf(rn, 0.75f));
  float stri = valueNoise(a * c.F * 2.0f, rn * 10.0f, c.seed, c.F * 2);
  base = mulc(base, (1.0f - 0.5f * c.k) + 1.0f * c.k * stri);
  // Reticulated dark network (Cartesian so it is not purely radial).
  float n = fbm(s * 5.0f + 3.0f, t * 5.0f, c.seed + 2, 3, 0);
  float net = smoothstep(0.55f, 0.66f, n);
  base = mixc(base, c.C, net * 0.8f);
  float fl = valueNoise(s * 24.0f, t * 24.0f, c.seed + 3, 0);
  base = mixc(base, c.C, smoothstep(0.74f, 0.9f, fl) * 0.7f);
  // Bright ring around the pupil.
  base = addc(base, mulc(c.A, 0.3f * expf(-sq(rn / 0.07f))));
  return limbal(c, base, rn, 0.66f, 1.0f);
}

Col patEmber(const BakeCtx& c, float rn, float a) {
  float flame = fibres(a, rn * 0.8f, c.F, c.seed + 2, 4);
  float heat = expf(-rn * 2.4f) * (0.5f + 0.95f * flame) + 0.12f * flame;
  heat = saturate(heat);
  Col col = heat < 0.5f ? mixc(c.B, c.A, heat * 2.0f)
                        : mixc(c.A, c.C, (heat - 0.5f) * 2.0f);
  return limbal(c, col, rn, 0.72f, 0.95f);
}

Col patLava(const BakeCtx& c, float rn, float a, float s, float t) {
  // Molten, fibrous core that cools into dark cracked plates toward the rim.
  float flame = fibres(a, rn * 0.7f, c.F, c.seed + 2, 4);
  float heat = saturate(expf(-rn * 1.5f) * (0.55f + 0.8f * flame));
  Col molten = heat < 0.5f ? mixc(mulc(c.A, 0.45f), c.A, heat * 2.0f)
                           : mixc(c.A, c.C, (heat - 0.5f) * 2.0f);
  float f1, f2;
  voronoi(s * 6.5f + 1.7f, t * 6.5f, c.seed, f1, f2);
  float edge = f2 - f1;
  float plate = smoothstep(0.05f, 0.2f, edge) * smoothstep(0.3f, 0.75f, rn);
  float rock = fbm(s * 9.0f, t * 9.0f, c.seed + 3, 3, 0);
  Col crust = mixc(c.L, c.B, rock * 0.7f);
  // Seams between plates glow; plates darken.
  Col col = mixc(molten, crust, plate * 0.92f);
  float seamGlow = smoothstep(0.22f, 0.0f, edge) * smoothstep(0.25f, 0.6f, rn);
  col = addc(col, mulc(c.A, 0.35f * seamGlow));
  return limbal(c, col, rn, 0.82f, 0.9f);
}

Col patNebula(const BakeCtx& c, float rn, float s, float t, int i, int j) {
  float n1 = fbm(s * 2.0f + 11.0f, t * 2.0f, c.seed, 5, 0);
  float n2 = fbm(s * 3.3f, t * 3.3f + 7.0f, c.seed + 1, 4, 0);
  Col col = mixc(colHex(0x04020A), c.A, sq(n1) * 1.7f);
  col = addc(col, mulc(c.B, powf(n2, 3.0f) * 1.5f));
  col = mulc(col, 0.55f + 0.9f * fbm(s * 5.0f, t * 5.0f, c.seed + 2, 3, 0));
  uint32_t h = hash2i(i, j, c.seed + 3);
  if ((h % 1000u) < 7u) {
    float b = 0.55f + 0.45f * ((h >> 12) & 255) / 255.0f;
    col = addc(col, colRGB(b, b, b * 0.95f));
  }
  // Accretion ring hugging the pupil.
  col = addc(col, mulc(c.C, 1.2f * expf(-sq((rn - 0.05f) / 0.06f))));
  return mixc(col, c.L, smoothstep(0.86f, 1.0f, rn) * 0.75f);
}

Col patVoid(const BakeCtx& c, float rn, float a) {
  float f = fibres(a, rn, c.F, c.seed + 2, 3);
  Col col = addc(c.A, mulc(c.B, powf(f, 3.0f) * 1.1f * (1.0f - rn * 0.5f)));
  col = addc(col, mulc(c.C, 0.5f * expf(-sq(rn / 0.07f))));
  return limbal(c, col, rn, 0.8f, 0.9f);
}

Col patBiolum(const BakeCtx& c, float rn, float a, float s, float t) {
  float fib = fibres(a, rn, c.F, c.seed + 2, 3);
  Col col = mulc(mixc(c.A, c.B, rn), 0.7f + 0.6f * fib);
  // Glowing threads along the brightest fibres.
  float thread = smoothstep(0.68f, 0.8f, fib) * (0.35f + 0.65f * (1.0f - rn));
  col = addc(col, mulc(c.C, 0.55f * thread));
  // Luminous rings hugging the pupil and the rim.
  col = addc(col, mulc(c.C, 0.9f * expf(-sq((rn - 0.05f) / 0.05f))));
  col = addc(col, mulc(c.C, 0.35f * expf(-sq((rn - 0.9f) / 0.035f))));
  // One ring of small photophores.
  const float n = 11.0f;
  float ang = (floorf(a * n + 0.5f)) / n * EYE_TAU;
  float rr = c.p0 + 0.58f * (1.0f - c.p0);
  float d2 = sq(s - cosf(ang) * rr) + sq(t - sinf(ang) * rr);
  col = addc(col, mulc(c.C, expf(-d2 / 0.0011f) + 0.22f * expf(-d2 / 0.006f)));
  return limbal(c, col, rn, 0.8f, 0.9f);
}

Col patTech(const BakeCtx& c, float rn, float a) {
  float brushed = valueNoise(a * 180.0f, rn * 2.0f, c.seed, 180);
  Col col = mixc(colHex(0x07090C), colHex(0x161C22), brushed);
  // Bevelled rings.
  const float rings[4] = {0.12f, 0.44f, 0.76f, 0.97f};
  for (int k = 0; k < 4; k++) {
    float line = expf(-sq((rn - rings[k]) / 0.014f));
    col = addc(col, mulc(c.A, 0.85f * line));
  }
  // Segmented light band.
  float seg = fastFrac(a * 16.0f);
  float lit = smoothstep(0.0f, 0.04f, seg) * smoothstep(0.78f, 0.74f, seg);
  float band = smoothstep(0.49f, 0.51f, rn) * smoothstep(0.71f, 0.69f, rn);
  col = mixc(col, mulc(c.B, 0.35f + 0.35f * (1.0f - rn)), lit * band);
  // Graduation ticks.
  float tick = smoothstep(0.2f, 0.1f, fastFrac(a * 72.0f));
  float major = smoothstep(0.08f, 0.04f, fastFrac(a * 12.0f + 0.02f));
  float tickBand = smoothstep(0.83f, 0.85f, rn) * smoothstep(0.93f, 0.91f, rn);
  float majBand = smoothstep(0.80f, 0.82f, rn) * smoothstep(0.95f, 0.93f, rn);
  col = addc(col, mulc(c.A, 0.55f * tick * tickBand + 0.9f * major * majBand));
  col = addc(col, mulc(c.B, 0.45f * expf(-rn / 0.18f)));
  return col;
}

Col patCircuit(const BakeCtx& c, float rn, float a, float r) {
  Col col = colHex(0x060403);
  float pxPerUnit = c.N * 0.5f;
  float glow = 0.0f;
  // Radial traces in random sectors.
  const int sectors = 36;
  int sec = (int)floorf(a * sectors);
  uint32_t h = hash32((uint32_t)sec * 2654435761u ^ c.seed);
  if ((h & 3u) != 0u) {
    float r0 = 0.12f + ((h >> 8) & 255) / 255.0f * 0.35f;
    float r1 = r0 + 0.2f + ((h >> 16) & 255) / 255.0f * 0.45f;
    float off = (a * sectors - sec - 0.5f) / sectors * EYE_TAU * r * pxPerUnit;
    float d = fabsf(off);
    float on = smoothstep(r0 - 0.01f, r0 + 0.01f, rn) * smoothstep(r1 + 0.01f, r1 - 0.01f, rn);
    glow = maxf(glow, on * expf(-sq(d / 0.75f)));
    // Via pad at the outer end.
    float dv = sq(off) + sq((rn - r1) * pxPerUnit);
    glow = maxf(glow, expf(-dv / 2.2f) * 1.2f);
  }
  // Arc traces at fixed radii.
  const float lv[4] = {0.22f, 0.45f, 0.66f, 0.9f};
  for (int k = 0; k < 4; k++) {
    int seg = (int)floorf(a * 20.0f + k * 0.37f);
    uint32_t h2 = hash32((uint32_t)(seg * 7 + k * 131) ^ (c.seed * 3u));
    if (h2 & 1u) {
      float d = fabsf(rn - lv[k]) * pxPerUnit * (1.0f - c.p0);
      glow = maxf(glow, expf(-sq(d / 0.75f)));
    }
  }
  col = addc(col, mulc(c.A, glow));
  col = addc(col, mulc(c.B, 0.35f * expf(-rn / 0.16f)));
  col = addc(col, mulc(c.A, 0.06f * valueNoise(a * 40.0f, rn * 6.0f, c.seed + 4, 40)));
  return col;
}

Col patPhosphor(const BakeCtx& c, float rn, float a, float r) {
  float px = c.N * 0.5f;
  float line = 0.0f;
  for (int k = 1; k <= 4; k++) {
    float d = (rn - k * 0.25f) * px * (1.0f - c.p0);
    line = maxf(line, expf(-sq(d / 0.7f)));
  }
  float spoke = fabsf(sinf(a * EYE_TAU * 6.0f)) * r * px;
  line = maxf(line, expf(-sq(spoke / 0.7f)) * smoothstep(0.05f, 0.15f, rn));
  Col col = mulc(c.A, line);
  return addc(col, mulc(c.B, 0.12f + 0.25f * expf(-rn / 0.3f)));
}

Col patHazard(const BakeCtx& c, float rn, float a) {
  float brushed = valueNoise(a * 160.0f, rn * 2.0f, c.seed, 160);
  Col col = mixc(colHex(0x0B0906), colHex(0x1C1812), brushed);
  float band = smoothstep(0.6f, 0.62f, rn) * smoothstep(0.95f, 0.93f, rn);
  float stripe = smoothstep(0.46f, 0.5f, fastFrac(a * 14.0f + rn * 1.4f)) *
                 smoothstep(1.0f, 0.96f, fastFrac(a * 14.0f + rn * 1.4f));
  col = mixc(col, mulc(c.A, 0.9f), band * stripe);
  const float rings[3] = {0.14f, 0.58f, 0.97f};
  for (int k = 0; k < 3; k++) col = addc(col, mulc(c.A, 0.7f * expf(-sq((rn - rings[k]) / 0.015f))));
  col = addc(col, mulc(c.B, 0.5f * expf(-rn / 0.2f)));
  return col;
}

// Glowing machine cores and camera-aperture blades, drawn at the resting
// pupil size (the renderer's dilation remap resizes them).
Col bakedPupil(const BakeCtx& c, float s, float t, float r, float th, Col base) {
  const EyeStyle& S = *c.s;
  const float p = c.p0;
  const float texel = 2.0f / c.N;
  const Col pc = colHex(S.pupilColor), acc = colHex(S.pupilAccent);
  if (S.pupil == PUPIL_APERTURE) {
    const int n = 7;
    const float step = EYE_TAU / n;
    const float al = th - floorf(th / step) * step - step * 0.5f;
    const float d = r - p * cosf(EYE_PI / n) / cosf(al);
    if (r < 0.9f && d > -texel) {
      // Blades: spiral seams, alternating tone, a coloured rim near the lens.
      const float sp = (th + 1.25f * (r - p)) / step;
      const float seam = fastFrac(sp);
      const int bladeIdx = imod((int)floorf(sp), n);  // same blade all the way round
      const float tone = 0.78f + 0.22f * ((bladeIdx * 3) % n) / (n - 1.0f);
      const float lit = 0.55f + 0.45f * cosf(th - 2.2f);
      const float seamTex = seam * step * r / texel;
      const float line = seamTex < 3.0f ? (1.0f - seamTex / 3.0f) * 0.8f : 0.0f;
      const float dp = maxf(d, 0.0f) / texel;
      const float rim = 1.0f / (1.0f + dp * 0.45f + dp * dp * 0.1f);
      const float br = 30.0f / 255.0f * tone * lit * (1.0f - line);
      Col blade = addc(colRGB(br, br * 1.05f, br * 1.12f), mulc(acc, 0.55f * rim));
      base = mixc(base, blade, smoothstep(0.9f, 0.86f, r));
    }
    // The lens behind the opening glows toward its centre.
    const float cov = saturate(0.5f - d / texel);
    const Col lens = mixc(pc, acc, saturate(1.0f - r / p) * 0.9f);
    return mixc(base, lens, cov);
  }
  // Bright core (round or rounded square) with a soft halo.
  float d;
  if (S.pupil == PUPIL_SQUARE) {
    const float rc = p * 0.35f, core = p - rc;
    const float qx = maxf(fabsf(s) - core, 0.0f), qy = maxf(fabsf(t) - core, 0.0f);
    d = (qx > 0.0f || qy > 0.0f) ? fsqrt(qx * qx + qy * qy) - rc
                                 : maxf(fabsf(s), fabsf(t)) - p;
  } else {
    d = r - p;
  }
  if (d > 0.0f) base = addc(base, mulc(pc, 0.7f * expf(-d * 9.0f)));
  const float cov = saturate(0.5f - d / texel);
  return mixc(base, mulc(pc, 0.8f + 0.25f * saturate(-d / p)), cov);
}

}  // namespace

// ---------------------------------------------------------------------------

void IrisBaker::begin(const EyeStyle& style, EyeAssets& assets) {
  s_ = &style;
  a_ = &assets;
  // Texture resolution tracks the on-screen iris size (about 1 texel/pixel).
  int n = style.irisRadius * 2 + 4;
  if (n > EyeAssets::IRIS_MAX) n = EyeAssets::IRIS_MAX;
  assets.irisN = n & ~1;
  phase_ = PHASE_IRIS;
  row_ = 0;
}

bool IrisBaker::step(int rows) {
  while (rows-- > 0 && phase_ < PHASE_DONE) {
    if (phase_ == PHASE_IRIS) {
      irisRow(row_++);
      if (row_ >= a_->irisN) {
        phase_ = PHASE_SCLERA;
        row_ = 0;
      }
    } else if (phase_ == PHASE_SCLERA) {
      scleraRow(row_++);
      if (row_ >= EyeAssets::SCLERA_N) phase_ = PHASE_VEINS;
    } else {
      veins();
      phase_ = PHASE_DONE;
    }
  }
  return done();
}

void IrisBaker::bakeAll(const EyeStyle& style, EyeAssets& assets) {
  IrisBaker b;
  b.begin(style, assets);
  while (!b.step(64)) {
  }
}

void IrisBaker::irisRow(int j) {
  const EyeStyle& S = *s_;
  BakeCtx c;
  c.s = &S;
  c.N = a_->irisN;
  c.p0 = S.pupilSize;
  c.seed = S.seed * 7919u + 17u;
  c.A = colHex(S.irisA);
  c.B = colHex(S.irisB);
  c.C = colHex(S.irisC);
  c.L = colHex(S.limbus);
  c.k = S.contrast / 255.0f;
  c.F = S.fibers < 8 ? 8 : S.fibers;

  const int N = c.N;
  const float inv = 2.0f / N;
  const bool baked = styleBakesPupil(S);
  uint16_t* row = &a_->iris[j * N];
  const float t = 1.0f - (j + 0.5f) * inv;
  for (int i = 0; i < N; i++) {
    const float s = (i + 0.5f) * inv - 1.0f;
    const float r = fsqrt(s * s + t * t);
    const float th = fastAtan2(t, s);
    float a = th * (1.0f / EYE_TAU);
    if (a < 0.0f) a += 1.0f;
    float rn = (r - c.p0) / (1.0f - c.p0);
    if (rn < 0.0f) rn = 0.0f;
    Col col;
    if (r > 1.03f) {
      col = c.L;
    } else {
      switch (S.pattern) {
        case IRIS_FELINE: col = patFeline(c, rn, a); break;
        case IRIS_REPTILE: col = patReptile(c, rn, a, s, t); break;
        case IRIS_EMBER: col = patEmber(c, rn, a); break;
        case IRIS_LAVA: col = patLava(c, rn, a, s, t); break;
        case IRIS_NEBULA: col = patNebula(c, rn, s, t, i, j); break;
        case IRIS_VOID: col = patVoid(c, rn, a); break;
        case IRIS_BIOLUM: col = patBiolum(c, rn, a, s, t); break;
        case IRIS_TECH: col = patTech(c, rn, a); break;
        case IRIS_CIRCUIT: col = patCircuit(c, rn, a, r); break;
        case IRIS_PHOSPHOR: col = patPhosphor(c, rn, a, r); break;
        case IRIS_HAZARD: col = patHazard(c, rn, a); break;
        case IRIS_HUMAN:
        default: col = patHuman(c, rn, a); break;
      }
      if (baked) col = bakedPupil(c, s, t, r, th, col);
    }
    row[i] = col565(col, kBayer4[((j & 3) << 2) | (i & 3)] >> 1);
  }
}

void IrisBaker::scleraRow(int j) {
  const EyeStyle& S = *s_;
  const int N = EyeAssets::SCLERA_N;
  uint8_t* row = &a_->sclera[j * N];
  if (!S.mottle) {
    memset(row, 0, N);
    return;
  }
  const float amt = S.mottle / 255.0f;
  const uint32_t seed = S.seed * 104729u + 5u;
  const float lb = 1.0f - (j + 0.5f) * (2.0f / N);
  for (int i = 0; i < N; i++) {
    float la = (i + 0.5f) * (2.0f / N) - 1.0f;
    float n = fbm(la * 2.6f, lb * 2.6f, seed, 4, 0);
    float v = saturate((n - 0.32f) * 2.2f) * amt;
    row[i] = (uint8_t)(v * 255.0f);
  }
}

namespace {

void stamp(uint8_t* map, int N, float x, float y, float w, float inten) {
  int x0 = (int)floorf(x - w - 1.0f), x1 = (int)ceilf(x + w + 1.0f);
  int y0 = (int)floorf(y - w - 1.0f), y1 = (int)ceilf(y + w + 1.0f);
  for (int yy = y0; yy <= y1; yy++) {
    if (yy < 0 || yy >= N) continue;
    for (int xx = x0; xx <= x1; xx++) {
      if (xx < 0 || xx >= N) continue;
      float d = fsqrt(sq(xx + 0.5f - x) + sq(yy + 0.5f - y));
      float v = inten * saturate(w + 0.5f - d);
      int iv = (int)(v * 255.0f);
      uint8_t& m = map[yy * N + xx];
      if (iv > m) m = (uint8_t)(iv > 255 ? 255 : iv);
    }
  }
}

struct VeinSeed {
  float x, y, dir, w, inten;
  int steps;
};

}  // namespace

void IrisBaker::veins() {
  const EyeStyle& S = *s_;
  if (!S.veins) return;
  const int N = EyeAssets::SCLERA_N;
  const float c = N * 0.5f;
  // Veins run from the back of the eye toward (not into) the iris.
  const float irisEdge = (float)S.irisRadius / (float)S.eyeRadius;
  Rng rng(S.seed * 31337u + 99u);
  VeinSeed stack[24];
  int sp = 0;
  for (int v = 0; v < S.veins; v++) {
    // Most veins sit in the corners of the eye (left/right), like real ones.
    float base = rng.chance(0.5f) ? 0.0f : EYE_PI;
    float ang = base + rng.range(-0.75f, 0.75f);
    if (rng.chance(0.2f)) ang = rng.range(0.0f, EYE_TAU);
    float r0 = 0.985f;
    VeinSeed vs;
    vs.x = c + cosf(ang) * r0 * c;
    vs.y = c - sinf(ang) * r0 * c;
    vs.dir = ang + EYE_PI + rng.range(-0.35f, 0.35f);
    vs.w = rng.range(0.6f, 1.1f);
    vs.inten = rng.range(0.35f, 0.75f);
    float reach = rng.range(0.12f, 0.45f);
    vs.steps = (int)((r0 - irisEdge - 0.03f) * c * (1.0f - reach) / 0.7f);
    if (vs.steps < 8) vs.steps = 8;
    stack[sp++] = vs;
    while (sp > 0) {
      VeinSeed cur = stack[--sp];
      for (int st = 0; st < cur.steps; st++) {
        float f = 1.0f - (float)st / cur.steps;
        stamp(a_->sclera, N, cur.x, cur.y, cur.w * (0.35f + 0.65f * f),
              cur.inten * (0.35f + 0.65f * f));
        cur.dir += rng.range(-0.28f, 0.28f);
        cur.x += cosf(cur.dir) * 0.7f;
        cur.y -= sinf(cur.dir) * 0.7f;
        float dx = cur.x - c, dy = cur.y - c;
        if (dx * dx + dy * dy < sq((irisEdge + 0.02f) * c)) break;
        if (sp < 23 && cur.steps > 12 && rng.chance(0.035f)) {
          VeinSeed b = cur;
          b.dir += rng.chance(0.5f) ? rng.range(0.5f, 1.0f) : -rng.range(0.5f, 1.0f);
          b.w *= 0.65f;
          b.inten *= 0.8f;
          b.steps = (cur.steps - st) / 2;
          stack[sp++] = b;
        }
      }
    }
  }
}
