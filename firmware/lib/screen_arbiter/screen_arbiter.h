#pragma once
#include <stdint.h>

// 画面の所有権を一元調停する。Arduino非依存（nativeテスト対象）。
// grant条件: owner不在 / 同一ownerの再request / 新priority >= 現priority（同優先度は後勝ち）。
class ScreenArbiter {
 public:
  // 画面を要求。grantならtrue。別ownerを横取りした場合 *preempted にそのidを格納（それ以外は-1）。
  bool request(int id, int priority, uint32_t timeoutMs, uint32_t now, int* preempted = nullptr);
  void release(int id);    // owner本人以外からの呼び出しは無視
  int tick(uint32_t now);  // 期限切れならそのowner idを返して所有解除。なければ-1
  int owner() const { return ownerId_; }

 private:
  int ownerId_ = -1;
  int priority_ = 0;
  uint32_t deadline_ = 0;
};
