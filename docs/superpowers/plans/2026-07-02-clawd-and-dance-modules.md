# clawd（ベース顔）/ dance（演出）モジュール化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** コア(`app.cpp`)に直書きされているベース顔（idle/working）を優先度0の背景常駐モジュール `clawd` に切り出し、発火型ダンス演出モジュール `dance`（`POST /dance`）を新機能追加のお手本として追加する。

**Architecture:** 画面調停 `ScreenArbiter` の「owner不在 / 自分の再request / 新priority >= 現priority」でgrantする性質を使い、clawdを優先度0で毎ループ再requestする背景オーナーにする。コア／調停ロジックは無改造で、`app.cpp` の else 節を削るだけ。dance は既存モジュールの pending→loop→requestScreen 作法に従う。アニメ描画エンジン（`display.cpp`）は共有ヘルパのまま全モジュールが直接使う。

**Tech Stack:** PlatformIO / Arduino framework / ESP32 (M5Stack Core2) / M5Unified / ESPAsyncWebServer / ArduinoJson。純ロジックは `lib/` に置き native + Unity でテスト。

設計の根拠: [`docs/superpowers/specs/2026-07-02-clawd-and-dance-modules-design.md`](../specs/2026-07-02-clawd-and-dance-modules-design.md)

## Global Constraints

- ビルド対象env: `core2`（`platform = espressif32@^6.7.0`, `board = m5stack-core2`, `board_build.filesystem = littlefs`, `build_flags = -Isrc`）。テストenv: `native`（`test_framework = unity`, `-std=gnu++17`）
- 依存: `m5stack/M5Unified@^0.2.2`, `bblanchon/ArduinoJson@^7.1.0`, `mathieucarbou/ESPAsyncWebServer@^3.3.12`
- HTTP受信ハンドラ（async TCPタスク）では描画・音声再生をしない。pendingに積んで `loop()` で処理する。pendingの `ready`（volatile bool）は書き込み完了後に最後に立てる
- 純ロジックは `lib/<name>/` に切り出し `test/test_<name>/` に native テストを書く
- 既存エンドポイント（`/heartbeat` `/base` `/clip` `/state` `/notify` `/approve`）のパス・パラメータ・レスポンスは変えない。追加は `POST /dance` のみ
- `daemon/` `hooks/` `assets/` には触れない。ベース顔・通知・承認・使用率のUX（表示内容・タイムアウト秒数・優先順位）は不変
- コミット名義は `chickenpapa`（kddiメール禁止）。コミットメッセージ末尾に `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>` を付ける

---

## File Structure

作成・変更するファイルと責務:

- `firmware/src/modules/clawd/clawd.h`（新規）— ClawdModule 宣言。背景常駐オーナーのベース顔
- `firmware/src/modules/clawd/clawd.cpp`（新規）— 背景常駐（requestScreen(0)）・ベース顔描画・`/clip` 登録・`dbg` 出力
- `firmware/src/core/app.cpp`（変更）— else 節（ベース顔描画）を削除。他は無変更
- `firmware/src/core/net_core.cpp`（変更）— `/clip` ルートと `/state` の `dbg` 追記を削除（clawdへ移設）。`display.h` include も除去。`/base` は残す
- `firmware/src/module_registry.cpp`（変更）— clawd と dance を登録
- `firmware/lib/danceparam/danceparam.h` / `.cpp`（新規）— `/dance` の ms 正規化の純ロジック
- `firmware/test/test_danceparam/test_danceparam.cpp`（新規）— danceparam の native テスト
- `firmware/src/modules/dance/dance.h`（新規）— DanceModule 宣言
- `firmware/src/modules/dance/dance.cpp`（新規）— `POST /dance` 受信・priority40 で画面要求・doneプール再生

`display.cpp` のアニメエンジンは移動しない（clawdへ移すと notify/approve/usage がclawdに依存するため）。`displayBegin()` / `audioBegin()` は引き続きコアの `appSetup()` が初期化する。

---

## Task 1: clawd モジュール（ベース顔を背景常駐に切り出し）

コア `app.cpp` の else 節（`arbiter.owner()<0` のときのベース顔描画）と、net_core が持つ `/clip`・`dbg` を、新しい clawd モジュールに移す。純リファクタで外部挙動は不変。

**Files:**
- Create: `firmware/src/modules/clawd/clawd.h`
- Create: `firmware/src/modules/clawd/clawd.cpp`
- Modify: `firmware/src/module_registry.cpp`
- Modify: `firmware/src/core/app.cpp:82-89`（else 節）
- Modify: `firmware/src/core/net_core.cpp:4,40-45,52`（display.h include / `/clip` / dbg 行）

