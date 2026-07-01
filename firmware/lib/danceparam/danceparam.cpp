#include "danceparam.h"

uint32_t clampDanceMs(long ms) {
  if (ms <= 0) return 6000;      // 未指定/不正は既定
  if (ms < 500) return 500;      // 下限
  if (ms > 30000) return 30000;  // 上限
  return (uint32_t)ms;
}
