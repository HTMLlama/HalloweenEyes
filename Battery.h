// Battery.h -- LiPo voltage, filtered, with a realistic charge curve.
#pragma once

#include <Arduino.h>

class Battery {
 public:
  void begin();
  void update(uint32_t nowMs);
  float volts() const { return v_; }
  int percent() const;

 private:
  float sample();
  float v_ = 0.0f;
  uint32_t next_ = 0;
};
