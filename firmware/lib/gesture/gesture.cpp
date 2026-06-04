#include "gesture.h"
#include <stdlib.h>

void GestureDetector::down(uint32_t t, int x, int y) {
  downT_ = t;
  downX_ = x;
  downY_ = y;
}

Gesture GestureDetector::up(uint32_t t, int x, int y) {
  int dist = abs(x - downX_) + abs(y - downY_);
  uint32_t dur = t - downT_;
  if (dist >= swipeThresh_) {
    lastTapT_ = 0;
    return GESTURE_SWIPE;
  }
  if (dur <= tapMaxMs_) {
    if (lastTapT_ != 0 && (t - lastTapT_) <= doubleMs_) {
      lastTapT_ = 0;
      return GESTURE_DOUBLETAP;
    }
    lastTapT_ = t;
    return GESTURE_TAP;
  }
  lastTapT_ = 0;
  return GESTURE_NONE;
}
