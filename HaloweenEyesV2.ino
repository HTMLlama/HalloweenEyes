// HaloweenEyesV2 -- animated OLED eyes for an Adafruit Feather M4 Express
// driving two 1.5" SSD1351 128x128 OLEDs, with a thumb joystick.
//
// Controls (joystick button):
//   click             next eye style ("PLEASE STANDBY" channel change)
//   double click      next mood: neutral > angry > suspicious > sleepy
//   triple click      brightness
//   hold ~1 s         sleep (eyes close, screens off) / wake
//   click while stick pushed hard left/right: wink that side's eye
// Stick: steers the eyes. Let go and they wander on their own again.
//
// Settings are in config.h. USB serial (115200) takes commands; send '?'.
#include <SPI.h>

#include "Battery.h"
#include "Controls.h"
#include "CrashGuard.h"
#include "EyeBehavior.h"
#include "EyeDisplay.h"
#include "EyeRenderer.h"
#include "EyeStyle.h"
#include "IrisBaker.h"
#include "Screens.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Hardware and engine
// ---------------------------------------------------------------------------
static EyeDisplay gDisp[2] = {EyeDisplay(OLED1_CS, OLED1_DC, OLED1_RST),
                              EyeDisplay(OLED2_CS, OLED2_DC, OLED2_RST)};
// One framebuffer per eye (one is composed while the other is transmitted),
// plus the eyeball shaded once per frame for both.
static uint16_t gFrame[2][EYE_W * EYE_H] __attribute__((aligned(4)));
static uint16_t gShared[EYE_W * EYE_H] __attribute__((aligned(4)));
static EyeAssets gAssets;
static EyeStyle gStyle;
static EyeRenderer gRenderer;
static EyeBehavior gBehavior;
static IrisBaker gBaker;
static Controls gControls;
static Battery gBattery;
static Rng gRng;

static const uint8_t kBrightness[] = BRIGHTNESS_LEVELS;
static const int8_t kPlaylistCfg[] = STYLE_PLAYLIST;
static const int kBakeRowsPerFrame = 10;

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------
enum AppState : uint8_t {
  ST_BOOT,      // boot card while the first style bakes
  ST_EYES,      // normal running
  ST_OUT,       // leaving a style (CRT collapse, or lids closing)
  ST_CARD,      // "PLEASE STANDBY" while the next style bakes
  ST_IN,        // arriving at the new style
  ST_SLEEPING,  // lids closing before the screens switch off
  ST_ASLEEP,    // screens off
};

static AppState gState = ST_BOOT;
static uint32_t gStateAt = 0;
static int gStyleIdx = -1, gNextIdx = 0;
static int8_t gPlaylist[64];
static int gPlaylistLen = 0, gPlaylistPos = 0;
static uint8_t gBrightIdx = 0;
static bool gGamma = GAMMA_CORRECT;
static bool gStats = PRINT_STATS;
static bool gBatteryDimmed = false;
static uint32_t gLastInput = 0, gBatteryCheckAt = 0;
static RowSpan gPrevSpan[2] = {{0, 127}, {0, 127}};
static bool gForceFull = true;
static bool gInReacted = false;
static bool gCrashReported = false;

// Stats
static uint32_t gFrames = 0, gStatsAt = 0, gRenderUs = 0, gWaitUs = 0, gRows = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint32_t trngSeed() {
#if defined(__SAMD51__)
  MCLK->APBCMASK.reg |= MCLK_APBCMASK_TRNG;
  TRNG->CTRLA.reg = TRNG_CTRLA_ENABLE;
  while (!(TRNG->INTFLAG.reg & TRNG_INTFLAG_DATARDY)) {
  }
  uint32_t v = TRNG->DATA.reg;
  TRNG->CTRLA.reg = 0;
  return v ^ micros();
#else
  return (uint32_t)analogRead(A2) * 2654435761u ^ micros();
#endif
}

static void setState(AppState s) {
  gState = s;
  gStateAt = millis();
}

static void finishAll() {
  gDisp[0].finish();
  gDisp[1].finish();
}

static void applyBrightness() {
  finishAll();
  uint8_t lvl = kBrightness[gBrightIdx];
  if (gBatteryDimmed && lvl > 5) lvl = 5;
  gDisp[0].setContrast(lvl);
  gDisp[1].setContrast(lvl);
}

