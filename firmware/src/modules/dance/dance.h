#pragma once
#include "core/module.h"

// POST /dance?clip=&ms= を受けてダンス演出を再生。priority 40（notify=50/approve=100の下）。
class DanceModule : public Module {
 public:
  const char* name() override { return "dance"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  void draw(uint32_t now) override;
  void onScreenLost() override;  // 横取り/演出終了時、自分が張った強制クリップを畳む

 private:
  struct Pending {
    volatile bool ready = false;  // asyncタスク→loop()受け渡し。最後に立てる
    String clip;                  // 空なら "done" プールからランダム
    uint32_t ms = 6000;
  };
  Pending pending_;
  Services* svc_ = nullptr;
  bool forced_ = false;  // clip指定でdisplayForceClipを張ったか（解除の対象判定用）
};
