#include "screen_arbiter.h"

bool ScreenArbiter::request(int id, int priority, uint32_t timeoutMs, uint32_t now, int* preempted) {
  if (preempted) *preempted = -1;
  if (ownerId_ != -1 && ownerId_ != id && priority < priority_) return false;
  if (preempted && ownerId_ != -1 && ownerId_ != id) *preempted = ownerId_;
  ownerId_ = id;
  priority_ = priority;
  deadline_ = now + timeoutMs;
  return true;
}

void ScreenArbiter::release(int id) {
  if (id == ownerId_) ownerId_ = -1;
}

int ScreenArbiter::tick(uint32_t now) {
  if (ownerId_ == -1) return -1;
  // 符号付き差分でmillis()ラップアラウンドに耐える
  if ((int32_t)(now - deadline_) < 0) return -1;
  int expired = ownerId_;
  ownerId_ = -1;
  return expired;
}