**Interfaces:**
- Consumes: `Module`/`Services`（`core/module.h`）、`tickFace(const String&, const String&)` / `displayForceClip(const String&, uint32_t)` / `displayDebug()`（`display.h`）、`Services::requestScreen(int,uint32_t,uint32_t)` / `Services::baseState()`
- Produces: `class ClawdModule : public Module`（`module_registry.cpp` から `static ClawdModule clawdModule;` として登録される）

- [ ] **Step 1: clawd.h を作成**

```cpp
// firmware/src/modules/clawd/clawd.h
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
```

- [ ] **Step 2: clawd.cpp を作成**

```cpp
// firmware/src/modules/clawd/clawd.cpp
#include "clawd.h"
#include "display.h"

void ClawdModule::setup(Services& s) {
  svc_ = &s;
  // /clip: 指定prefixのクリップをms間強制表示（デバッグ用）。net_coreから移設。
  s.http->on("/clip", HTTP_POST, [](AsyncWebServerRequest* req) {
    String p = req->hasParam("p") ? req->getParam("p")->value() : String("look");
    uint32_t ms = req->hasParam("ms") ? (uint32_t)req->getParam("ms")->value().toInt() : 4000;
    displayForceClip(p, ms);
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void ClawdModule::loop(uint32_t now) {
  // 優先度0で背景に常駐。自分がownerなら期限更新、空きなら取得、上位ownerがいればfalse（何もしない）。
  svc_->requestScreen(0, 1000, now);
}

void ClawdModule::draw(uint32_t now) {
  bool working = svc_->baseState() == 1;
  tickFace(working ? "working" : "normal",
           working ? "おしごとちゅうなのだ" : "まってるのだ");
}

void ClawdModule::appendState(String& json) {
  json += ",\"dbg\":\"" + displayDebug() + "\"";
}
```

- [ ] **Step 3: module_registry.cpp に clawd を登録**

`firmware/src/module_registry.cpp` を以下の内容にする（clawd を最後尾＝背景常駐オーナーとして追加）:

```cpp
#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/notify/notify.h"
#include "modules/usage/usage.h"
#include "modules/clawd/clawd.h"

static ApproveModule approveModule;
static NotifyModule notifyModule;
static UsageModule usageModule;
static ClawdModule clawdModule;

Module* const MODULES[] = {
    &approveModule,  // フォールバック先頭（承認表示中は全ジェスチャを消費する）
    &notifyModule,
    &usageModule,    // タップ/ダブルタップを拾う（通知オーバーレイ中でも有効）
    &clawdModule,    // 背景(0) 常駐オーナー。onGestureは消費しない
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
```

- [ ] **Step 4: app.cpp の else 節を削除**

`firmware/src/core/app.cpp` の描画部を変更する。

変更前（82-89行）:
```cpp
  int own = arbiter.owner();
  if (own >= 0) {
    MODULES[own]->draw(now);
  } else {
    bool working = netBaseState() == 1;
    tickFace(working ? "working" : "normal",
             working ? "おしごとちゅうなのだ" : "まってるのだ");
  }
```

変更後:
```cpp
  int own = arbiter.owner();
  if (own >= 0) MODULES[own]->draw(now);  // ベース顔はclawdモジュール(優先度0背景)が描く
```

- [ ] **Step 5: net_core.cpp から /clip と dbg を除去**

`firmware/src/core/net_core.cpp` を3箇所変更する。

(a) `#include "display.h"`（4行目）を削除する（`/clip` と `dbg` を移設後は net_core で display 依存が無くなる）。

(b) `/clip` ルート（40-45行）を丸ごと削除する:
```cpp
  server.on("/clip", HTTP_POST, [](AsyncWebServerRequest* req) {
    String p = req->hasParam("p") ? req->getParam("p")->value() : String("look");
    uint32_t ms = req->hasParam("ms") ? (uint32_t)req->getParam("ms")->value().toInt() : 4000;
    displayForceClip(p, ms);
    req->send(200, "application/json", "{\"ok\":true}");
  });
```

(c) `/state` の dbg 追記（52行）を、JSONを閉じるだけに変更する。

変更前:
```cpp
    j += ",\"dbg\":\"" + displayDebug() + "\"}";
```
変更後:
```cpp
    j += "}";
```

（dbg は clawd の `appendState()` がモジュールループ内で追記するので、clawd が MODULES 最後尾にいる限り `/state` の JSON 末尾に従来どおり dbg が出る。JSON のキー順は消費側の契約ではないため互換）

