// Battery.cpp -- see Battery.h.
#include "Battery.h"

#include "config.h"

namespace {
// Resting LiPo voltage -> state of charge. A straight 3.2-4.2 V line (the
// old sketch) reads far too high mid-discharge and then falls off a cliff.
const float kCurve[][2] = {
    {4.20f, 100}, {4.15f, 95}, {4.11f, 90}, {4.08f, 85}, {4.02f, 80},
    {3.98f, 75},  {3.95f, 70}, {3.91f, 65}, {3.87f, 60}, {3.85f, 55},
    {3.84f, 50},  {3.82f, 45}, {3.80f, 40}, {3.79f, 35}, {3.77f, 30},
    {3.75f, 25},  {3.73f, 20}, {3.71f, 15}, {3.69f, 10}, {3.61f, 5},
    {3.27f, 0},
};
}  // namespace

float Battery::sample() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(VBAT_PIN);
  // 12-bit reading of half the battery voltage against 3.3 V.
  return (sum / 16.0f) * (2.0f * 3.3f / 4095.0f);
}

void Battery::begin() {
  v_ = sample();
  next_ = millis();
}

void Battery::update(uint32_t now) {
  if ((int32_t)(now - next_) < 0) return;
  next_ = now + 250;
  v_ += (sample() - v_) * 0.1f;  // ~2.5 s time constant; OLED load varies
}

int Battery::percent() const {
  const int n = sizeof(kCurve) / sizeof(kCurve[0]);
  if (v_ >= kCurve[0][0]) return 100;
  for (int i = 1; i < n; i++) {
    if (v_ >= kCurve[i][0]) {
      float t = (v_ - kCurve[i][0]) / (kCurve[i - 1][0] - kCurve[i][0]);
      return (int)(kCurve[i][1] + t * (kCurve[i - 1][1] - kCurve[i][1]) + 0.5f);
    }
  }
  return 0;
}