static void shufflePlaylist() {
  for (int i = gPlaylistLen - 1; i > 0; i--) {
    int j = (int)(gRng.next() % (uint32_t)(i + 1));
    int8_t t = gPlaylist[i];
    gPlaylist[i] = gPlaylist[j];
    gPlaylist[j] = t;
  }
}

static void buildPlaylist() {
  const int n = eyeStyleCount();
  gPlaylistLen = 0;
  if (kPlaylistCfg[0] < 0) {
    for (int i = 0; i < n && i < 64; i++) gPlaylist[gPlaylistLen++] = (int8_t)i;
  } else {
    for (unsigned i = 0; i < sizeof(kPlaylistCfg); i++)
      if (kPlaylistCfg[i] >= 0 && kPlaylistCfg[i] < n && gPlaylistLen < 64)
        gPlaylist[gPlaylistLen++] = kPlaylistCfg[i];
  }
  if (!gPlaylistLen) gPlaylist[gPlaylistLen++] = 0;
  shufflePlaylist();
  gPlaylistPos = 0;
}

// Shuffled, no repeats until every style has shown, and never the same style
// twice in a row.
static int nextFromPlaylist() {
  for (int tries = 0; tries < 2; tries++) {
    if (gPlaylistPos >= gPlaylistLen) {
      int last = gPlaylist[gPlaylistLen - 1];
      shufflePlaylist();
      if (gPlaylistLen > 1 && gPlaylist[0] == last) {
        int8_t t = gPlaylist[0];
        gPlaylist[0] = gPlaylist[1];
        gPlaylist[1] = t;
      }
      gPlaylistPos = 0;
    }
    int v = gPlaylist[gPlaylistPos++];
    if (v != gStyleIdx || gPlaylistLen == 1) return v;
  }
  return gPlaylist[0];
}

static void announceStyle(int idx) {
  if (!Serial) return;
  EyeStyle s;
  eyeStyleGet(idx, s);
  Serial.print("Style ");
  Serial.print(idx);
  Serial.print(": ");
  Serial.println(s.name);
}

static void requestStyle(int idx) {
  if (gState != ST_EYES) return;
  gNextIdx = idx;
  announceStyle(idx);
#if !TRANSITION_TV
  gBehavior.closeTo(1.0f, 0.1f);
#endif
  setState(ST_OUT);
}

// Makes the freshly baked gStyle current.
static void activateStyle() {
  gStyleIdx = gNextIdx;
  gRenderer.setStyle(&gStyle, &gAssets);
  gBehavior.setStyle(gStyle);
}

static void beginBake(int idx) {
  eyeStyleGet(idx, gStyle);
  gBaker.begin(gStyle, gAssets);
}

static void goToSleep() {
  gBehavior.closeTo(1.0f, 1.1f);
  setState(ST_SLEEPING);
}