- [ ] **Step 6: core2 ビルドを実行**

Run: `cd firmware && pio run -e core2`
Expected: 末尾に `SUCCESS` が出て終了コード0。`modules/clawd/clawd.cpp` がコンパイルされリンクが通る。

- [ ] **Step 7: native テストが無変更でパスすることを確認**

Run: `cd firmware && pio test -e native`
Expected: 既存 test_gesture / test_wavparse / test_screen_arbiter が全て PASS（このタスクは native 側に影響しないため緑のまま）。

- [ ] **Step 8: コミット**

```bash
cd /Users/ka-murata/Documents/dotlog/claudecode_push_display
git add firmware/src/modules/clawd/ firmware/src/module_registry.cpp firmware/src/core/app.cpp firmware/src/core/net_core.cpp
git commit -m "$(cat <<'EOF'
feat(firmware): clawdモジュール移植（ベース顔を優先度0の背景常駐に切り出し）

- app.cppのelse節(ベース顔描画)をClawdModuleへ移設。coreからClawd固有ロジックが消える
- /clipとstateのdbgをnet_coreからclawdへ移設(外部仕様は不変)
- registryにclawdを最後尾(背景常駐オーナー)として登録

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 9（実機・手動リグレッション、ハード必要）**

`pio run -e core2 -t upload` 後、以下を目視確認（外部挙動が Task 前と同一であること）:
- [ ] 起動→WiFi接続→アイドル顔アニメが出る（clawd 背景が描く）
- [ ] `curl -X POST "http://stackchan.local/base?s=working"` でおしごと顔、`?s=idle` で待機顔に切り替わる
- [ ] `curl -X POST "http://stackchan.local/clip?p=wink&ms=3000"` で wink が3秒出て戻る
- [ ] 通知・承認・使用率タップが従来どおり動き、終わると clawd 背景に復帰する
- [ ] `curl "http://stackchan.local/state"` が従来キー（末尾に `dbg`）を含む JSON を返す

---

## Task 2: danceparam ライブラリ（ms 正規化の純ロジック）＋ native テスト

`/dance` の `ms` パラメータを正規化する純関数を TDD で作る。`ms<=0`（未指定）は既定 6000ms、それ以外は `[500, 30000]` にクランプする。

**Files:**
- Create: `firmware/lib/danceparam/danceparam.h`
- Create: `firmware/lib/danceparam/danceparam.cpp`
- Test: `firmware/test/test_danceparam/test_danceparam.cpp`

**Interfaces:**
- Consumes: `<stdint.h>` のみ（Arduino非依存）
- Produces: `uint32_t clampDanceMs(long ms);`（Task 3 の dance.cpp が使用）

- [ ] **Step 1: 失敗するテストを書く**

```cpp
// firmware/test/test_danceparam/test_danceparam.cpp
#include <unity.h>
#include "danceparam.h"

void test_default_when_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(0));  // 未指定
}

void test_default_when_negative(void) {
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(-100));
}

void test_clamp_below_min(void) {
  TEST_ASSERT_EQUAL_UINT32(500, clampDanceMs(100));  // 下限
}

void test_clamp_above_max(void) {
  TEST_ASSERT_EQUAL_UINT32(30000, clampDanceMs(999999));  // 上限
}

void test_passthrough_in_range(void) {
  TEST_ASSERT_EQUAL_UINT32(500, clampDanceMs(500));      // 境界(下)
  TEST_ASSERT_EQUAL_UINT32(6000, clampDanceMs(6000));    // 中間
  TEST_ASSERT_EQUAL_UINT32(30000, clampDanceMs(30000));  // 境界(上)
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_default_when_zero);
  RUN_TEST(test_default_when_negative);
  RUN_TEST(test_clamp_below_min);
  RUN_TEST(test_clamp_above_max);
  RUN_TEST(test_passthrough_in_range);
  return UNITY_END();
}
```

- [ ] **Step 2: ヘッダだけ作り、テストが「未定義でリンク失敗」することを確認**

まず宣言のみのヘッダを作る:
```cpp
// firmware/lib/danceparam/danceparam.h
#pragma once
#include <stdint.h>

// /dance の ms を正規化する。ms<=0（未指定）は既定6000ms、それ以外は[500,30000]にクランプ。
uint32_t clampDanceMs(long ms);
```

Run: `cd firmware && pio test -e native -f test_danceparam`
Expected: FAIL。`clampDanceMs` の実体が無くリンクエラー（undefined reference to `clampDanceMs`）で落ちる。

- [ ] **Step 3: 最小実装を書く**

```cpp
// firmware/lib/danceparam/danceparam.cpp
#include "danceparam.h"

