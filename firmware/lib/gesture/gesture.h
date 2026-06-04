#pragma once
#include <stdint.h>

enum Gesture { GESTURE_NONE, GESTURE_TAP, GESTURE_DOUBLETAP, GESTURE_SWIPE };

// 指が触れた瞬間 down()、離れた瞬間 up() を呼ぶ。up() がジェスチャを返す。
// 直近TAPから doubleMs 以内の2回目TAPは DOUBLETAP。移動量が swipeThresh 以上は SWIPE。
class GestureDetector {
 public:
  void down(uint32_t t, int x, int y);
  Gesture up(uint32_t t, int x, int y);

 private:
  uint32_t downT_ = 0;
  int downX_ = 0;
  int downY_ = 0;
  uint32_t lastTapT_ = 0;
  int swipeThresh_ = 55;
  uint32_t doubleMs_ = 800;   // 2回目までの猶予（緩め）
  uint32_t tapMaxMs_ = 700;   // 1タップの最長押下時間（緩め）
};
