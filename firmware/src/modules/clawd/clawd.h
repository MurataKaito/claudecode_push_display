#pragma once
#include "core/module.h"

// 背景（優先度0）に常駐し、baseStateに応じたベース顔を描く。/clip を所有し dbg を出力する。
class ClawdModule : public Module {
 public:
  const char* name() override { return "clawd"; }
  void setup(Services& s) override;         // /clip 登録
  void loop(uint32_t now) override;         // requestScreen(0,...) で背景常駐
  void draw(uint32_t now) override;         // baseStateに応じ tickFace(...)
  void appendState(String& json) override;  // dbg（displayDebug()）を出力

 private:
  Services* svc_ = nullptr;
};