uint32_t clampDanceMs(long ms) {
  if (ms <= 0) return 6000;      // 未指定/不正は既定
  if (ms < 500) return 500;      // 下限
  if (ms > 30000) return 30000;  // 上限
  return (uint32_t)ms;
}
```

- [ ] **Step 4: テストが通ることを確認**

Run: `cd firmware && pio test -e native -f test_danceparam`
Expected: PASS。5テスト全て緑。

- [ ] **Step 5: 全 native テストが緑のままか確認**

Run: `cd firmware && pio test -e native`
Expected: 既存3テスト + test_danceparam が全て PASS。

- [ ] **Step 6: コミット**

```bash
cd /Users/ka-murata/Documents/dotlog/claudecode_push_display
git add firmware/lib/danceparam/ firmware/test/test_danceparam/
git commit -m "$(cat <<'EOF'
test(firmware): dance msクランプ純ロジック(danceparam)をnativeテスト付きで追加

- ms<=0は既定6000ms、それ以外は[500,30000]にクランプ
- 未指定/負値/下限/上限/範囲内の5ケースをUnityで検証

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: dance モジュール（発火型ダンス演出）

`POST /dance?clip=&ms=` を受けて priority 40 で画面を取り、doneプール（bounce/sway/djmix/bouncedj/swaydj）をフルスクリーン再生して自動で戻る。既存モジュールの pending→loop→requestScreen 作法に従う。

**Files:**
- Create: `firmware/src/modules/dance/dance.h`
- Create: `firmware/src/modules/dance/dance.cpp`
- Modify: `firmware/src/module_registry.cpp`

**Interfaces:**
- Consumes: `Module`/`Services`（`core/module.h`）、`tickFace` / `displayForceClip`（`display.h`）、`clampDanceMs(long)`（`danceparam.h`、Task 2）、`Services::requestScreen`
- Produces: `class DanceModule : public Module`（`module_registry.cpp` から `static DanceModule danceModule;` として登録される）

- [ ] **Step 1: dance.h を作成**

```cpp
// firmware/src/modules/dance/dance.h
#pragma once
#include "core/module.h"

// POST /dance?clip=&ms= を受けてダンス演出を再生。priority 40（notify=50/approve=100の下）。
class DanceModule : public Module {
 public:
  const char* name() override { return "dance"; }
  void setup(Services& s) override;
  void loop(uint32_t now) override;
  void draw(uint32_t now) override;

 private:
  struct Pending {
    volatile bool ready = false;  // asyncタスク→loop()受け渡し。最後に立てる
    String clip;                  // 空なら "done" プールからランダム
    uint32_t ms = 6000;
  };
  Pending pending_;
  Services* svc_ = nullptr;
};
```

- [ ] **Step 2: dance.cpp を作成**

```cpp
// firmware/src/modules/dance/dance.cpp
#include "dance.h"
#include "display.h"
#include "danceparam.h"

void DanceModule::setup(Services& s) {
  svc_ = &s;
  s.http->on("/dance", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pending_.clip = req->hasParam("clip") ? req->getParam("clip")->value() : String("");
    long ms = req->hasParam("ms") ? req->getParam("ms")->value().toInt() : 0;
    pending_.ms = clampDanceMs(ms);
    pending_.ready = true;  // 描画・音声はここでしない。最後にreadyを立てる
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void DanceModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  // priority 40。承認(100)・通知(50)が前面なら拒否され、pendingのまま次ループ再試行。
  if (!svc_->requestScreen(40, pending_.ms, now)) return;
  pending_.ready = false;
  // clip指定時のみ強制クリップ。未指定はdraw()のtickFace("done")がdoneプールから選ぶ。
  if (pending_.clip.length()) displayForceClip(pending_.clip, pending_.ms);
}

void DanceModule::draw(uint32_t now) {
  // 画面所有中のみ呼ばれる。timeout(ms)満了でclawdが背景を取り戻しidle/workingへ復帰。
  tickFace("done", "おどるのだ");
}
```

- [ ] **Step 3: module_registry.cpp に dance を登録**

`firmware/src/module_registry.cpp` を以下の内容にする（approve の次に dance を追加）:

```cpp
#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/dance/dance.h"
#include "modules/notify/notify.h"
#include "modules/usage/usage.h"
#include "modules/clawd/clawd.h"

static ApproveModule approveModule;
static DanceModule danceModule;
static NotifyModule notifyModule;
static UsageModule usageModule;
static ClawdModule clawdModule;

Module* const MODULES[] = {
    &approveModule,  // 100（割り込み禁止）
    &danceModule,    // 40（演出）
    &notifyModule,   // 50
    &usageModule,    // タップ/ダブルタップ
    &clawdModule,    // 0（背景常駐オーナー）
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
```

