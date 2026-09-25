// EyeBehavior.h -- makes the eyes act alive.
//
// Produces an EyePose for each eye every frame from the joystick (when it is
// being used) or from an autonomous model of eye movement: saccades with
// minimum-jerk timing, fixation holds, micro-saccades and drift, natural blink
// timing (fast close, slower open, occasional double blinks, blinks that
// accompany big gaze shifts), pupil hippus and dilation events, moods, winks
// and a reptile third eyelid.
#pragma once

#include <stdint.h>

#include "EyeMath.h"
#include "EyeRenderer.h"
#include "EyeStyle.h"

enum Mood : uint8_t {
  MOOD_NEUTRAL,
  MOOD_ANGRY,
  MOOD_SUSPICIOUS,
  MOOD_SLEEPY,
  MOOD_COUNT
};

const char* moodName(Mood m);

struct BehaviorInput {
  float joyX, joyY;  // -1..1 after dead zone, +y = up
  bool joyActive;    // stick is being pushed
};

class EyeBehavior {
 public:
  void begin(uint32_t seed, uint32_t nowMs);
  void setStyle(const EyeStyle& s);
  void update(uint32_t nowMs, const BehaviorInput& in);
  const EyePose& pose(int eye) const { return pose_[eye]; }

  void setMood(Mood m) { mood_ = m; }
  Mood mood() const { return mood_; }
  bool manual() const { return manual_; }

  void blinkNow();
  void wink(int eye);
  void startle();  // lids fly open, pupils snap small, then relax

  // Lid override used for transitions and sleep: 0 = no effect, 1 = shut.
  void setClosure(float v) { closure_ = closureTarget_ = v; }
  void closeTo(float target, float seconds);
  float closure() const { return closure_; }

 private:
  struct Blink {
    uint8_t phase;  // 0 idle, 1 closing, 2 closed, 3 opening
    uint32_t start;
    uint16_t closeMs, holdMs, openMs;
    float depth;
  };

  void autoGaze(uint32_t now);
  void startSaccade(uint32_t now, float tx, float ty, float speedK);
  void scheduleBlink(uint32_t now);
  void startBlink(int eye, uint32_t now, uint16_t c, uint16_t h, uint16_t o,
                  float depth);
  float blinkValue(Blink& b, uint32_t now);
  float pupilEvents(uint32_t now);

  Rng rng_;
  uint32_t last_ = 0;
  uint32_t t0_ = 0;

  // From the style.
  float maxYaw_ = 0.45f, maxPitch_ = 0.35f;
  float pupilRest_ = 0.3f, pupilMin_ = 0.2f, pupilMax_ = 0.6f;
  bool nictitates_ = false;

  // Gaze, normalised to the unit disc.
  float gx_ = 0, gy_ = 0, vx_ = 0, vy_ = 0;
  bool manual_ = false;
  uint32_t lastJoy_ = 0;
  bool saccade_ = false;
  float sx0_ = 0, sy0_ = 0, sx1_ = 0, sy1_ = 0;
  uint32_t sStart_ = 0, sDur_ = 1, fixUntil_ = 0;
  int glanceSide_ = 1;
  float microX_ = 0, microY_ = 0, microTX_ = 0, microTY_ = 0;
  uint32_t nextMicro_ = 0;

  // Blinks.
  Blink blink_[2];
  uint32_t nextBlink_ = 0, pendingDouble_ = 0;
  uint32_t nictStart_ = 0;
  bool nictActive_ = false;

  // Pupil.
  float pupil_ = 0.3f;
  uint8_t pPhase_ = 0;
  uint32_t pStart_ = 0, pNext_ = 0;
  uint16_t pHold_ = 1000;
  float pTarget_ = 1.0f;

  // Mood, startle and lid override.
  Mood mood_ = MOOD_NEUTRAL;
  LidPose moodCur_ = {0, 0, 0, 0};
  uint32_t startleAt_ = 0;
  bool startled_ = false;
  float closure_ = 0.0f, closureTarget_ = 0.0f, closureRate_ = 1.0f;

  EyePose pose_[2];
};
