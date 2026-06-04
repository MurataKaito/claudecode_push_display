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
extern String g_daemonBase;  // 例: "http://172.20.10.5:4920"（heartbeatで学習）

struct PendingApprove {
  volatile bool ready = false;  // 表示すべき承認あり
  String id;
  String title;
  String detail;
};
extern PendingApprove g_approve;
extern volatile int g_approveRecv;   // /approve を受けた回数
extern volatile int g_approveShown;  // main が承認画面を出した回数
extern volatile int g_touchReleases; // タッチ離しを検出した回数
extern volatile int g_baseState;     // ベース状態 0=idle / 1=working
