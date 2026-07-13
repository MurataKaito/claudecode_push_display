#pragma once
#include "core/module.h"

// POST /approve?id=&title=&detail= を受けて承認画面を静止表示。priority 100（最優先）。
// スワイプ=deny / それ以外のタッチ=allow → daemonへPOST → フィードバック顔1.8秒(priority 50に自己降格)。
// 30秒無操作でタイムアウト（decision送信なし）。
class ApproveModule : public Module {
 public:
  const char* name() override { return "approve"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  bool onGesture(Gesture g, uint32_t now) override;
  void draw(uint32_t now) override;
  void onScreenLost() override { phase_ = PHASE_NONE; }
  void appendState(String& json) override;

 private:
  void postResult(const char* decision);

  struct Pending {
    volatile bool ready = false;
    String id;
    String title;
    String detail;
  };
  Pending pending_;
  Services* svc_ = nullptr;

  enum Phase { PHASE_NONE, PHASE_SHOWING, PHASE_FEEDBACK };
  Phase phase_ = PHASE_NONE;
  bool drawn_ = false;
  String id_, title_, detail_;
  String fbExpr_, fbText_;
  volatile int recv_ = 0;  // /approve を受けた回数（asyncタスクで加算）
  int shown_ = 0;          // 承認画面を出した回数
};
