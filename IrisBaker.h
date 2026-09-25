// IrisBaker.h -- procedural texture generation for a style.
//
// Every style's iris and sclera detail are generated at load time instead of
// being stored in flash, so there is no art to maintain and styles cost only a
// few bytes each. Baking can run incrementally (a few rows per frame) so the
// standby animation keeps moving while the next style is prepared.
#pragma once

#include <stdint.h>

#include "EyeStyle.h"

struct EyeAssets {
  enum { IRIS_MAX = 104, SCLERA_N = 128 };
  // Iris texture: native-order RGB565, irisN x irisN texels covering the iris
  // disc s,t in [-1,1] (t up). Sized to the style's on-screen iris (about one
  // texel per pixel), so iris radii up to 50 px fit.
  uint16_t iris[IRIS_MAX * IRIS_MAX];
  int irisN;
  // Sclera detail: 0..255 amount of scleraDetail colour, indexed by the
  // eyeball-local coordinates (la, lb) in [-1,1].
  uint8_t sclera[SCLERA_N * SCLERA_N];
};

class IrisBaker {
 public:
  // Prepare to bake 'style' into 'assets' (does no heavy work).
  void begin(const EyeStyle& style, EyeAssets& assets);
  // Bake up to 'rows' texture rows. Returns true once everything is done.
  bool step(int rows);
  bool done() const { return phase_ >= PHASE_DONE; }
  // Convenience: bake everything now.
  static void bakeAll(const EyeStyle& style, EyeAssets& assets);

 private:
  enum { PHASE_IRIS, PHASE_SCLERA, PHASE_VEINS, PHASE_DONE };
  void irisRow(int j);
  void scleraRow(int j);
  void veins();

  const EyeStyle* s_ = nullptr;
  EyeAssets* a_ = nullptr;
  int phase_ = PHASE_DONE;
  int row_ = 0;
};
