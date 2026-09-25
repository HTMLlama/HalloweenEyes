// EyeStyle.h -- the data that describes one eye design.
//
// A style is pure data: colours, iris pattern recipe, pupil shape, lid shape,
// lighting and animation flags. The procedural baker (IrisBaker.cpp) turns
// the recipe into textures and the renderer (EyeRenderer.cpp) draws it. The
// catalogue of styles lives in EyeStyles.cpp.
#pragma once

#include <stdint.h>

// How the iris texture is generated.
enum IrisPattern : uint8_t {
  IRIS_HUMAN,     // fibres, collarette, crypts, limbal ring
  IRIS_FELINE,    // smooth sunburst gradient with fine rays (cats, wolves)
  IRIS_REPTILE,   // dense striations plus dark reticulated speckle
  IRIS_EMBER,     // glowing corona fading to dark (demons, fire)
  IRIS_LAVA,      // cracked dark crust with glowing seams (dragon)
  IRIS_NEBULA,    // cosmic clouds, stars and an accretion ring
  IRIS_VOID,      // near black with faint veining (possessed)
  IRIS_BIOLUM,    // deep-sea blue with glowing photophores
  IRIS_TECH,      // machined rings, ticks and segments (robots)
  IRIS_CIRCUIT,   // glowing circuit traces
  IRIS_PHOSPHOR,  // green vector-display wireframe
  IRIS_HAZARD,    // industrial hazard stripes
};

enum PupilShape : uint8_t {
  PUPIL_ROUND,
  PUPIL_SLIT,      // vertical lens (cats, snakes)
  PUPIL_GOAT,      // horizontal bar (goats, demons)
  PUPIL_SQUARE,    // rounded square (machines)
  PUPIL_APERTURE,  // camera iris diaphragm with blades
  PUPIL_CUTTLE,    // W-shaped (cuttlefish)
  PUPIL_NONE,
};

enum LidStyle : uint8_t {
  LIDS_ALMOND,   // human: almond opening, lids track the pupil
  LIDS_ROUND,    // creature: big round opening
  LIDS_SHUTTER,  // machine: round housing with flat sliding shutters
};

// Animated overlays drawn over the iris every frame.
enum : uint16_t {
  OVL_SEG_RING = 1 << 0,   // rotating segmented ring
  OVL_SEG_RING2 = 1 << 1,  // outer counter-rotating ring
  OVL_SWEEP = 1 << 2,      // radar sweep
  OVL_RETICLE = 1 << 3,    // targeting ticks and brackets
  OVL_PULSE = 1 << 4,      // rings expanding out of the pupil
};

// Whole-eye effects.
enum : uint16_t {
  FX_SCANLINES = 1 << 0,  // CRT scanlines
  FX_FLICKER = 1 << 1,    // brightness flicker
  FX_GLITCH = 1 << 2,     // row tearing and channel splits
  FX_PIXELATE = 1 << 3,   // chunky 4x4 pixels with a 4-colour palette
  FX_ROLLBAR = 1 << 4,    // CRT roll bar
  FX_NICTITATE = 1 << 5,  // reptile third eyelid sweeps across on some blinks
  FX_TAPETUM = 1 << 6,    // eyeshine glowing inside the pupil
};

struct EyeStyle {
  const char* name;  // shown on the standby screen (upper case, <= 13 chars)

  // --- Geometry, in panel pixels -------------------------------------------
  uint8_t eyeRadius;   // radius of the eyeball sphere
  uint8_t irisRadius;  // iris radius when looking straight ahead
  float maxGaze;       // how far the eye can rotate, degrees

  // --- Sclera ---------------------------------------------------------------
  uint32_t sclera;        // base colour
  uint32_t scleraDetail;  // vein / mottling colour
  uint8_t veins;          // number of veins (0 = none)
  uint8_t mottle;         // 0..255 low-frequency blotchiness
  uint8_t shade;          // 0..255 how strongly the ball darkens toward edges

  // --- Iris -----------------------------------------------------------------
  IrisPattern pattern;
  uint32_t irisA;   // usually the inner colour
  uint32_t irisB;   // usually the outer colour
  uint32_t irisC;   // accent (collarette, speckle, hot core...)
  uint32_t limbus;  // dark ring at the iris edge
  uint8_t fibers;   // radial detail density
  uint8_t contrast; // texture contrast 0..255
  uint8_t emissive; // 0..255: how much the iris glows through lid shadow
  uint16_t seed;    // texture random seed
  float spin;       // texture rotation in revolutions per second

  // --- Pupil ----------------------------------------------------------------
  PupilShape pupil;
  uint32_t pupilColor;
  uint32_t pupilAccent;  // eyeshine / machine core colour
  float pupilSize;       // resting size as a fraction of the iris radius
  float pupilMin, pupilMax;

  // --- Light ----------------------------------------------------------------
  uint8_t highlight;   // corneal specular highlight strength
  uint32_t glowColor;  // bloom spilling from the iris onto the sclera
  uint8_t glow;        // bloom strength
  uint8_t pulse;       // emissive pulse depth 0..255
  float pulseHz;

  // --- Lids -----------------------------------------------------------------
  LidStyle lids;
  uint32_t lidColor;  // black blends into the mask
  uint32_t lidEdge;   // thin rim along the lid edge, 0 = none

  // --- Animation ------------------------------------------------------------
  uint16_t overlays;
  uint32_t overlayColor;
  uint16_t fx;
  uint32_t fxColor;  // palette tint for FX_PIXELATE
};

// Catalogue access (EyeStyles.cpp).
int eyeStyleCount();
void eyeStyleGet(int index, EyeStyle& out);

// Machine pupils (glowing cores and camera apertures) are baked into the iris
// texture and resized by the dilation remap, instead of being drawn per pixel.
inline bool styleBakesPupil(const EyeStyle& s) {
  if (s.pupil == PUPIL_APERTURE) return true;
  if (s.pupil != PUPIL_ROUND && s.pupil != PUPIL_SQUARE) return false;
  const uint32_t c = s.pupilColor;
  const uint32_t lum = (((c >> 16) & 255) * 77 + ((c >> 8) & 255) * 150 + (c & 255) * 29) >> 8;
  return lum > 115;  // a light pupil colour means a glowing core
}
