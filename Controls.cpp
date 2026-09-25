// Controls.cpp -- see Controls.h.
#include "Controls.h"

#include "config.h"

namespace {
const uint32_t kDebounceMs = 20;
const uint32_t kLongMs = 800;
const uint32_t kMultiMs = 300;  // max gap between clicks of a double/triple
}  // namespace

int Controls::readAxis(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(pin);
  return (int)(sum >> 2);
}

void Controls::begin() {
  pinMode(JOY_BTN_PIN, INPUT_PULLUP);
#if JOYSTICK_ENABLED
  // Calibrate the centre, and check the readings look like a real stick
  // (steady, roughly mid-scale) rather than floating pins.
  long sx = 0, sy = 0;
  int mnx = 4095, mxx = 0, mny = 4095, mxy = 0;
  for (int i = 0; i < 32; i++) {
    int ax = readAxis(JOY_X_PIN), ay = readAxis(JOY_Y_PIN);
    sx += ax;
    sy += ay;
    mnx = min(mnx, ax);
    mxx = max(mxx, ax);
    mny = min(mny, ay);
    mxy = max(mxy, ay);
    delay(2);
  }
  cx_ = (int)(sx / 32);
  cy_ = (int)(sy / 32);
  bool centred = cx_ > 800 && cx_ < 3300 && cy_ > 800 && cy_ < 3300;
  bool steady = (mxx - mnx) < 250 && (mxy - mny) < 250;
  present_ = centred && steady;
#endif
  last_ = millis();
}

void Controls::push(Event e) {
  if (count_ < 4) {
    queue_[(head_ + count_) & 3] = e;
    count_++;
  }
}

Controls::Event Controls::poll() {
  if (!count_) return EV_NONE;
  Event e = queue_[head_];
  head_ = (head_ + 1) & 3;
  count_--;
  return e;
}

void Controls::update(uint32_t now) {
  float dt = (now - last_) * 0.001f;
  last_ = now;

  // --- Joystick ---------------------------------------------------------------
  if (present_) {
    rawX_ = readAxis(JOY_X_PIN);
    rawY_ = readAxis(JOY_Y_PIN);
    const float span = 2048.0f * JOY_THROW;
    float nx = (rawX_ - cx_) / span;
    float ny = -(rawY_ - cy_) / span;  // original sketch: raw up = pupil down
#if JOY_SWAP_XY
    float tmp = nx;
    nx = ny;
    ny = tmp;
#endif
#if JOY_INVERT_X
    nx = -nx;
#endif
#if JOY_INVERT_Y
    ny = -ny;
#endif
    float m = sqrtf(nx * nx + ny * ny);
    if (m < JOY_DEADZONE) {
      nx = ny = 0.0f;
      active_ = false;
    } else {
      float k = min((m - JOY_DEADZONE) / (1.0f - JOY_DEADZONE), 1.0f) / m;
      nx *= k;
      ny *= k;
      active_ = true;
    }
    float a = 1.0f - expf(-dt / 0.025f);
    x_ += (nx - x_) * a;
    y_ += (ny - y_) * a;
  }

  // --- Button -----------------------------------------------------------------
  bool raw = digitalRead(JOY_BTN_PIN) == LOW;
  if (raw != raw_) {
    raw_ = raw;
    changeAt_ = now;
  }
  if (raw_ != stable_ && now - changeAt_ >= kDebounceMs) {
    stable_ = raw_;
    if (stable_) {
      pressAt_ = now;
      longFired_ = false;
      pressX_ = x_;
    } else if (!longFired_) {
      clicks_++;
      releaseAt_ = now;
    }
  }
  if (stable_ && !longFired_ && now - pressAt_ >= kLongMs) {
    longFired_ = true;
    clicks_ = 0;
    push(EV_LONG);
  }
  if (!stable_ && clicks_ && now - releaseAt_ >= kMultiMs) {
    if (clicks_ == 1) {
#if CLICK_DEFLECTED_WINKS
      if (pressX_ < -0.7f) push(EV_WINK_LEFT);
      else if (pressX_ > 0.7f) push(EV_WINK_RIGHT);
      else push(EV_CLICK);
#else
      push(EV_CLICK);
#endif
    } else {
      push(clicks_ == 2 ? EV_DOUBLE : EV_TRIPLE);
    }
    clicks_ = 0;
  }
}
