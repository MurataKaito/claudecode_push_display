#pragma once
#include <Arduino.h>

// async受信(TCPタスク)→mainループ受け渡し用の共有状態。
struct PendingNotify {
  volatile bool ready = false;  // mainが処理すべき通知あり
  String expr;
  String text;
  uint8_t* wav = nullptr;       // PSRAM上のWAVバッファ
  size_t wavLen = 0;
};

extern PendingNotify g_notify;