- [ ] **Step 4: core2 ビルドを実行**

Run: `cd firmware && pio run -e core2`
Expected: 末尾に `SUCCESS`、終了コード0。`modules/dance/dance.cpp` と `lib/danceparam` がリンクされる。

- [ ] **Step 5: native テストが緑のままか確認**

Run: `cd firmware && pio test -e native`
Expected: 全テスト PASS（このタスクは既存 native テストに影響しない）。

- [ ] **Step 6: コミット**

```bash
cd /Users/ka-murata/Documents/dotlog/claudecode_push_display
git add firmware/src/modules/dance/ firmware/src/module_registry.cpp
git commit -m "$(cat <<'EOF'
feat(firmware): danceモジュール追加（POST /dance, priority40, doneプール再生）

- clip省略時はdoneプール(bounce/sway/djmix/bouncedj/swaydj)からランダム再生
- ms省略/範囲外はdanceparamで正規化(既定6000ms, [500,30000]クランプ)
- priority40で通知(50)・承認(100)を邪魔せず、退いてから再生。timeout満了でclawd復帰

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 7（実機・手動、ハード必要）**

`pio run -e core2 -t upload` 後:
- [ ] `curl -X POST "http://stackchan.local/dance"` でダンスが再生され、約6秒で idle/working に復帰する
- [ ] `curl -X POST "http://stackchan.local/dance?clip=djmix&ms=4000"` で djmix が約4秒再生されて戻る
- [ ] 通知中に `/dance` → 通知が終わってからダンスが出る（40 < 50）
- [ ] 承認中に `/dance` → 承認が終わってからダンスが出る（40 < 100）
- [ ] `curl -X POST "http://stackchan.local/dance?ms=100"` → 短すぎる指定でも500msにクランプされて破綻しない

---

## Self-Review

**1. Spec coverage（spec の各節 → 対応タスク）:**
- clawd モジュール（背景常駐・draw・/clip・dbg・app.cpp else 削除・net_core 差分）→ Task 1
- baseState/`/base` をコアに残す → Task 1（net_core 変更は /clip と dbg のみ、/base は触らない）
- dance モジュール（/dance・priority40・doneプール・onScreenLost不要）→ Task 3
- dance の ms クランプ（純ロジック・native テスト）→ Task 2
- registry 並び順（approve/dance/notify/usage/clawd）→ Task 3 Step 3
- 互換性（既存エンドポイント不変、daemon/hooks/assets 不変）→ Global Constraints + 各タスクの diff が限定的
- テスト計画（native + 実機リグレッション）→ Task 1 Step 7/9、Task 2 Step 4-5、Task 3 Step 5/7
- スコープ外（dance のジェスチャ/音楽、実行時ON/OFF）→ プランに含めない（記載どおり）

**2. Placeholder scan:** TBD/TODO・曖昧指示なし。全 code step に実コードを記載済み。

**3. Type consistency:** `clampDanceMs(long)→uint32_t` は Task 2 の定義と Task 3 の呼び出しで一致。`ClawdModule`/`DanceModule` の宣言（Task 1/3 の .h）と registry の `static ...Module` / `&...Module` が一致。`requestScreen(int,uint32_t,uint32_t)` の引数順は既存 `Services` 定義と一致。

**メモ:** spec の「ms 範囲外は既定6000扱い」という括弧書きは、本プランでは「範囲外はクランプ、未指定(ms<=0)のみ既定6000」に具体化した（クランプの方が直感的で、Task 2 のテストもこの挙動で固定）。外部挙動として `/dance?ms=100` は 500ms、`/dance?ms=99999` は 30000ms になる。

---

## 実装メモ（背景常駐オーナーの遷移が成立する根拠）

`ScreenArbiter::request` は「owner不在 / 自分の再request / 新priority>=現priority」でgrant。clawd の `requestScreen(0,1000,now)`:
- owner不在 → grant（clawd が背景オーナー）
- owner==clawd（自分）→ 期限更新のみ、preemptedなし
- owner==notify(50)/approve(100) → `0 < 50/100` で false（奪わない・何もしない）

notify/approve が `tick()` 期限切れ or release すると owner=-1 になり、次ループ（同ループ）の clawd `loop()` が再取得 → `draw()` で背景復帰。現行モジュールは `tick()` 期限切れか `onGesture` で退場するため、通常は描画空白すら発生しない（最悪でも約10msの直前フレーム保持）。
