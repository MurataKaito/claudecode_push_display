# ファームウェアのモジュール構造と追加手順

## 構造

```
firmware/src/
  core/                  # app loop・画面調停・WiFi/HTTPサーバ基盤。通常触らない
  modules/<name>/        # 機能 = モジュール。1機能1ディレクトリ
    clawd/               #   ベース顔(idle/working)。優先度0で背景に常駐する特殊モジュール
    dance/               #   POST /dance でダンス演出を再生（機能追加のお手本）
    notify/ approve/ usage/  # 通知・承認・使用率
  module_registry.cpp    # 有効モジュール一覧。ここから消せばビルドに含まれない
  display.h / audio.h    # 共有の描画・音声ヘルパ（モジュールから直接includeして使う）
firmware/lib/
  screen_arbiter/        # 画面所有権の調停（純ロジック、nativeテスト対象）
  danceparam/            # /dance の ms 正規化（純ロジック、nativeテスト対象）
  gesture/ wavparse/     # 共有ライブラリ
```

## 画面調停のルール

- モジュールは `svc_->requestScreen(priority, timeoutMs, now)` で画面を要求する
- grant条件: 画面が空き / 自分が所有中 / `新priority >= 現ownerのpriority`（同優先度は後勝ち）
- priorityの目安: 承認など割り込み禁止=100、通常の表示・通知=50、演出=40、背景ベース顔=0
- timeout経過か横取りで `onScreenLost()` が呼ばれる。所有中は毎ループ `draw(now)` が呼ばれる
- ベース顔（idle/working）は clawd モジュールが優先度0で背景に常駐して描く。core は owner の `draw()` を呼ぶだけで、Clawd固有ロジックは持たない（registryから clawd を外せば背景顔ごと消える）

### 背景常駐オーナーのパターン（clawd）

常に画面に居たい「背景」モジュールは、`loop()` で毎回 `requestScreen(0, kRenewMs, now)` を呼ぶ。owner不在なら取得、自分がownerなら期限更新、上位（40/50/100）が居れば拒否されるだけ。上位が退けば次ループで自動的に背景を取り戻す。専用のコア機構は不要で、既存の画面調停にそのまま乗る。

## ジェスチャのルール

- core がタッチからジェスチャ（TAP/DOUBLETAP/SWIPE/NONE)を検出し、
  「画面のowner → レジストリ順」で `onGesture(g, now)` を回す。trueを返すと消費
- リリースイベントは GESTURE_NONE でも配られる（「何でもタッチでOK」系のUIのため）

## モジュール追加手順

1. `src/modules/<name>/<name>.h` `.cpp` を作り `Module` を実装する

```cpp
// src/modules/hello/hello.h
#pragma once
#include "core/module.h"

// POST /hello?text= を受けて顔オーバーレイを3秒表示する最小例
class HelloModule : public Module {
 public:
  const char* name() override { return "hello"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  void draw(uint32_t now) override;

 private:
  struct Pending {
    volatile bool ready = false;  // asyncタスク→loop()受け渡し。最後に立てる
    String text;
  };
  Pending pending_;
  Services* svc_ = nullptr;
  String text_;
};
```

```cpp
// src/modules/hello/hello.cpp
#include "hello.h"
#include "display.h"

void HelloModule::setup(Services& s) {
  svc_ = &s;
  s.http->on("/hello", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pending_.text = req->hasParam("text") ? req->getParam("text")->value() : String("はろーなのだ");
    pending_.ready = true;
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void HelloModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  if (!svc_->requestScreen(50, 3000, now)) return;  // 拒否されたらpendingのまま再試行
  pending_.ready = false;
  text_ = pending_.text;
}

void HelloModule::draw(uint32_t now) { tickFace("normal", text_); }
```

2. `src/module_registry.cpp` に3行（include・インスタンス・配列1行）足す

```cpp
#include "modules/hello/hello.h"
static HelloModule helloModule;
// MODULES[] 配列に &helloModule, を追加
```

3. ビルドして書き込む

```bash
pio run -e core2 -t upload
curl -X POST "http://stackchan.local/hello?text=てすと"
```

## 設計上の決まりごと

- HTTP受信ハンドラ（asyncタスク）では描画・音声再生をしない。pendingに積んで `loop()` で処理する
- 純ロジック（タイマー計算・パースなど）は `lib/` に切り出して native テストを書く
  （`pio test -e native`）
- 共有状態を増やさない。モジュールの状態はモジュールのメンバに持つ

## 設計の経緯

設計判断の背景は [`docs/superpowers/specs/2026-06-12-modular-firmware-design.md`](superpowers/specs/2026-06-12-modular-firmware-design.md) を参照。
