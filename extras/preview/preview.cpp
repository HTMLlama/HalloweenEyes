// Desktop preview for the eye engine. Compiles the same renderer, styles,
// baker and behaviour code that runs on the Feather and writes PPM frames.
//
//   ./preview sheet <outdir>                 one pose per style
//   ./preview poses <outdir> <style>         a set of poses for one style
//   ./preview anim <outdir> <style> <frames> [fps]   behaviour-driven animation
//
// Build with extras/preview/build.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EyeBehavior.h"
#include "EyeRenderer.h"
#include "EyeStyle.h"
#include "IrisBaker.h"
#include "Screens.h"

static EyeAssets gAssets;
static uint16_t gFb[2][EYE_W * EYE_H];
static uint16_t gShared[EYE_W * EYE_H];
static EyeRenderer gRenderer;
static EyeStyle gStyle;

// Writes both eyes side by side as an onlooker sees them: the wearer's right
// eye (eye 1) on the left of the image, the wearer's left eye (eye 0) on the
// right.
static void writePair(const char* path) {
  const int gap = 16, W = EYE_W * 2 + gap, H = EYE_H;
  FILE* f = fopen(path, "wb");
  if (!f) {
    perror(path);
    exit(1);
  }
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int r = 0, g = 0, b = 0;
      const uint16_t* src = nullptr;
      int sx = 0;
      if (x < EYE_W) {
        src = gFb[1];
        sx = x;
      } else if (x >= EYE_W + gap) {
        src = gFb[0];
        sx = x - EYE_W - gap;
      }
      if (src) unpack565(swap16(src[y * EYE_W + sx]), r, g, b);
      unsigned char px[3] = {(unsigned char)r, (unsigned char)g, (unsigned char)b};
      fwrite(px, 1, 3, f);
    }
  }
  fclose(f);
}

static void loadStyle(int idx) {
  eyeStyleGet(idx, gStyle);
  IrisBaker::bakeAll(gStyle, gAssets);
  gRenderer.setStyle(&gStyle, &gAssets);
  gRenderer.setEye(0, true, 0, 0);
  gRenderer.setEye(1, false, 0, 0);
}

static EyePose basePose() {
  EyePose p;
  memset(&p, 0, sizeof(p));
  p.pupil = gStyle.pupilSize;
  return p;
}

