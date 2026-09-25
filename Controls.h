// Controls.h -- joystick and button.
//
// The stick is calibrated at power-up (leave it centred), gets a round dead
// zone and light smoothing, and is ignored if it looks disconnected. The
// button is debounced and turned into gestures: click, double, triple, long
// press, and click-while-pushed-sideways (wink).
#pragma once

#include <Arduino.h>

class Controls {
 public:
  enum Event : uint8_t {
    EV_NONE,
    EV_CLICK,
    EV_DOUBLE,
    EV_TRIPLE,
    EV_LONG,
    EV_WINK_LEFT,   // clicked with the stick pushed toward the panel's left
    EV_WINK_RIGHT,
  };

  void begin();
  void update(uint32_t nowMs);
  Event poll();  // next queued gesture, or EV_NONE

  float x() const { return x_; }  // -1..1, + = panel right
  float y() const { return y_; }  // -1..1, + = up
  bool active() const { return active_; }
  bool present() const { return present_; }
  bool pressed() const { return stable_; }
  int rawX() const { return rawX_; }
  int rawY() const { return rawY_; }

 private:
  void push(Event e);
  int readAxis(uint8_t pin);

  // Joystick
  bool present_ = false;
  int cx_ = 2048, cy_ = 2048, rawX_ = 0, rawY_ = 0;
  float x_ = 0, y_ = 0;
  bool active_ = false;
  uint32_t last_ = 0;

  // Button
  bool raw_ = false, stable_ = false, longFired_ = false;
  uint32_t changeAt_ = 0, pressAt_ = 0, releaseAt_ = 0;
  uint8_t clicks_ = 0;
  float pressX_ = 0;

  Event queue_[4];
  uint8_t head_ = 0, count_ = 0;
};
