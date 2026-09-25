// EyeStyles.cpp -- the eye catalogue.
//
// Each style starts from a family preset (human / creature / machine) and then
// overrides what makes it unique. Colours are 0xRRGGBB. To add a style, write
// a function below and add it to kStyles[]. To drop one from the button
// rotation without deleting it, see STYLE_PLAYLIST in config.h.
#include <string.h>

#include "EyeStyle.h"

namespace {

// --- Family presets ----------------------------------------------------------

void base(EyeStyle& s) {
  memset(&s, 0, sizeof(s));
  s.name = "EYE";
  s.eyeRadius = 72;
  s.irisRadius = 34;
  s.maxGaze = 26.0f;
  s.sclera = 0xE6DED6;
  s.scleraDetail = 0xB23A34;
  s.veins = 7;
  s.mottle = 50;
  s.shade = 175;
  s.pattern = IRIS_HUMAN;
  s.limbus = 0x0C0C10;
  s.fibers = 60;
  s.contrast = 165;
  s.seed = 1;
  s.pupil = PUPIL_ROUND;
  s.pupilColor = 0x040404;
  s.pupilSize = 0.34f;
  s.pupilMin = 0.22f;
  s.pupilMax = 0.58f;
  s.highlight = 210;
  s.pulseHz = 0.5f;
  s.lids = LIDS_ALMOND;
}

void human(EyeStyle& s) { base(s); }

void creature(EyeStyle& s) {
  base(s);
  s.eyeRadius = 70;
  s.irisRadius = 50;
  s.maxGaze = 22.0f;
  s.sclera = 0x2A2418;
  s.scleraDetail = 0x4C3C22;
  s.veins = 0;
  s.mottle = 140;
  s.shade = 205;
  s.pattern = IRIS_FELINE;
  s.lids = LIDS_ROUND;
  s.highlight = 225;
  s.fibers = 70;
}

void machine(EyeStyle& s) {
  base(s);
  s.eyeRadius = 96;
  s.irisRadius = 43;
  s.maxGaze = 16.0f;
  s.sclera = 0x161B20;
  s.scleraDetail = 0x2E363E;
  s.veins = 0;
  s.mottle = 160;
  s.shade = 150;
  s.pattern = IRIS_TECH;
  s.lids = LIDS_SHUTTER;
  s.highlight = 150;
  s.emissive = 220;
  s.pupilSize = 0.2f;
  s.pupilMin = 0.12f;
  s.pupilMax = 0.34f;
}

// --- Human ------------------------------------------------------------------

void skyBlue(EyeStyle& s) {
  human(s);
  s.name = "SKY BLUE";
  s.irisA = 0x6C98C0;
  s.irisB = 0x2C68A8;
  s.irisC = 0xB2CCE0;
  s.limbus = 0x0A1830;
  s.seed = 11;
}

void emerald(EyeStyle& s) {
  human(s);
  s.name = "EMERALD";
  s.irisA = 0xB88C2C;
  s.irisB = 0x2A8A46;
  s.irisC = 0xDCC462;
  s.limbus = 0x08200E;
  s.seed = 12;
}

void hazel(EyeStyle& s) {
  human(s);
  s.name = "HAZEL";
  s.irisA = 0x7C4A1A;
  s.irisB = 0x6C8442;
  s.irisC = 0xCC9C4A;
  s.limbus = 0x1E1206;
  s.seed = 13;
}

void storm(EyeStyle& s) {
  human(s);
  s.name = "STORM GRAY";
  s.irisA = 0x9CA8B0;
  s.irisB = 0x4C6070;
  s.irisC = 0xCCD6DC;
  s.limbus = 0x121A20;
  s.veins = 11;
  s.seed = 14;
}

void possessed(EyeStyle& s) {
  human(s);
  s.name = "POSSESSED";
  s.irisRadius = 40;
  s.sclera = 0xD6CBC2;
  s.scleraDetail = 0xA0100E;
  s.veins = 24;
  s.mottle = 90;
  s.pattern = IRIS_VOID;
  s.irisA = 0x040202;
  s.irisB = 0x3C0A0A;
  s.irisC = 0x7A1E1E;
  s.limbus = 0x000000;
  s.pupilColor = 0xFFF4E8;  // a pinprick of cold light where the pupil should be
  s.pupilSize = 0.12f;
  s.pupilMin = 0.08f;
  s.pupilMax = 0.2f;
  s.highlight = 70;
  s.seed = 15;
}

void ghost(EyeStyle& s) {
  human(s);
  s.name = "GHOST";
  s.irisRadius = 36;
  s.sclera = 0xD4E0EA;
  s.veins = 0;
  s.mottle = 30;
  s.pattern = IRIS_FELINE;
  s.irisA = 0xEEF6FF;
  s.irisB = 0x98B6CE;
  s.irisC = 0xFFFFFF;
  s.limbus = 0x5C748C;
  s.contrast = 90;
  s.pupilColor = 0x5A6878;  // milky, unseeing
  s.pupilSize = 0.2f;
  s.pupilMin = 0.16f;
  s.pupilMax = 0.28f;
  s.glowColor = 0xB8DCFF;
  s.glow = 70;
  s.highlight = 90;
  s.seed = 16;
}

// --- Creatures ----------------------------------------------------------------

void viper(EyeStyle& s) {
  creature(s);
  s.name = "VIPER";
  s.pattern = IRIS_REPTILE;
  s.sclera = 0x1C1608;
  s.irisA = 0xFFE24A;
  s.irisB = 0xB8780E;
  s.irisC = 0x2A1400;
  s.limbus = 0x120800;
  s.contrast = 190;
  s.pupil = PUPIL_SLIT;
  s.pupilColor = 0x020100;
  s.pupilSize = 0.12f;
  s.pupilMin = 0.06f;
  s.pupilMax = 0.5f;
  s.fx = FX_NICTITATE;
  s.seed = 21;
}

void basilisk(EyeStyle& s) {
  viper(s);
  s.name = "BASILISK";
  s.irisA = 0xFFEC80;
  s.irisB = 0xD08612;
  s.irisC = 0x3C1600;
  s.limbus = 0x1A0600;
  s.pupilColor = 0x3A0008;  // deep maroon slit
  s.glowColor = 0xFFB020;
  s.glow = 40;
  s.seed = 22;
}

void panther(EyeStyle& s) {
  creature(s);
  s.name = "PANTHER";
  s.sclera = 0x140A18;
  s.irisA = 0xE8ACFF;
  s.irisB = 0x561896;
  s.irisC = 0xFFDCFF;
  s.limbus = 0x12041C;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.14f;
  s.pupilMin = 0.07f;
  s.pupilMax = 0.62f;
  s.fx = FX_TAPETUM;
  s.pupilAccent = 0x38FF78;  // green eyeshine
  s.seed = 23;
}

void jadeCat(EyeStyle& s) {
  panther(s);
  s.name = "JADE CAT";
  s.sclera = 0x10180E;
  s.irisA = 0xD8FFBC;
  s.irisB = 0x38965A;
  s.irisC = 0xFFFFD4;
  s.limbus = 0x06200E;
  s.pupilAccent = 0xFFC040;
  s.seed = 24;
}

void wolf(EyeStyle& s) {
  creature(s);
  s.name = "WOLF";
  s.irisRadius = 46;
  s.sclera = 0x2A1C10;
  s.irisA = 0xFFD050;
  s.irisB = 0xBC6810;
  s.irisC = 0xFFE8A4;
  s.limbus = 0x281004;
  s.pupilSize = 0.3f;
  s.pupilMin = 0.2f;
  s.pupilMax = 0.52f;
  s.fx = FX_TAPETUM;
  s.pupilAccent = 0x70FFB0;
  s.seed = 25;
}

void goat(EyeStyle& s) {
  creature(s);
  s.name = "GOAT";
  s.sclera = 0x1E160C;
  s.irisA = 0xECB444;
  s.irisB = 0x8A5812;
  s.irisC = 0xFFE294;
  s.limbus = 0x1E0E02;
  s.pupil = PUPIL_GOAT;
  s.pupilSize = 0.2f;
  s.pupilMin = 0.13f;
  s.pupilMax = 0.34f;
  s.seed = 26;
}

void cuttlefish(EyeStyle& s) {
  creature(s);
  s.name = "CUTTLEFISH";
  s.sclera = 0x181A10;
  s.irisA = 0xD4BC6A;
  s.irisB = 0x566A30;
  s.irisC = 0xF4E6A8;
  s.limbus = 0x10160A;
  s.pupil = PUPIL_CUTTLE;
  s.pupilSize = 0.32f;
  s.pupilMin = 0.24f;
  s.pupilMax = 0.44f;
  s.seed = 27;
}

void leviathan(EyeStyle& s) {
  creature(s);
  s.name = "LEVIATHAN";
  s.pattern = IRIS_BIOLUM;
  s.sclera = 0x02060E;
  s.scleraDetail = 0x0A1830;
  s.irisA = 0x10306E;
  s.irisB = 0x030A20;
  s.irisC = 0x40E8FF;
  s.limbus = 0x010308;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.14f;
  s.pupilMin = 0.07f;
  s.pupilMax = 0.45f;
  s.glowColor = 0x2080FF;
  s.glow = 70;
  s.emissive = 160;
  s.pulse = 60;
  s.pulseHz = 0.25f;
  s.seed = 28;
}

void neonCat(EyeStyle& s) {
  creature(s);
  s.name = "NEON CAT";
  s.sclera = 0x0C0414;
  s.scleraDetail = 0x2A0A40;
  s.irisA = 0xFF5CF0;
  s.irisB = 0x6400B4;
  s.irisC = 0x60F0FF;
  s.limbus = 0x100020;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.13f;
  s.pupilMin = 0.07f;
  s.pupilMax = 0.55f;
  s.glowColor = 0xE040FF;
  s.glow = 95;
  s.emissive = 150;
  s.fx = FX_TAPETUM;
  s.pupilAccent = 0x40E8FF;
  s.seed = 29;
}

void toxic(EyeStyle& s) {
  creature(s);
  s.name = "TOXIC";
  s.pattern = IRIS_EMBER;
  s.sclera = 0x061004;
  s.scleraDetail = 0x143010;
  s.irisA = 0x9CFF30;
  s.irisB = 0x0A2600;
  s.irisC = 0xF0FFB8;
  s.limbus = 0x020800;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.12f;
  s.pupilMin = 0.06f;
  s.pupilMax = 0.4f;
  s.glowColor = 0x60FF20;
  s.glow = 120;
  s.emissive = 200;
  s.pulse = 90;
  s.pulseHz = 1.1f;
  s.seed = 30;
}

void dragon(EyeStyle& s) {
  creature(s);
  s.name = "DRAGON";
  s.pattern = IRIS_LAVA;
  s.sclera = 0x1A0804;
  s.scleraDetail = 0x3A1206;
  s.irisA = 0xFF7A10;
  s.irisB = 0x3A1408;
  s.irisC = 0xFFE890;
  s.limbus = 0x0A0302;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.11f;
  s.pupilMin = 0.05f;
  s.pupilMax = 0.4f;
  s.glowColor = 0xFF5000;
  s.glow = 110;
  s.emissive = 180;
  s.pulse = 70;
  s.pulseHz = 0.35f;
  s.spin = 0.012f;
  s.fx = FX_NICTITATE;
  s.seed = 31;
}

void solarFlare(EyeStyle& s) {
  creature(s);
  s.name = "SOLAR FLARE";
  s.pattern = IRIS_EMBER;
  s.sclera = 0x160602;
  s.irisA = 0xFF8A10;
  s.irisB = 0x401000;
  s.irisC = 0xFFF0B0;
  s.limbus = 0x100400;
  s.pupil = PUPIL_SLIT;
  s.pupilSize = 0.13f;
  s.pupilMin = 0.07f;
  s.pupilMax = 0.45f;
  s.glowColor = 0xFF7000;
  s.glow = 100;
  s.emissive = 190;
  s.pulse = 60;
  s.pulseHz = 0.8f;
  s.spin = 0.03f;
  s.seed = 32;
}

// --- Supernatural -------------------------------------------------------------

void redEclipse(EyeStyle& s) {
  creature(s);
  s.name = "RED ECLIPSE";
  s.pattern = IRIS_EMBER;
  s.sclera = 0x060000;
  s.scleraDetail = 0x1E0000;
  s.irisRadius = 46;
  s.irisA = 0xFF2A10;
  s.irisB = 0x280000;
  s.irisC = 0xFFD0A0;
  s.limbus = 0x0A0000;
  s.pupilColor = 0x080000;
  s.pupilSize = 0.38f;
  s.pupilMin = 0.3f;
  s.pupilMax = 0.5f;
  s.glowColor = 0xFF1000;
  s.glow = 140;
  s.emissive = 200;
  s.pulse = 50;
  s.pulseHz = 0.5f;
  s.highlight = 130;
  s.seed = 41;
}

void nebula(EyeStyle& s) {
  creature(s);
  s.name = "NEBULA";
  s.pattern = IRIS_NEBULA;
  s.sclera = 0x05030C;
  s.scleraDetail = 0x16103A;
  s.irisA = 0x9A20C8;
  s.irisB = 0x20C8FF;
  s.irisC = 0xFFB060;
  s.limbus = 0x080010;
  s.pupilColor = 0x000000;
  s.pupilSize = 0.26f;
  s.pupilMin = 0.2f;
  s.pupilMax = 0.4f;
  s.glowColor = 0x8040FF;
  s.glow = 80;
  s.emissive = 170;
  s.spin = 0.02f;
  s.seed = 42;
}

// --- Machines -----------------------------------------------------------------

void cyberdyne(EyeStyle& s) {
  machine(s);
  s.name = "CYBERDYNE HUD";
  s.irisA = 0x40E0FF;
  s.irisB = 0x0080B0;
  s.pupil = PUPIL_SQUARE;
  s.pupilColor = 0xE8FFFF;
  s.pupilSize = 0.16f;
  s.overlays = OVL_SEG_RING | OVL_SEG_RING2;
  s.overlayColor = 0x40E0FF;
  s.glowColor = 0x00A0FF;
  s.glow = 60;
  s.seed = 51;
}

void targeting(EyeStyle& s) {
  machine(s);
  s.name = "TARGETING";
  s.irisA = 0xFF3020;
  s.irisB = 0xA00800;
  s.pupil = PUPIL_SQUARE;
  s.pupilColor = 0xFFE040;
  s.pupilSize = 0.14f;
  s.overlays = OVL_RETICLE | OVL_SWEEP;
  s.overlayColor = 0xFF4020;
  s.glowColor = 0xFF2000;
  s.glow = 60;
  s.seed = 52;
}

void arcReactor(EyeStyle& s) {
  machine(s);
  s.name = "ARC REACTOR";
  s.irisA = 0x80F0FF;
  s.irisB = 0x00B0FF;
  s.pupilColor = 0xF0FFFF;
  s.pupilSize = 0.28f;
  s.pupilMin = 0.2f;
  s.pupilMax = 0.36f;
  s.overlays = OVL_SEG_RING | OVL_PULSE;
  s.overlayColor = 0x60E8FF;
  s.glowColor = 0x40C0FF;
  s.glow = 130;
  s.pulse = 60;
  s.pulseHz = 0.6f;
  s.emissive = 255;
  s.seed = 53;
}

void infiltrator(EyeStyle& s) {
  machine(s);
  s.name = "INFILTRATOR";
  s.irisA = 0xFF2010;
  s.irisB = 0x600000;
  s.pupil = PUPIL_APERTURE;
  s.pupilColor = 0x300000;
  s.pupilAccent = 0xFF3818;
  s.pupilSize = 0.3f;
  s.pupilMin = 0.12f;
  s.pupilMax = 0.5f;
  s.glowColor = 0xFF1000;
  s.glow = 110;
  s.pulse = 40;
  s.pulseHz = 0.4f;
  s.seed = 54;
}

void loader(EyeStyle& s) {
  machine(s);
  s.name = "LOADER";
  s.pattern = IRIS_HAZARD;
  s.irisA = 0xFFB000;
  s.irisB = 0xFF6A00;
  s.pupil = PUPIL_APERTURE;
  s.pupilColor = 0x100800;
  s.pupilAccent = 0xFFB000;
  s.pupilSize = 0.28f;
  s.pupilMin = 0.14f;
  s.pupilMax = 0.44f;
  s.glowColor = 0xFF8000;
  s.glow = 50;
  s.lidEdge = 0xFFA000;
  s.seed = 55;
}

void crtTerminal(EyeStyle& s) {
  machine(s);
  s.name = "CRT TERMINAL";
  s.pattern = IRIS_PHOSPHOR;
  s.sclera = 0x020802;
  s.scleraDetail = 0x0A200C;
  s.mottle = 60;
  s.irisA = 0x40FF70;
  s.irisB = 0x08300F;
  s.pupilColor = 0x70FF98;
  s.pupilSize = 0.22f;
  s.overlays = OVL_PULSE;
  s.overlayColor = 0x40FF70;
  s.glowColor = 0x20FF40;
  s.glow = 60;
  s.emissive = 255;
  s.highlight = 60;
  s.lidEdge = 0x20A040;
  s.fx = FX_SCANLINES | FX_FLICKER | FX_ROLLBAR;
  s.seed = 56;
}

void glitchCore(EyeStyle& s) {
  machine(s);
  s.name = "GLITCH CORE";
  s.pattern = IRIS_CIRCUIT;
  s.irisA = 0xFF8A10;
  s.irisB = 0xFF5000;
  s.pupil = PUPIL_SQUARE;
  s.pupilColor = 0x40F0FF;
  s.pupilSize = 0.17f;
  s.overlays = OVL_SEG_RING;
  s.overlayColor = 0x40F0FF;
  s.glowColor = 0xFF6000;
  s.glow = 50;
  s.fx = FX_GLITCH | FX_SCANLINES;
  s.seed = 57;
}

void retroBit(EyeStyle& s) {
  human(s);
  s.name = "RETRO BIT";
  s.irisA = 0x6A9A40;
  s.irisB = 0x2A5A20;
  s.irisC = 0xA0C860;
  s.veins = 0;
  s.fx = FX_PIXELATE;
  s.fxColor = 0x9BBC0F;  // classic handheld green
  s.seed = 58;
}

typedef void (*StyleFn)(EyeStyle&);

// Order here is the order used by the serial console ("l" lists them).
const StyleFn kStyles[] = {
    viper,     cyberdyne, skyBlue,    redEclipse, solarFlare,  crtTerminal,
    emerald,   targeting, panther,    hazel,      glitchCore,  possessed,
    toxic,     arcReactor, jadeCat,   nebula,     loader,      storm,
    dragon,    infiltrator, leviathan, wolf,      retroBit,    cuttlefish,
    neonCat,   goat,      basilisk,   ghost,
};

}  // namespace

int eyeStyleCount() { return (int)(sizeof(kStyles) / sizeof(kStyles[0])); }

void eyeStyleGet(int index, EyeStyle& out) {
  int n = eyeStyleCount();
  index %= n;
  if (index < 0) index += n;
  kStyles[index](out);
}