static void wake() {
  if (gState == ST_ASLEEP) {
    finishAll();
    gDisp[0].setEnabled(true);
    gDisp[1].setEnabled(true);
    gForceFull = true;
    gBehavior.setClosure(1.0f);
  }
  gBehavior.closeTo(0.0f, 1.3f);
  setState(ST_EYES);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static void handleEvents(uint32_t now) {
  Controls::Event ev;
  while ((ev = gControls.poll()) != Controls::EV_NONE) {
    gLastInput = now;
    if (gState == ST_ASLEEP || gState == ST_SLEEPING) {
      wake();
      continue;
    }
    if (gState != ST_EYES) continue;  // ignore presses mid-transition
    switch (ev) {
      case Controls::EV_CLICK:
        requestStyle(nextFromPlaylist());
        break;
      case Controls::EV_DOUBLE: {
        Mood m = (Mood)((gBehavior.mood() + 1) % MOOD_COUNT);
        gBehavior.setMood(m);
        if (Serial) {
          Serial.print("Mood: ");
          Serial.println(moodName(m));
        }
        break;
      }
      case Controls::EV_TRIPLE:
        gBrightIdx = (gBrightIdx + 1) % sizeof(kBrightness);
        applyBrightness();
        break;
      case Controls::EV_LONG:
        goToSleep();
        break;
      // The eye on the viewer's left is the wearer's right eye (display 2).
      case Controls::EV_WINK_LEFT:
        gBehavior.wink(1);
        break;
      case Controls::EV_WINK_RIGHT:
        gBehavior.wink(0);
        break;
      default:
        break;
    }
  }
}

static void printHelp() {
  Serial.println(F("HaloweenEyesV2 commands:"));
  Serial.println(F("  n / p   next / previous style      <number><enter>  jump to style"));
  Serial.println(F("  l       list styles                m  next mood"));
  Serial.println(F("  k       blink    w  wink    x  startle    s  sleep / wake"));
  Serial.println(F("  b       brightness    g  gamma on/off    f  fps stats on/off"));
  Serial.println(F("  j       joystick readings          d  dump eye 1 frame (binary)"));
  Serial.println(F("  B       benchmark every style"));
#if defined(__SAMD51__)
  Serial.print(F("  (cache "));
  Serial.print(CMCC->SR.reg & 1 ? F("on") : F("OFF"));
  Serial.print(F(", "));
  Serial.print(F_CPU / 1000000);
  Serial.println(F(" MHz)"));
#endif
}

// Times every style's texture bake and eye render (no display output), then
// restores the current style. Handy when adding or tuning styles.
static void benchmark() {
  finishAll();
  Serial.println(F("style             us/frame (both eyes)   bake ms"));
  EyePose pose;
  memset(&pose, 0, sizeof(pose));
  pose.yaw = 0.15f;
  pose.pitch = 0.08f;
  for (int i = 0; i < eyeStyleCount(); i++) {
    crashGuardFeed();
    eyeStyleGet(i, gStyle);
    uint32_t b0 = micros();
    IrisBaker::bakeAll(gStyle, gAssets);
    uint32_t bakeUs = micros() - b0;
    gRenderer.setStyle(&gStyle, &gAssets);
    pose.pupil = gStyle.pupilSize;
    RowSpan sp;
    uint32_t shadeUs = 0, composeUs = 0;
    for (int k = 0; k < 8; k++) {
      uint32_t a = micros();
      gRenderer.renderShared(pose, pose, 1000 + k * 33, gShared);
      uint32_t b = micros();
      gRenderer.composeEye(0, 1000 + k * 33, gShared, gFrame[0], &sp);
      gRenderer.composeEye(1, 1000 + k * 33, gShared, gFrame[1], &sp);
      composeUs += micros() - b;
      shadeUs += b - a;
    }
    uint32_t us = (shadeUs + composeUs) / 8;
    char line[120];
    snprintf(line, sizeof(line), "%2d %-16s %8lu (shade %5lu, 2x compose %5lu) %6lu", i,
             gStyle.name, (unsigned long)us, (unsigned long)(shadeUs / 8),
             (unsigned long)(composeUs / 8), (unsigned long)(bakeUs / 1000));
    Serial.print(line);
#if defined(EYE_PROFILE)
    extern uint32_t gEyeProf[6];
    snprintf(line, sizeof(line), "   kcyc: lids %lu tables %lu shade %lu hilite %lu",
             (unsigned long)(gEyeProf[1] - gEyeProf[0]) / 1000, (unsigned long)(gEyeProf[2] - gEyeProf[1]) / 1000,
             (unsigned long)(gEyeProf[3] - gEyeProf[2]) / 1000, (unsigned long)(gEyeProf[4] - gEyeProf[3]) / 1000);
    Serial.print(line);
#endif
    Serial.println();
  }
  gNextIdx = gStyleIdx;
  eyeStyleGet(gStyleIdx, gStyle);
  IrisBaker::bakeAll(gStyle, gAssets);
  activateStyle();
  gForceFull = true;
}

static void handleSerial(uint32_t now) {
  static char num[6];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c >= '0' && c <= '9') {
      if (n < sizeof(num) - 1) num[n++] = c;
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (n) {
        num[n] = 0;
        n = 0;
        requestStyle(atoi(num) % eyeStyleCount());
      }
      continue;
    }
    n = 0;
    gLastInput = now;
    switch (c) {
      case 'n': requestStyle(nextFromPlaylist()); break;
      case 'p': requestStyle((gStyleIdx + eyeStyleCount() - 1) % eyeStyleCount()); break;
      case 'm': {
        Mood m = (Mood)((gBehavior.mood() + 1) % MOOD_COUNT);
        gBehavior.setMood(m);
        Serial.print("Mood: ");
        Serial.println(moodName(m));
        break;
      }
      case 'k': gBehavior.blinkNow(); break;
      case 'w': gBehavior.wink(0); break;
      case 'x': gBehavior.startle(); break;
      case 's':
        if (gState == ST_ASLEEP || gState == ST_SLEEPING) wake();
        else if (gState == ST_EYES) goToSleep();
        break;
      case 'b':
        gBrightIdx = (gBrightIdx + 1) % sizeof(kBrightness);
        applyBrightness();
        Serial.print("Brightness: ");
        Serial.println(kBrightness[gBrightIdx]);
        break;
      case 'g':
        gGamma = !gGamma;
        finishAll();
        gDisp[0].setGamma(gGamma);
        gDisp[1].setGamma(gGamma);
        Serial.println(gGamma ? "Gamma: on" : "Gamma: off (linear)");
        break;
      case 'f':
        gStats = !gStats;
        gFrames = gRenderUs = gWaitUs = gRows = 0;
        gStatsAt = now;
        break;
      case 'l':
        for (int i = 0; i < eyeStyleCount(); i++) {
          EyeStyle s;
          eyeStyleGet(i, s);
          Serial.print(i);
          Serial.print(i == gStyleIdx ? " * " : "   ");
          Serial.println(s.name);
        }
        break;
      case 'j':
        Serial.print("Joystick ");
        Serial.print(gControls.present() ? "present" : "NOT detected");
        Serial.print("  raw ");
        Serial.print(gControls.rawX());
        Serial.print(",");
        Serial.print(gControls.rawY());
        Serial.print("  x ");
        Serial.print(gControls.x(), 2);
        Serial.print(" y ");
        Serial.print(gControls.y(), 2);
        Serial.print(gControls.pressed() ? "  button DOWN" : "  button up");
        Serial.println();
        break;
      case 'd':
        finishAll();
        Serial.println("FRAME 128 128");
        // The USB stack rejects very large writes, so send it in pieces.
        for (size_t i = 0; i < sizeof(gFrame[0]); i += 512) {
          Serial.write((const uint8_t*)gFrame[0] + i, 512);
          crashGuardFeed();
        }
        Serial.println();
        Serial.println("END");
        break;
      case 'B':
        if (gState == ST_EYES) benchmark();
        break;
      case '?': printHelp(); break;
      default: break;
    }
  }
}