static void renderPair(const EyePose& p0, const EyePose& p1, uint32_t t) {
  RowSpan sp;
  gRenderer.renderShared(p0, p1, t, gShared);
  gRenderer.composeEye(0, t, gShared, gFb[0], &sp);
  gRenderer.composeEye(1, t, gShared, gFb[1], &sp);
}
static void renderPair(const EyePose& p, uint32_t t) { renderPair(p, p, t); }

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: preview sheet|poses|anim <outdir> ...\n");
    return 1;
  }
  const char* mode = argv[1];
  const char* out = argv[2];
  char path[512];

  if (!strcmp(mode, "fuzz")) {
    // Random styles, poses, offsets and times, to shake out rare bugs
    // (build with -fsanitize=address,undefined).
    int iters = argc >= 4 ? atoi(argv[3]) : 2000;
    Rng r(777);
    int style = -1;
    for (int i = 0; i < iters; i++) {
      if (style < 0 || r.chance(0.05f)) {
        style = r.irange(0, eyeStyleCount() - 1);
        loadStyle(style);
        gRenderer.setEye(0, true, r.irange(-6, 6), r.irange(-6, 6));
        gRenderer.setEye(1, false, r.irange(-6, 6), r.irange(-6, 6));
      }
      EyePose p[2];
      for (int e = 0; e < 2; e++) {
        memset(&p[e], 0, sizeof(EyePose));
        p[e].yaw = r.range(-0.6f, 0.6f);
        p[e].pitch = r.range(-0.5f, 0.5f);
        p[e].pupil = r.range(-0.1f, 1.1f);
        p[e].blink = r.chance(0.3f) ? r.range(0.0f, 1.2f) : 0.0f;
        p[e].nictitate = r.chance(0.2f) ? r.range(0.0f, 1.0f) : 0.0f;
        p[e].mood.squint = r.range(0.0f, 1.0f);
        p[e].mood.anger = r.range(0.0f, 1.0f);
        p[e].mood.droop = r.range(0.0f, 1.0f);
        p[e].mood.widen = r.range(0.0f, 1.0f);
      }
      p[1].yaw = p[0].yaw;
      p[1].pitch = p[0].pitch;
      p[1].pupil = p[0].pupil;
      renderPair(p[0], p[1], (uint32_t)r.irange(0, 3600000));
    }
    printf("fuzz ok (%d frames)\n", iters);
    return 0;
  }

  if (!strcmp(mode, "screens")) {
    // Standby card, boot card and CRT effects.
    StandbyInfo info;
    info.line1 = "PLEASE";
    info.line2 = "STANDBY";
    info.detail = "SOLAR FLARE";
    info.channel = 5;
    info.batteryPct = 87;
    info.batteryV = 4.02f;
    info.lowBattery = false;
    screenStandby(gFb[1], info, 1000, 0x7F4A7C15u);
    info.batteryPct = 9;
    info.batteryV = 3.68f;
    info.lowBattery = true;
    screenStandby(gFb[0], info, 1200, 0x9E3779B9u);
    snprintf(path, sizeof(path), "%s/screen_standby.ppm", out);
    writePair(path);
    info.line1 = "OPTICS";
    info.line2 = "ONLINE";
    info.detail = "VIPER";
    info.batteryPct = 100;
    info.batteryV = 4.2f;
    info.lowBattery = false;
    screenStandby(gFb[1], info, 400, 0x7F4A7C15u);
    loadStyle(4);
    EyePose p = basePose();
    renderPair(p, 1000);
    crtSqueeze(gFb[0], 0.35f);
    snprintf(path, sizeof(path), "%s/screen_boot_squeeze.ppm", out);
    writePair(path);
    crtLine(gFb[1], 60.0f, 1.0f);
    crtLine(gFb[0], 6.0f, 0.8f);
    snprintf(path, sizeof(path), "%s/screen_line.ppm", out);
    writePair(path);
    return 0;
  }

  if (!strcmp(mode, "sheet")) {
    for (int i = 0; i < eyeStyleCount(); i++) {
      loadStyle(i);
      EyePose p = basePose();
      p.yaw = 0.12f;
      p.pitch = 0.05f;
      renderPair(p, 1234);
      snprintf(path, sizeof(path), "%s/sheet_%02d.ppm", out, i);
      writePair(path);
      printf("%d %s\n", i, gStyle.name);
    }
    return 0;
  }

  if (!strcmp(mode, "poses") && argc >= 4) {
    loadStyle(atoi(argv[3]));
    struct {
      float yaw, pitch, blink, pupilK;
      LidPose mood;
    } poses[] = {
        {0.0f, 0.0f, 0.0f, 1.0f, {0, 0, 0, 0}},
        {0.42f, 0.25f, 0.0f, 1.0f, {0, 0, 0, 0}},
        {-0.42f, -0.28f, 0.0f, 1.0f, {0, 0, 0, 0}},
        {0.0f, 0.0f, 0.55f, 1.0f, {0, 0, 0, 0}},
        {0.1f, -0.05f, 0.0f, 0.7f, {0.35f, 0.9f, 0, 0}},   // angry
        {-0.3f, 0.0f, 0.0f, 1.0f, {0.75f, 0.15f, 0, 0}},   // suspicious
        {0.0f, -0.1f, 0.0f, 1.3f, {0.1f, 0, 0.62f, 0}},    // sleepy
        {0.0f, 0.05f, 0.0f, 0.6f, {0, 0, 0, 1.0f}},        // surprised
    };
    for (unsigned k = 0; k < sizeof(poses) / sizeof(poses[0]); k++) {
      EyePose p = basePose();
      p.yaw = poses[k].yaw;
      p.pitch = poses[k].pitch;
      p.blink = poses[k].blink;
      p.pupil = gStyle.pupilSize * poses[k].pupilK;
      p.mood = poses[k].mood;
      renderPair(p, 1000 + k * 250);
      snprintf(path, sizeof(path), "%s/pose_%02d.ppm", out, k);
      writePair(path);
    }
    return 0;
  }

  if (!strcmp(mode, "anim") && argc >= 5) {
    int idx = atoi(argv[3]);
    int frames = atoi(argv[4]);
    int fps = argc >= 6 ? atoi(argv[5]) : 30;
    loadStyle(idx);
    EyeBehavior beh;
    beh.begin(12345u, 0);
    beh.setStyle(gStyle);
    BehaviorInput in;
    memset(&in, 0, sizeof(in));
    for (int f = 0; f < frames; f++) {
      uint32_t t = (uint32_t)(f * 1000 / fps);
      beh.update(t, in);
      renderPair(beh.pose(0), beh.pose(1), t);
      snprintf(path, sizeof(path), "%s/anim_%04d.ppm", out, f);
      writePair(path);
    }
    return 0;
  }

  fprintf(stderr, "bad arguments\n");
  return 1;
}
