#pragma once
#include "core/module.h"

// タップ/ダブルタップでdaemonの GET /usage を叩き、使用率を5秒表示。priority 50。
// daemon未接続・取得失敗時はフィードバック顔2.5秒。
class UsageModule : public Module {
 public:
  const char* name() override { return "usage"; }
  void setup(Services& s) override { svc_ = &s; }
  bool onGesture(Gesture g, uint32_t now) override;
  void draw(uint32_t now) override;
  void onScreenLost() override { phase_ = PHASE_NONE; }

 private:
  void fetchAndShow(uint32_t now);
  void showFeedback(const char* expr, const char* text, uint32_t now);

  Services* svc_ = nullptr;
  enum Phase { PHASE_NONE, PHASE_USAGE, PHASE_FEEDBACK };
  Phase phase_ = PHASE_NONE;
  bool drawn_ = false;
  int percent_ = 0;
  int resetMin_ = 0;
  String fbExpr_, fbText_;
};