// ---------------------------------------------------------------------------
// Frame composition
// ---------------------------------------------------------------------------
static void standbyCard(uint16_t* fb, int eye, uint32_t now, bool boot) {
  StandbyInfo info;
  EyeStyle next;
  eyeStyleGet(gNextIdx, next);
  int pct = gBattery.percent();
  info.line1 = boot ? "OPTICS" : "PLEASE";
  info.line2 = boot ? "ONLINE" : "STANDBY";
  info.detail = next.name;
  info.channel = gNextIdx + 1;
  info.batteryPct = pct;
  info.batteryV = gBattery.volts();
  info.lowBattery = pct <= LOW_BATTERY_PCT;
  screenStandby(fb, info, now, eye ? 0x9E3779B9u : 0x7F4A7C15u);
}

// True when this frame shows live eyes on at least part of the screen.
static bool eyesVisible(uint32_t now) {
  const uint32_t t = now - gStateAt;
  switch (gState) {
    case ST_BOOT:
    case ST_CARD:
      return false;
#if TRANSITION_TV
    case ST_OUT:
      return t < 180;
#endif
    case ST_IN:
      return t >= 200;
    default:
      return true;
  }
}

// Draws eye 'e' for the current state into gFrame[e]. Returns false when only
// the rows in 'span' changed (normal eyes), true for full-screen content.
static bool drawEye(int e, uint32_t now, RowSpan& span) {
  uint16_t* fb = gFrame[e];
  const uint32_t t = now - gStateAt;
  switch (gState) {
    case ST_BOOT:
    case ST_CARD:
      standbyCard(fb, e, now, gState == ST_BOOT);
      if (t < 160) crtSqueeze(fb, easeOutQuad(t / 160.0f));  // CRT warming up
      return true;

    case ST_OUT:
#if TRANSITION_TV
      if (t < 180) {
        gRenderer.composeEye(e, now, gShared, fb, &span);
        crtSqueeze(fb, 1.0f - easeInQuad(t / 180.0f));
      } else if (t < 290) {
        crtLine(fb, 2.0f + 126.0f * (1.0f - easeInQuad((t - 180) / 110.0f)), 1.0f);
      } else {
        crtLine(fb, 2.0f, maxf(0.0f, 1.0f - (t - 290) / 60.0f));
      }
      return true;
#else
      break;  // lids close over normal rendering
#endif

    case ST_IN:
      if (t < 120) {
        standbyCard(fb, e, now, false);
        crtSqueeze(fb, 1.0f - easeInQuad(t / 120.0f));
      } else if (t < 200) {
        crtLine(fb, 2.0f + 126.0f * (1.0f - (t - 120) / 80.0f), 1.0f);
      } else {
        gRenderer.composeEye(e, now, gShared, fb, &span);
        crtSqueeze(fb, easeOutQuad(minf(1.0f, (t - 200) / 170.0f)));
      }
      return true;

    default:
      break;
  }
  gRenderer.composeEye(e, now, gShared, fb, &span);
  return false;
}

