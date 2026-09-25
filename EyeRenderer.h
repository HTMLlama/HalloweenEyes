// EyeRenderer.h -- draws the eyes into 128x128 RGB565 framebuffers.
//
// The eye is modelled as a sphere with a flat iris disc set into it, viewed
// straight on. Every visible pixel is traced to either the sclera (shaded
// sphere with moving vein texture) or the iris plane (textured, dilating
// pupil), with anti-aliased edges so sub-pixel motion stays smooth.
//
// Both eyes look the same way, so rendering is split in two:
//   renderShared() shades the eyeball once per frame (the expensive part);
//   composeEye()   applies one eye's own lids, lid shadow, blink/wink and
//                  third eyelid on top of it (mostly straight copies).
//
// All buffers hold big-endian RGB565, ready to DMA to the SSD1351.
#pragma once

#include <stdint.h>

#include "EyeMath.h"
#include "EyeStyle.h"
#include "IrisBaker.h"

struct LidPose {
  float squint;  // 0..1 both lids narrow
  float anger;   // 0..1 inner end of the upper lid drops
  float droop;   // 0..1 heavy upper lid (sleepy)
  float widen;   // 0..1 lids pulled open (startled)
};

struct EyePose {
  float yaw;        // radians, + turns toward the panel's right edge
  float pitch;      // radians, + looks up
  float pupil;      // pupil size as a fraction of the iris radius
  float blink;      // 0 open .. 1 closed
  float nictitate;  // 0..1 third-eyelid sweep (styles with FX_NICTITATE)
  LidPose mood;
};

struct RowSpan {
  int16_t first, last;  // panel rows that contain eye content; first > last = none
};

class EyeRenderer {
 public:
  EyeRenderer();

  void setStyle(const EyeStyle* style, const EyeAssets* assets);

  // nasalOnLeft: true when this eye's inner corner (the side toward the nose)
  // is on the left of the panel as an onlooker sees it. Offsets shift the
  // whole eye to line up with the mask openings.
  void setEye(int eye, bool nasalOnLeft, int offsetX, int offsetY);

  // Shades the eyeball for this frame. Gaze and pupil come from pose0; each
  // pose's lids decide which rows are needed.
  void renderShared(const EyePose& pose0, const EyePose& pose1, uint32_t timeMs,
                    uint16_t* shared);

  // Builds eye 'eye' from the shared eyeball (call renderShared() first).
  void composeEye(int eye, uint32_t timeMs, const uint16_t* shared, uint16_t* fb,
                  RowSpan* span);

 private:
  struct Lids {
    float top[EYE_W], bot[EYE_W], wTop[EYE_W], wBot[EYE_W];
  };

  void buildLidTables(int eye);
  void computeLids(int eye, const EyePose& pose, float gazeYpx, Lids& out);
  void drawHighlight(uint16_t* sh, float hx, float hy, float rx, float ry,
                     float strength, float sBase, float sXc, float sYc,
                     float tBase, float tXc, float tYc, float cx, float cy);
  void glitch(uint16_t* fb, const RowSpan& span, uint32_t timeMs);
  void pixelate(uint16_t* fb, RowSpan& span);

  const EyeStyle* style_;
  const EyeAssets* assets_;
  bool nasalLeft_[2];
  int8_t offX_[2], offY_[2];

  // Lid shape tables (depend on style + eye), one entry per panel column.
  float archU_[2][EYE_W], archL_[2][EYE_W], corner_[2][EYE_W];
  float nasalW_[2][EYE_W], nasalLin_[2][EYE_W], un_[2][EYE_W];
  uint8_t inside_[2][EYE_W];

  // Per-frame results of renderShared() used by composeEye().
  Lids lids_[2];
  float nictitate_[2];
  uint16_t aoT_[64], aoB_[32];  // lid shadow by distance (half pixels)
  uint8_t aoTopRows_, aoBotRows_;

  Rng rng_;
  uint32_t glitchNext_ = 0, glitchUntil_ = 0;
};
