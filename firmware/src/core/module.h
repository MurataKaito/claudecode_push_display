#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "gesture.h"

// モジュールに渡される共有サービス。メソッドの実体はcore(app.cpp)が持つ。
struct Services {
  AsyncWebServer* http;  // setup()内でのルート登録用
  int selfId;            // レジストリ上の自分のID（画面調停で使う）

  // 画面を要求。grantならtrue。別モジュールを横取りした場合は相手のonScreenLost()が呼ばれる。
  bool requestScreen(int priority, uint32_t timeoutMs, uint32_t now);
  void releaseScreen();

  const String& daemonBase() const;  // 例 "http://172.20.10.5:4920"。未学習なら空文字
  int baseState() const;             // 0=idle / 1=working
};

// 機能モジュールの共通インターフェース。1機能 = modules/<name>/ の1ディレクトリ。
// 追加手順: Moduleを実装し、module_registry.cpp に1行登録する。
struct Module {
  virtual ~Module() = default;
  virtual const char* name() = 0;
  virtual void setup(Services& s) {}        // 起動時1回。HTTPルート登録・初期化
  virtual void loop(uint32_t now) {}        // 毎ループ
  virtual bool onGesture(Gesture g, uint32_t now) { return false; }  // trueで消費（後続に回さない）
  virtual void draw(uint32_t now) {}        // 画面所有中、毎ループ。静止画面は自前のdirtyフラグで初回のみ描く
  virtual void onScreenLost() {}            // 横取り/タイムアウトで画面を失った
  virtual void appendState(String& json) {} // GET /state へ「,"key":value」形式で追記
};
