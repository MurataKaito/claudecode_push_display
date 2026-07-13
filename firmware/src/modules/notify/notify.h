#pragma once
#include "core/module.h"

// POST /notify?expr=&text= (body=WAVバイナリ) を受けて顔オーバーレイ表示+音声再生。priority 50。
class NotifyModule : public Module {
 public:
  const char* name() override { return "notify"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  void draw(uint32_t now) override;

 private:
  void onBody(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total);

  // async受信(TCPタスク)→loop()受け渡し。書き込み完了後にreadyを最後に立てる（現行と同じ作法）
  struct Pending {
    volatile bool ready = false;
    String expr;
    String text;
    uint8_t* wav = nullptr;  // PSRAM上のWAVバッファ
    size_t wavLen = 0;
  };
  Pending pending_;
  Services* svc_ = nullptr;
  String expr_ = "normal";
  String text_;
};
