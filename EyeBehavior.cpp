// EyeBehavior.cpp -- see EyeBehavior.h.
#include "EyeBehavior.h"

namespace {

// How long the stick must rest before the eyes wander on their own again.
const uint32_t kHandBackMs = 1500;

struct MoodTraits {
  LidPose lids;
  float pupil;      // pupil size multiplier
  float blinkGap;   // blink interval multiplier
  float hold;       // fixation duration multiplier
  float range;      // saccade amplitude multiplier
  float speed;      // saccade duration multiplier (bigger = slower)
};

const MoodTraits kMood[MOOD_COUNT] = {
    // lids {squint, anger, droop, widen}  pupil blink hold range speed
    {{0.0f, 0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},     // neutral
    {{0.35f, 0.9f, 0.0f, 0.0f}, 0.72f, 1.7f, 1.8f, 0.6f, 0.9f},   // angry
    {{0.75f, 0.15f, 0.0f, 0.0f}, 0.85f, 1.0f, 0.55f, 1.1f, 0.9f}, // suspicious
    {{0.1f, 0.0f, 0.62f, 0.0f}, 1.25f, 0.9f, 1.5f, 0.45f, 1.8f},  // sleepy
};

inline bool reached(uint32_t now, uint32_t when) { return (int32_t)(now - when) >= 0; }

// Critically damped spring toward 'target', advanced by the exact solution
// for dt. Stepping it per frame (Euler) goes unstable once dt approaches 1/w,
// i.e. below ~40 fps, and flings the eyes back and forth.
inline void springStep(float& x, float& v, float target, float w, float dt) {
  const float d = x - target;
  const float c = v + w * d;
  const float e = expf(-w * dt);
  x = target + (d + c * dt) * e;
  v = (v - w * c * dt) * e;
}

}  // namespace

const char* moodName(Mood m) {
  switch (m) {
    case MOOD_ANGRY: return "ANGRY";
    case MOOD_SUSPICIOUS: return "SUSPICIOUS";
    case MOOD_SLEEPY: return "SLEEPY";
    default: return "NEUTRAL";
  }
}

void EyeBehavior::begin(uint32_t seed, uint32_t now) {
  rng_.seed(seed);
  last_ = t0_ = now;
  gx_ = gy_ = vx_ = vy_ = 0.0f;
  manual_ = false;
  saccade_ = false;
  fixUntil_ = now + 600;
  nextMicro_ = now + 500;
  memset(blink_, 0, sizeof(blink_));
  nextBlink_ = now + 1500 + rng_.irange(0, 2000);
  pendingDouble_ = 0;
  nictActive_ = false;
  pPhase_ = 0;
  pNext_ = now + rng_.irange(5000, 12000);
  memset(pose_, 0, sizeof(pose_));
}

void EyeBehavior::setStyle(const EyeStyle& s) {
  const float deg = EYE_PI / 180.0f;
  maxYaw_ = s.maxGaze * deg;
  maxPitch_ = s.maxGaze * 0.72f * deg;  // eyes travel less vertically
  pupilRest_ = s.pupilSize;
  pupilMin_ = s.pupilMin;
  pupilMax_ = s.pupilMax;
  pupil_ = s.pupilSize;
  nictitates_ = (s.fx & FX_NICTITATE) != 0;
}

void EyeBehavior::closeTo(float target, float seconds) {
  closureTarget_ = clampf(target, 0.0f, 1.0f);
  float dist = fabsf(closureTarget_ - closure_);
  closureRate_ = seconds > 0.001f ? maxf(dist / seconds, 0.01f) : 1000.0f;
}

void EyeBehavior::startBlink(int eye, uint32_t now, uint16_t c, uint16_t h,
                             uint16_t o, float depth) {
  Blink& b = blink_[eye];
  b.phase = 1;
  b.start = now;
  b.closeMs = c;
  b.holdMs = h;
  b.openMs = o;
  b.depth = depth;
}

void EyeBehavior::blinkNow() {
  uint32_t now = last_;
  bool sleepy = mood_ == MOOD_SLEEPY;
  uint16_t c = sleepy ? 160 : (uint16_t)rng_.irange(60, 85);
  uint16_t h = sleepy ? 220 : (uint16_t)rng_.irange(20, 50);
  uint16_t o = sleepy ? 420 : (uint16_t)rng_.irange(130, 190);
  startBlink(0, now, c, h, o, 1.0f);
  startBlink(1, now, c, h, o, 1.0f);
  if (rng_.chance(0.12f)) pendingDouble_ = now + c + h + o + 90;
}

void EyeBehavior::wink(int eye) {
  uint32_t now = last_;
  startBlink(eye & 1, now, 90, 280, 220, 1.0f);
  startBlink((eye & 1) ^ 1, now, 90, 280, 220, 0.18f);  // the other eye squints a touch
}

void EyeBehavior::startle() {
  startleAt_ = last_;
  startled_ = true;
}

void EyeBehavior::scheduleBlink(uint32_t now) {
  float u = rng_.uniform();
  float gap = (1700.0f + 5200.0f * u * u) * kMood[mood_].blinkGap;
  nextBlink_ = now + (uint32_t)gap;
}

float EyeBehavior::blinkValue(Blink& b, uint32_t now) {
  if (b.phase == 0) return 0.0f;
  uint32_t el = now - b.start;
  if (el < b.closeMs) return b.depth * easeInQuad((float)el / b.closeMs);
  el -= b.closeMs;
  if (el < b.holdMs) return b.depth;
  el -= b.holdMs;
  if (el < b.openMs) return b.depth * (1.0f - easeOutQuad((float)el / b.openMs));
  b.phase = 0;
  return 0.0f;
}

void EyeBehavior::startSaccade(uint32_t now, float tx, float ty, float speedK) {
  // Keep targets inside the unit disc.
  float r2 = tx * tx + ty * ty;
  if (r2 > 1.0f) {
    float k = 1.0f / fsqrt(r2);
    tx *= k;
    ty *= k;
  }
  float dx = tx - gx_, dy = ty - gy_;
  float ampDeg = fsqrt(dx * dx + dy * dy) * maxYaw_ * (180.0f / EYE_PI);
  // Saccade "main sequence": duration grows with amplitude.
  sDur_ = (uint32_t)((24.0f + 2.4f * ampDeg) * speedK);
  if (sDur_ < 12) sDur_ = 12;
  sx0_ = gx_;
  sy0_ = gy_;
  sx1_ = tx;
  sy1_ = ty;
  sStart_ = now;
  saccade_ = true;
  // Big gaze shifts often come with a blink.
  if (ampDeg > 12.0f && blink_[0].phase == 0 && rng_.chance(0.3f)) blinkNow();
}

void EyeBehavior::autoGaze(uint32_t now) {
  const MoodTraits& mt = kMood[mood_];
  if (saccade_) {
    float p = (float)(now - sStart_) / (float)sDur_;
    if (p >= 1.0f) {
      saccade_ = false;
      gx_ = sx1_;
      gy_ = sy1_;
    } else {
      float e = easeMinJerk(p);
      gx_ = lerpf(sx0_, sx1_, e);
      gy_ = lerpf(sy0_, sy1_, e);
    }
    return;
  }
  if (!reached(now, fixUntil_)) return;

  float tx, ty;
  if (mood_ == MOOD_SUSPICIOUS && rng_.chance(0.75f)) {
    // Shifty side-to-side glances.
    glanceSide_ = -glanceSide_;
    tx = glanceSide_ * rng_.range(0.55f, 0.95f);
    ty = rng_.range(-0.15f, 0.1f);
  } else if (rng_.chance(mood_ == MOOD_ANGRY ? 0.65f : 0.3f)) {
    // Return to (near) straight ahead.
    tx = rng_.range(-0.15f, 0.15f);
    ty = rng_.range(-0.12f, 0.12f);
  } else {
    float ang = rng_.range(0.0f, EYE_TAU);
    float rad = fsqrt(rng_.uniform()) * 0.9f * mt.range;
    tx = cosf(ang) * rad;
    ty = sinf(ang) * rad * 0.8f;
  }
  startSaccade(now, tx, ty, mt.speed);
  float u = rng_.uniform();
  float hold = (280.0f + 2300.0f * u * u) * mt.hold;
  fixUntil_ = now + sDur_ + (uint32_t)hold;
}

float EyeBehavior::pupilEvents(uint32_t now) {
  // Occasional slow dilation or constriction, then back to rest.
  const uint32_t ramp = 650;
  switch (pPhase_) {
    case 0:
      if (reached(now, pNext_)) {
        pTarget_ = rng_.chance(0.5f) ? rng_.range(1.2f, 1.55f) : rng_.range(0.65f, 0.85f);
        pHold_ = (uint16_t)rng_.irange(800, 2200);
        pStart_ = now;
        pPhase_ = 1;
      }
      return 1.0f;
    case 1: {
      float p = (float)(now - pStart_) / ramp;
      if (p >= 1.0f) {
        pPhase_ = 2;
        pStart_ = now;
        return pTarget_;
      }
      return lerpf(1.0f, pTarget_, smoothstep(0.0f, 1.0f, p));
    }
    case 2:
      if (now - pStart_ >= pHold_) {
        pPhase_ = 3;
        pStart_ = now;
      }
      return pTarget_;
    default: {
      float p = (float)(now - pStart_) / ramp;
      if (p >= 1.0f) {
        pPhase_ = 0;
        pNext_ = now + rng_.irange(6000, 18000);
        return 1.0f;
      }
      return lerpf(pTarget_, 1.0f, smoothstep(0.0f, 1.0f, p));
    }
  }
}

void EyeBehavior::update(uint32_t now, const BehaviorInput& in) {
  float dt = (now - last_) * 0.001f;
  if (dt > 0.1f) dt = 0.1f;
  last_ = now;
  const float ts = (now - t0_) * 0.001f;
  const MoodTraits& mt = kMood[mood_];

  // --- Gaze ------------------------------------------------------------------
  if (in.joyActive) {
    manual_ = true;
    lastJoy_ = now;
  } else if (manual_ && now - lastJoy_ > kHandBackMs) {
    manual_ = false;
    saccade_ = false;
    fixUntil_ = now + rng_.irange(200, 900);
  }
  if (manual_) {
    // Follow the stick: snappy, no overshoot, smooth at any frame rate.
    const float w = 30.0f;
    springStep(gx_, vx_, in.joyX, w, dt);
    springStep(gy_, vy_, in.joyY, w, dt);
    saccade_ = false;
  } else {
    vx_ = vy_ = 0.0f;
    autoGaze(now);
  }
  // Belt and braces: never let a bad value (NaN, runaway) stick.
  if (!(fabsf(gx_) < 4.0f && fabsf(gy_) < 4.0f && fabsf(vx_) < 1000.0f && fabsf(vy_) < 1000.0f)) {
    gx_ = gy_ = vx_ = vy_ = 0.0f;
    saccade_ = false;
  }

  // Micro-saccades and slow drift keep a "still" eye alive.
  if (reached(now, nextMicro_)) {
    microTX_ = rng_.range(-0.028f, 0.028f);
    microTY_ = rng_.range(-0.022f, 0.022f);
    nextMicro_ = now + rng_.irange(350, 1500);
  }
  float mk = 1.0f - expf(-dt / 0.018f);
  microX_ += (microTX_ - microX_) * mk;
  microY_ += (microTY_ - microY_) * mk;
  float ex = gx_ + microX_ + 0.01f * sinf(ts * 0.83f) + 0.006f * sinf(ts * 2.1f + 1.0f);
  float ey = gy_ + microY_ + 0.008f * sinf(ts * 0.61f + 2.0f);
  float er2 = ex * ex + ey * ey;
  if (er2 > 1.0f) {
    float k = 1.0f / fsqrt(er2);
    ex *= k;
    ey *= k;
  }

  // --- Blinks ------------------------------------------------------------------
  if (reached(now, nextBlink_)) {
    if (nictitates_ && !nictActive_ && rng_.chance(0.4f)) {
      nictActive_ = true;
      nictStart_ = now;
    } else {
      blinkNow();
    }
    scheduleBlink(now);
  }
  if (pendingDouble_ && reached(now, pendingDouble_)) {
    pendingDouble_ = 0;
    startBlink(0, now, 60, 25, 140, 1.0f);
    startBlink(1, now, 60, 25, 140, 1.0f);
  }
  float bl[2] = {blinkValue(blink_[0], now), blinkValue(blink_[1], now)};

  float nict = 0.0f;
  if (nictActive_) {
    uint32_t el = now - nictStart_;
    if (el < 170) nict = easeOutQuad(el / 170.0f);
    else if (el < 230) nict = 1.0f;
    else if (el < 500) nict = 1.0f - easeInQuad((el - 230) / 270.0f);
    else nictActive_ = false;
  }

  // Lid override (transitions / sleep).
  if (closure_ != closureTarget_) {
    float step = closureRate_ * dt;
    if (fabsf(closureTarget_ - closure_) <= step) closure_ = closureTarget_;
    else closure_ += closure_ < closureTarget_ ? step : -step;
  }

  // --- Startle ---------------------------------------------------------------
  float startle = 0.0f;
  if (startled_) {
    float el = (now - startleAt_) * 0.001f;
    if (el > 1.6f) startled_ = false;
    else startle = el < 0.08f ? el / 0.08f : expf(-(el - 0.08f) * 2.2f);
  }

  // --- Pupil -------------------------------------------------------------------
  float hippus = 1.0f + 0.045f * (0.6f * sinf(ts * EYE_TAU * 0.31f) +
                                  0.4f * sinf(ts * EYE_TAU * 0.53f + 1.3f));
  float target = pupilRest_ * mt.pupil * hippus * pupilEvents(now) * (1.0f - 0.4f * startle);
  target = clampf(target, pupilMin_, pupilMax_);
  pupil_ += (target - pupil_) * (1.0f - expf(-dt / 0.12f));

  // --- Mood --------------------------------------------------------------------
  float k = 1.0f - expf(-dt / 0.15f);
  moodCur_.squint += (mt.lids.squint - moodCur_.squint) * k;
  moodCur_.anger += (mt.lids.anger - moodCur_.anger) * k;
  moodCur_.droop += (mt.lids.droop - moodCur_.droop) * k;
  moodCur_.widen += (mt.lids.widen - moodCur_.widen) * k;
  LidPose lids = moodCur_;
  lids.widen = maxf(lids.widen, startle);
  lids.squint *= 1.0f - startle;
  lids.droop *= 1.0f - startle;

  for (int e = 0; e < 2; e++) {
    EyePose& p = pose_[e];
    p.yaw = ex * maxYaw_;
    p.pitch = ey * maxPitch_;
    p.pupil = pupil_;
    p.blink = maxf(bl[e], closure_);
    p.nictitate = nict;
    p.mood = lids;
  }
}