static void drawFrame(uint32_t now) {
  // The expensive part: shade the eyeball once for both eyes. Neither
  // framebuffer is touched, so the previous frame can still be transmitting.
  uint32_t t0 = micros();
  if (eyesVisible(now)) gRenderer.renderShared(gBehavior.pose(0), gBehavior.pose(1), now, gShared);
  gRenderUs += micros() - t0;

  for (int e = 0; e < 2; e++) {
    t0 = micros();
    RowSpan span = {0, EYE_H - 1};
    bool full = drawEye(e, now, span);
    uint32_t t1 = micros();
    gDisp[e ^ 1].finish();  // the bus must be free before this eye can go
    uint32_t t2 = micros();

    RowSpan send;
    if (full || gForceFull) {
      send.first = 0;
      send.last = EYE_H - 1;
    } else {
      send.first = min(span.first, gPrevSpan[e].first);
      send.last = max(span.last, gPrevSpan[e].last);
    }
    if (full) {
      gPrevSpan[e].first = 0;
      gPrevSpan[e].last = EYE_H - 1;
    } else {
      gPrevSpan[e] = span;
    }
    if (send.first <= send.last) {
      gDisp[e].pushAsync(gFrame[e], send.first, send.last);
      gRows += send.last - send.first + 1;
    }
    gRenderUs += t1 - t0;
    gWaitUs += t2 - t1;
  }
  gForceFull = false;
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
static void advanceState(uint32_t now) {
  const uint32_t t = now - gStateAt;
  switch (gState) {
    case ST_BOOT:
      gBaker.step(kBakeRowsPerFrame);
      if (t > 1400 && gBaker.done()) {
        activateStyle();
        setState(ST_IN);
      }
      break;

    case ST_OUT:
#if TRANSITION_TV
      if (t >= 350) {
        beginBake(gNextIdx);
        setState(ST_CARD);
      }
#else
      if (gBehavior.closure() >= 0.999f) {
        eyeStyleGet(gNextIdx, gStyle);
        IrisBaker::bakeAll(gStyle, gAssets);
        activateStyle();
        gBehavior.closeTo(0.0f, 0.22f);
        gForceFull = true;
        setState(ST_EYES);
      }
#endif
      break;

    case ST_CARD:
      gBaker.step(kBakeRowsPerFrame);
      if (t > 1000 && gBaker.done()) {
        activateStyle();
        setState(ST_IN);
      }
      break;

    case ST_IN:
      // The new eye pops on wide-eyed, then settles.
      if (t >= 200 && !gInReacted) {
        gInReacted = true;
        gBehavior.startle();
      }
      if (t >= 380) {
        gForceFull = true;
        setState(ST_EYES);
      }
      break;

    case ST_SLEEPING:
      if (gBehavior.closure() >= 0.999f && t > 1300) {
        finishAll();
        gDisp[0].setEnabled(false);
        gDisp[1].setEnabled(false);
        setState(ST_ASLEEP);
      }
      break;

    default:
      break;
  }
}

static void checkBattery(uint32_t now) {
  if ((int32_t)(now - gBatteryCheckAt) < 0) return;
  gBatteryCheckAt = now + 5000;
  bool critical = gBattery.percent() <= CRITICAL_BATTERY_PCT;
  if (critical && !gBatteryDimmed) {
    gBatteryDimmed = true;
    gBehavior.setMood(MOOD_SLEEPY);  // the eyes get visibly tired
    applyBrightness();
  } else if (!critical && gBatteryDimmed && gBattery.percent() > CRITICAL_BATTERY_PCT + 5) {
    gBatteryDimmed = false;
    applyBrightness();
  }
}

static void printStats(uint32_t now) {
  if (!gStats || now - gStatsAt < 2000) return;
  if (Serial && gFrames) {
    float secs = (now - gStatsAt) * 0.001f;
    Serial.print("fps ");
    Serial.print(gFrames / secs, 1);
    Serial.print("  render ");
    Serial.print(gRenderUs / 1000.0f / gFrames, 2);
    Serial.print(" ms/frame  bus wait ");
    Serial.print(gWaitUs / 1000.0f / gFrames, 2);
    Serial.print(" ms  rows/eye ");
    Serial.print(gRows / (2.0f * gFrames), 0);
    Serial.print("  batt ");
    Serial.print(gBattery.volts(), 2);
    Serial.print("V ");
    Serial.print(gBattery.percent());
    Serial.print("%  ");
    Serial.println(gStyleIdx >= 0 ? gStyle.name : "-");
  }
  gFrames = gRenderUs = gWaitUs = gRows = 0;
  gStatsAt = now;
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  crashGuardBegin();
  Serial.begin(SERIAL_BAUD);
  analogReadResolution(12);
  gRng.seed(trngSeed());

  gControls.begin();
  gBattery.begin();
  crashGuardFeed();

  gDisp[0].begin(SPI_HZ, EYE1_ROTATION);
  crashGuardFeed();
  gDisp[1].begin(SPI_HZ, EYE2_ROTATION);
  crashGuardFeed();
  for (int e = 0; e < 2; e++) gDisp[e].setGamma(gGamma);
  applyBrightness();

  gRenderer.setEye(0, EYE1_NASAL_ON_LEFT, EYE1_OFFSET_X, EYE1_OFFSET_Y);
  gRenderer.setEye(1, EYE2_NASAL_ON_LEFT, EYE2_OFFSET_X, EYE2_OFFSET_Y);

  buildPlaylist();
  gNextIdx = BOOT_STYLE >= 0 ? BOOT_STYLE % eyeStyleCount() : nextFromPlaylist();
  beginBake(gNextIdx);
  gBehavior.begin(gRng.next(), millis());
  gLastInput = millis();
  gStatsAt = millis();
  setState(ST_BOOT);
}

void loop() {
  crashGuardFeed();
  const uint32_t now = millis();
  if (!gCrashReported && Serial) {
    crashGuardReport(Serial);
    gCrashReported = true;
  }
  gControls.update(now);
  gBattery.update(now);
  handleSerial(now);
  handleEvents(now);
  checkBattery(now);

#if AUTO_CYCLE_MINUTES > 0
  if (gState == ST_EYES && now - gLastInput > AUTO_CYCLE_MINUTES * 60000UL) {
    gLastInput = now;
    requestStyle(nextFromPlaylist());
  }
#endif

  if (gState == ST_ASLEEP) {
    delay(15);  // screens are off; just watch for the button
    return;
  }

  BehaviorInput in;
  in.joyX = gControls.x();
  in.joyY = gControls.y();
  in.joyActive = gControls.active();
  gBehavior.update(now, in);
  AppState before = gState;
  advanceState(now);
  if (gState == ST_IN && before != ST_IN) {
    gBehavior.setClosure(0.0f);
    gInReacted = false;
  }
  drawFrame(now);
  gFrames++;
  printStats(now);
}
