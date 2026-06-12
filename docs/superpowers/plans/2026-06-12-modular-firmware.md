# モジュール化ファームウェア Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ファームウェアを「機能 = モジュール（1ディレクトリ + レジストリ1行）」構造に再編し、既存3機能（notify/approve/usage）を移植して規約を実証する。

**Architecture:** core（app loop・画面調停・HTTPサーバ基盤）と modules（機能単位）に分離。画面の取り合いは Arduino 非依存の ScreenArbiter（priority + timeout、同優先度は後勝ち）が一元調停し、native でユニットテストする。HTTP 受信は async タスク → モジュール内 pending フラグ → loop() で処理、の現行パターンを踏襲。**純リファクタ**であり、HTTPエンドポイント仕様・UX・daemon連携は一切変えない。

**Tech Stack:** PlatformIO (espressif32 / native), M5Unified, ESPAsyncWebServer, ArduinoJson, Unity (test)

**Spec:** `docs/superpowers/specs/2026-06-12-modular-firmware-design.md`

**作業ディレクトリ:** すべてのコマンドは `firmware/` で実行する。

---

### Task 1: ScreenArbiter（TDD・native）

**Files:**
- Create: `firmware/lib/screen_arbiter/screen_arbiter.h`
- Create: `firmware/lib/screen_arbiter/screen_arbiter.cpp`
- Test: `firmware/test/test_screen_arbiter/test_screen_arbiter.cpp`

- [ ] **Step 1: 失敗するテストを書く**

`firmware/test/test_screen_arbiter/test_screen_arbiter.cpp`:

```cpp
#include <unity.h>
#include "screen_arbiter.h"

void test_grant_when_vacant(void) {
  ScreenArbiter a;
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(0, 50, 1000, 0, &pre));
  TEST_ASSERT_EQUAL_INT(-1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_vacant_owner_is_minus1(void) {
  ScreenArbiter a;
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
  TEST_ASSERT_EQUAL_INT(-1, a.tick(100));
}

void test_deny_lower_priority(void) {
  ScreenArbiter a;
  a.request(0, 100, 30000, 0, nullptr);  // 承認が前面
  int pre = -2;
  TEST_ASSERT_FALSE(a.request(1, 50, 4000, 10, &pre));  // 通知は拒否
  TEST_ASSERT_EQUAL_INT(-1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_equal_priority_last_wins(void) {
  ScreenArbiter a;
  a.request(0, 50, 4000, 0, nullptr);  // 通知が前面
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(1, 50, 5000, 10, &pre));  // タップ→使用率が奪う
  TEST_ASSERT_EQUAL_INT(0, pre);  // 被横取りを通知
  TEST_ASSERT_EQUAL_INT(1, a.owner());
}

void test_higher_priority_preempts(void) {
  ScreenArbiter a;
  a.request(1, 50, 4000, 0, nullptr);
  int pre = -2;
  TEST_ASSERT_TRUE(a.request(0, 100, 30000, 10, &pre));
  TEST_ASSERT_EQUAL_INT(1, pre);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_same_owner_rerequest_always_granted(void) {
  ScreenArbiter a;
  a.request(0, 100, 30000, 0, nullptr);
  int pre = -2;
  // 承認がフィードバック表示のため自分の優先度を下げるケース
  TEST_ASSERT_TRUE(a.request(0, 50, 1800, 10, &pre));
  TEST_ASSERT_EQUAL_INT(-1, pre);  // 自分自身はpreemptedにしない
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_timeout_releases(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  TEST_ASSERT_EQUAL_INT(-1, a.tick(999));   // 期限前
  TEST_ASSERT_EQUAL_INT(0, a.tick(1000));   // 期限到来でowner idを返す
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
  TEST_ASSERT_EQUAL_INT(-1, a.tick(1001));  // 二重発火しない
}

void test_release_by_owner(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  a.release(0);
  TEST_ASSERT_EQUAL_INT(-1, a.owner());
}

void test_release_by_non_owner_ignored(void) {
  ScreenArbiter a;
  a.request(0, 50, 1000, 0, nullptr);
  a.release(1);
  TEST_ASSERT_EQUAL_INT(0, a.owner());
}

void test_grant_after_timeout(void) {
  ScreenArbiter a;
  a.request(0, 100, 1000, 0, nullptr);
  a.tick(1000);
  TEST_ASSERT_TRUE(a.request(1, 50, 4000, 1100, nullptr));
}

void test_millis_wraparound(void) {
  ScreenArbiter a;
  // millis()が32bit上限間際 → deadlineがラップしても正しく判定できること
  a.request(0, 50, 0x200, 0xFFFFFF00u, nullptr);
  TEST_ASSERT_EQUAL_INT(-1, a.tick(0xFFFFFFF0u));  // まだ期限前
  TEST_ASSERT_EQUAL_INT(0, a.tick(0x100u));        // ラップ後に期限到来
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_grant_when_vacant);
  RUN_TEST(test_vacant_owner_is_minus1);
  RUN_TEST(test_deny_lower_priority);
  RUN_TEST(test_equal_priority_last_wins);
  RUN_TEST(test_higher_priority_preempts);
  RUN_TEST(test_same_owner_rerequest_always_granted);
  RUN_TEST(test_timeout_releases);
  RUN_TEST(test_release_by_owner);
  RUN_TEST(test_release_by_non_owner_ignored);
  RUN_TEST(test_grant_after_timeout);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
```

- [ ] **Step 2: テストが失敗することを確認**

Run: `pio test -e native -f test_screen_arbiter`
Expected: FAIL（`screen_arbiter.h: No such file or directory` のコンパイルエラー = レッド状態）

- [ ] **Step 3: 最小実装を書く**

`firmware/lib/screen_arbiter/screen_arbiter.h`:

```cpp
#pragma once
#include <stdint.h>

// 画面の所有権を一元調停する。Arduino非依存（nativeテスト対象）。
// grant条件: owner不在 / 同一ownerの再request / 新priority >= 現priority（同優先度は後勝ち）。
class ScreenArbiter {
 public:
  // 画面を要求。grantならtrue。別ownerを横取りした場合 *preempted にそのidを格納（それ以外は-1）。
  bool request(int id, int priority, uint32_t timeoutMs, uint32_t now, int* preempted = nullptr);
  void release(int id);    // owner本人以外からの呼び出しは無視
  int tick(uint32_t now);  // 期限切れならそのowner idを返して所有解除。なければ-1
  int owner() const { return ownerId_; }

 private:
  int ownerId_ = -1;
  int priority_ = 0;
  uint32_t deadline_ = 0;
};
```

`firmware/lib/screen_arbiter/screen_arbiter.cpp`:

```cpp
#include "screen_arbiter.h"

bool ScreenArbiter::request(int id, int priority, uint32_t timeoutMs, uint32_t now, int* preempted) {
  if (preempted) *preempted = -1;
  if (ownerId_ != -1 && ownerId_ != id && priority < priority_) return false;
  if (preempted && ownerId_ != -1 && ownerId_ != id) *preempted = ownerId_;
  ownerId_ = id;
  priority_ = priority;
  deadline_ = now + timeoutMs;
  return true;
}

void ScreenArbiter::release(int id) {
  if (id == ownerId_) ownerId_ = -1;
}

int ScreenArbiter::tick(uint32_t now) {
  if (ownerId_ == -1) return -1;
  // 符号付き差分でmillis()ラップアラウンドに耐える
  if ((int32_t)(now - deadline_) < 0) return -1;
  int expired = ownerId_;
  ownerId_ = -1;
  return expired;
}
```

- [ ] **Step 4: テストが通ることを確認**

Run: `pio test -e native -f test_screen_arbiter`
Expected: PASS（11 Tests 0 Failures）

- [ ] **Step 5: 既存テストも全部通ることを確認**

Run: `pio test -e native`
Expected: test_gesture / test_wavparse / test_screen_arbiter すべて PASS

- [ ] **Step 6: Commit**

```bash
git add lib/screen_arbiter test/test_screen_arbiter
git commit -m "feat(firmware): 画面調停ScreenArbiterをTDDで追加（native対象の純ロジック）"
```

---

### Task 2: Moduleインターフェース定義 + インクルードパス設定

**Files:**
- Create: `firmware/src/core/module.h`
- Modify: `firmware/platformio.ini:14`（build_flags に -Isrc 追加）

- [ ] **Step 1: core/module.h を書く**

```cpp
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
```

- [ ] **Step 2: platformio.ini の core2 env に -Isrc を追加**

変更前: `build_flags = -DCORE_DEBUG_LEVEL=2`
変更後: `build_flags = -DCORE_DEBUG_LEVEL=2 -Isrc`

（`#include "core/module.h"` / `#include "modules/..."` を src 起点で書けるようにするため）

- [ ] **Step 3: ビルド確認**

Run: `pio run -e core2`
Expected: SUCCESS（module.h はまだ誰も include していないが、ini変更の破壊がないことを確認）

- [ ] **Step 4: Commit**

```bash
git add src/core/module.h platformio.ini
git commit -m "feat(firmware): Module/Servicesインターフェース定義と-Isrc設定"
```

---

### Task 3: core骨格への置き換え（net_core / app / 空レジストリ、旧ファイル削除）

このタスク完了時点では基盤のみ動作（アイドル顔・/heartbeat・/state・/base・/clip）。/notify /approve /usage は Task 4〜6 で復活する。**中間状態として機能が一時的に消えるのは意図どおり**（ブランチ内のみ。実機リグレッションは Task 8 で実施）。

**Files:**
- Create: `firmware/src/core/net_core.h` / `firmware/src/core/net_core.cpp`
- Create: `firmware/src/core/app.h` / `firmware/src/core/app.cpp`
- Create: `firmware/src/module_registry.h` / `firmware/src/module_registry.cpp`
- Modify: `firmware/src/main.cpp`（全置換）
- Delete: `firmware/src/net.h` / `firmware/src/net.cpp` / `firmware/src/app_state.h`

- [ ] **Step 1: module_registry.h / .cpp を書く（空レジストリ）**

`firmware/src/module_registry.h`:

```cpp
#pragma once
#include "core/module.h"

// 有効モジュール一覧。ここに登録されたものだけがビルドに含まれる（部分組み込み）。
// 並び順 = ジェスチャのフォールバック順（前面モジュールの次に先頭から試行）。
extern Module* const MODULES[];
extern const size_t MODULE_COUNT;
```

`firmware/src/module_registry.cpp`:

```cpp
#include "module_registry.h"

// まだモジュールなし（Task 4以降で追加）。nullptrは空配列を避ける番兵でCOUNT=0なら参照されない。
Module* const MODULES[] = { nullptr };
const size_t MODULE_COUNT = 0;
```

- [ ] **Step 2: core/net_core.h / .cpp を書く**

`firmware/src/core/net_core.h`:

```cpp
#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void netCoreBegin(const char* ssid, const char* pass);  // WiFi接続 + mDNS + 基盤ルート登録
void netCoreStart();                                    // server.begin()。全モジュールsetup後に呼ぶ
AsyncWebServer& netServer();
const String& netDaemonBase();  // /heartbeat で学習したdaemonのURL。未学習なら空文字
int netBaseState();             // 0=idle / 1=working
```

`firmware/src/core/net_core.cpp`:

```cpp
#include "core/net_core.h"
#include "core/app.h"
#include "module_registry.h"
#include "display.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static AsyncWebServer server(80);
static String daemonBase;
static volatile int baseState = 0;  // 0=idle / 1=working

AsyncWebServer& netServer() { return server; }
const String& netDaemonBase() { return daemonBase; }
int netBaseState() { return baseState; }

void netCoreBegin(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(300);

  if (MDNS.begin("stackchan")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/heartbeat", HTTP_POST, [](AsyncWebServerRequest* req) {
    String ip = req->client()->remoteIP().toString();
    int port = req->hasParam("port") ? req->getParam("port")->value().toInt() : 4920;
    daemonBase = "http://" + ip + ":" + String(port);
    Serial.printf("[HB] daemon=%s\n", daemonBase.c_str());
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/base", HTTP_POST, [](AsyncWebServerRequest* req) {
    String s = req->hasParam("s") ? req->getParam("s")->value() : String("idle");
    baseState = (s == "working") ? 1 : 0;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/clip", HTTP_POST, [](AsyncWebServerRequest* req) {
    String p = req->hasParam("p") ? req->getParam("p")->value() : String("look");
    uint32_t ms = req->hasParam("ms") ? (uint32_t)req->getParam("ms")->value().toInt() : 4000;
    displayForceClip(p, ms);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // core分のフィールド + 各モジュールのappendState()を連結（既存キーは名前・型とも維持）
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    String j = "{\"daemonBase\":\"" + daemonBase + "\",\"touch\":" + String(appTouchReleases()) +
               ",\"g\":" + String(appLastGesture());
    for (size_t i = 0; i < MODULE_COUNT; i++) MODULES[i]->appendState(j);
    j += ",\"dbg\":\"" + displayDebug() + "\"}";
    req->send(200, "application/json", j);
  });
}

void netCoreStart() { server.begin(); }
```

- [ ] **Step 3: core/app.h / .cpp を書く**

`firmware/src/core/app.h`:

```cpp
#pragma once

void appSetup();
void appLoop();
int appTouchReleases();  // /state 診断用カウンタ
int appLastGesture();    // /state 診断用（最後のジェスチャ 0NONE/1TAP/2DOUBLE/3SWIPE）
```

`firmware/src/core/app.cpp`:

```cpp
#include "core/app.h"
#include "core/module.h"
#include "core/net_core.h"
#include "module_registry.h"
#include "display.h"
#include "audio.h"
#include "secrets.h"
#include "gesture.h"
#include "screen_arbiter.h"
#include <M5Unified.h>

static GestureDetector gestureDetector;
static ScreenArbiter arbiter;
static Services* services = nullptr;
static volatile int touchReleases = 0;
static volatile int lastGesture = -1;

int appTouchReleases() { return touchReleases; }
int appLastGesture() { return lastGesture; }

bool Services::requestScreen(int priority, uint32_t timeoutMs, uint32_t now) {
  int preempted = -1;
  if (!arbiter.request(selfId, priority, timeoutMs, now, &preempted)) return false;
  if (preempted >= 0) MODULES[preempted]->onScreenLost();
  return true;
}

void Services::releaseScreen() { arbiter.release(selfId); }
const String& Services::daemonBase() const { return netDaemonBase(); }
int Services::baseState() const { return netBaseState(); }

// 前面モジュール → レジストリ順。最初にtrueを返したところで止める。
static void dispatchGesture(Gesture g, uint32_t now) {
  int own = arbiter.owner();
  if (own >= 0 && MODULES[own]->onGesture(g, now)) return;
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    if ((int)i == own) continue;
    if (MODULES[i]->onGesture(g, now)) return;
  }
}

void appSetup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  displayBegin();
  audioBegin();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print("WiFi...");
  netCoreBegin(WIFI_SSID, WIFI_PASS);

  services = new Services[MODULE_COUNT > 0 ? MODULE_COUNT : 1];
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    services[i].http = &netServer();
    services[i].selfId = (int)i;
    MODULES[i]->setup(services[i]);
  }
  netCoreStart();  // 全モジュールのルート登録が済んでからlisten開始
}

void appLoop() {
  M5.update();
  uint32_t now = millis();

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gestureDetector.down(now, t.x, t.y);
  if (t.wasReleased()) {
    touchReleases = touchReleases + 1;
    Gesture g = gestureDetector.up(now, t.x, t.y);
    lastGesture = (int)g;
    dispatchGesture(g, now);  // GESTURE_NONEも配る（承認の「スワイプ以外は何でもallow」のため）
  }

  // 期限切れの画面を解放
  int expired = arbiter.tick(now);
  if (expired >= 0) MODULES[expired]->onScreenLost();

  for (size_t i = 0; i < MODULE_COUNT; i++) MODULES[i]->loop(now);

  // 描画: ownerがいればそのdraw()、いなければベース顔（baseState連動）
  int own = arbiter.owner();
  if (own >= 0) {
    MODULES[own]->draw(now);
  } else {
    bool working = netBaseState() == 1;
    tickFace(working ? "working" : "normal",
             working ? "おしごとちゅうなのだ" : "まってるのだ");
  }
  delay(10);
}
```

- [ ] **Step 4: main.cpp を全置換**

`firmware/src/main.cpp`:

```cpp
#include "core/app.h"

void setup() { appSetup(); }
void loop() { appLoop(); }
```

- [ ] **Step 5: 旧ファイルを削除**

```bash
git rm src/net.h src/net.cpp src/app_state.h
```

- [ ] **Step 6: ビルド確認**

Run: `pio run -e core2`
Expected: SUCCESS

- [ ] **Step 7: Commit**

```bash
git add src/core src/module_registry.h src/module_registry.cpp src/main.cpp
git commit -m "refactor(firmware): core骨格(app/net_core/registry)に再編、mode変数を画面調停に置換"
```

---

### Task 4: notifyモジュール（通知 + WAV再生の移植）

**Files:**
- Create: `firmware/src/modules/notify/notify.h` / `firmware/src/modules/notify/notify.cpp`
- Modify: `firmware/src/module_registry.cpp`（全置換）

- [ ] **Step 1: notify.h を書く**

```cpp
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
```

- [ ] **Step 2: notify.cpp を書く**

```cpp
#include "notify.h"
#include "audio.h"
#include "display.h"

void NotifyModule::setup(Services& s) {
  svc_ = &s;
  s.http->on(
      "/notify", HTTP_POST,
      [](AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); },
      nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        onBody(req, data, len, index, total);
      });
}

// /notify?expr=&text=  body=WAVバイナリ
void NotifyModule::onBody(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                          size_t index, size_t total) {
  if (index == 0) {
    // 受信開始: 前のバッファを解放しPSRAMに確保
    if (pending_.wav) { free(pending_.wav); pending_.wav = nullptr; }
    pending_.wav = (uint8_t*)ps_malloc(total);
    pending_.wavLen = 0;
  }
  if (pending_.wav && index + len <= total) {
    memcpy(pending_.wav + index, data, len);
    pending_.wavLen = index + len;
  }
  if (index + len == total) {
    pending_.expr = req->hasParam("expr") ? req->getParam("expr")->value() : String("normal");
    pending_.text = req->hasParam("text") ? req->getParam("text")->value() : String("");
    pending_.ready = true;  // loop()が拾う
  }
}

void NotifyModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  // 承認(100)が前面の間はgrantされず、pendingのまま次ループ再試行（現行挙動: 承認中は割り込まない）
  if (!svc_->requestScreen(50, 4000, now)) return;
  pending_.ready = false;
  expr_ = pending_.expr;
  text_ = pending_.text.length() ? pending_.text : "おしらせなのだ";
  if (pending_.wav && pending_.wavLen > 0) playWav(pending_.wav, pending_.wavLen);
}

void NotifyModule::draw(uint32_t now) { tickFace(expr_, text_); }
```

- [ ] **Step 3: module_registry.cpp を更新（全置換）**

```cpp
#include "module_registry.h"
#include "modules/notify/notify.h"

static NotifyModule notifyModule;

Module* const MODULES[] = {
    &notifyModule,
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
```

- [ ] **Step 4: ビルド確認**

Run: `pio run -e core2`
Expected: SUCCESS

- [ ] **Step 5: Commit**

```bash
git add src/modules/notify src/module_registry.cpp
git commit -m "feat(firmware): notifyモジュール移植（通知顔+WAV再生、承認中はpending再試行）"
```

---

### Task 5: approveモジュール（承認フローの移植）

**Files:**
- Create: `firmware/src/modules/approve/approve.h` / `firmware/src/modules/approve/approve.cpp`
- Modify: `firmware/src/module_registry.cpp`（全置換）

- [ ] **Step 1: approve.h を書く**

```cpp
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
```

- [ ] **Step 2: approve.cpp を書く**

```cpp
#include "approve.h"
#include "display.h"
#include <HTTPClient.h>

void ApproveModule::setup(Services& s) {
  svc_ = &s;
  s.http->on("/approve", HTTP_POST, [this](AsyncWebServerRequest* req) {
    pending_.id = req->hasParam("id") ? req->getParam("id")->value() : String("");
    pending_.title = req->hasParam("title") ? req->getParam("title")->value() : String("CLAUDE OK?");
    pending_.detail = req->hasParam("detail") ? req->getParam("detail")->value() : String("");
    pending_.ready = true;
    recv_ = recv_ + 1;
    req->send(200, "application/json", "{\"ok\":true}");
  });
}

void ApproveModule::loop(uint32_t now) {
  if (!pending_.ready) return;
  pending_.ready = false;
  // priority 100は常にgrant（承認中に新しい承認が来たら後勝ちで置き換え=現行挙動）
  if (svc_->requestScreen(100, 30000, now)) {
    id_ = pending_.id;
    title_ = pending_.title;
    detail_ = pending_.detail;
    shown_++;
    phase_ = PHASE_SHOWING;
    drawn_ = false;
  }
}

bool ApproveModule::onGesture(Gesture g, uint32_t now) {
  if (phase_ != PHASE_SHOWING) return false;
  // スワイプ=deny、それ以外のタッチ（タップ/長押し等）=allow（現行挙動）
  if (g == GESTURE_SWIPE) {
    postResult("deny");
    fbExpr_ = "worried";
    fbText_ = "やめておくのだ";
  } else {
    postResult("allow");
    fbExpr_ = "wink";
    fbText_ = "オッケーなのだ";
  }
  phase_ = PHASE_FEEDBACK;
  // フィードバック顔は通知と同格(50)に自己降格。通知やタップで上書きされてよい（現行挙動）
  svc_->requestScreen(50, 1800, now);
  return true;
}

void ApproveModule::draw(uint32_t now) {
  if (phase_ == PHASE_SHOWING) {
    if (!drawn_) {
      showApprove(title_, detail_);  // 静止画面なので初回のみ描画
      drawn_ = true;
    }
  } else if (phase_ == PHASE_FEEDBACK) {
    tickFace(fbExpr_, fbText_);
  }
}

void ApproveModule::postResult(const char* decision) {
  if (svc_->daemonBase().length() == 0) return;
  HTTPClient http;
  http.begin(svc_->daemonBase() + "/approve_result/" + id_);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);
  http.POST(String("{\"decision\":\"") + decision + "\"}");
  http.end();
}

// /state の既存キー recv/shown/ready/lastId はapproveの所有（現行のg_approve系の移管）
void ApproveModule::appendState(String& json) {
  json += ",\"recv\":" + String(recv_) + ",\"shown\":" + String(shown_) +
          ",\"ready\":" + String(pending_.ready ? 1 : 0) + ",\"lastId\":\"" + pending_.id + "\"";
}
```

- [ ] **Step 3: module_registry.cpp を更新（全置換）**

```cpp
#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/notify/notify.h"

static ApproveModule approveModule;
static NotifyModule notifyModule;

Module* const MODULES[] = {
    &approveModule,  // フォールバック先頭（承認表示中は全ジェスチャを消費する）
    &notifyModule,
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
```

- [ ] **Step 4: ビルド確認**

Run: `pio run -e core2`
Expected: SUCCESS

- [ ] **Step 5: Commit**

```bash
git add src/modules/approve src/module_registry.cpp
git commit -m "feat(firmware): approveモジュール移植（priority100・スワイプdeny・/state診断キー維持）"
```

---

### Task 6: usageモジュール（タップ→使用率表示の移植）

**Files:**
- Create: `firmware/src/modules/usage/usage.h` / `firmware/src/modules/usage/usage.cpp`
- Modify: `firmware/src/module_registry.cpp`（全置換）

- [ ] **Step 1: usage.h を書く**

```cpp
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
```

- [ ] **Step 2: usage.cpp を書く**

```cpp
#include "usage.h"
#include "display.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool UsageModule::onGesture(Gesture g, uint32_t now) {
  if (g != GESTURE_TAP && g != GESTURE_DOUBLETAP) return false;
  if (phase_ == PHASE_USAGE) return true;  // 使用率表示中の再タップは無視（現行どおり）
  // シングルタップでも使用率（重いアニメで2連打を取りこぼすため）。
  // 承認(100)が前面ならrequestScreenが拒否され何も起きない（現行: 承認中タップはapproveが消費）。
  fetchAndShow(now);
  return true;
}

void UsageModule::fetchAndShow(uint32_t now) {
  if (svc_->daemonBase().length() == 0) {
    showFeedback("worried", "まだつながってないのだ", now);
    return;
  }
  HTTPClient http;
  http.begin(svc_->daemonBase() + "/usage");
  http.setTimeout(4000);  // この間loopはブロックする（現行と同じ）
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      if (svc_->requestScreen(50, 5000, now)) {
        percent_ = doc["percent"] | 0;
        resetMin_ = doc["resetMin"] | 0;
        phase_ = PHASE_USAGE;
        drawn_ = false;
      }
    }
  } else {
    showFeedback("worried", "しゅとくしっぱいなのだ", now);
  }
  http.end();
}

void UsageModule::showFeedback(const char* expr, const char* text, uint32_t now) {
  if (!svc_->requestScreen(50, 2500, now)) return;
  fbExpr_ = expr;
  fbText_ = text;
  phase_ = PHASE_FEEDBACK;
}

void UsageModule::draw(uint32_t now) {
  if (phase_ == PHASE_USAGE) {
    if (!drawn_) {
      showUsage(percent_, resetMin_);  // 静止画面なので初回のみ描画
      drawn_ = true;
    }
  } else if (phase_ == PHASE_FEEDBACK) {
    tickFace(fbExpr_, fbText_);
  }
}
```

- [ ] **Step 3: module_registry.cpp を更新（全置換・最終形）**

```cpp
#include "module_registry.h"
#include "modules/approve/approve.h"
#include "modules/notify/notify.h"
#include "modules/usage/usage.h"

static ApproveModule approveModule;
static NotifyModule notifyModule;
static UsageModule usageModule;

Module* const MODULES[] = {
    &approveModule,  // フォールバック先頭（承認表示中は全ジェスチャを消費する）
    &notifyModule,
    &usageModule,    // タップ/ダブルタップを拾う（通知オーバーレイ中でも有効）
};
const size_t MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);
```

- [ ] **Step 4: ビルド確認**

Run: `pio run -e core2`
Expected: SUCCESS

- [ ] **Step 5: native全テスト確認**

Run: `pio test -e native`
Expected: 全テスト PASS

- [ ] **Step 6: Commit**

```bash
git add src/modules/usage src/module_registry.cpp
git commit -m "feat(firmware): usageモジュール移植（タップ起動・失敗フィードバック）。既存3機能の移植完了"
```

---

### Task 7: ドキュメント（モジュール追加手順）

**Files:**
- Create: `docs/firmware-modules.md`
- Modify: `README.md:86-87` 付近（ドキュメントリンク一覧に1行追加）

- [ ] **Step 1: docs/firmware-modules.md を書く**

内容は以下の構成（コード例はTask 4のNotifyModuleではなく、最小のサンプルモジュールを使う）:

````markdown
# ファームウェアのモジュール構造と追加手順

## 構造

```
firmware/src/
  core/                  # app loop・画面調停・WiFi/HTTPサーバ基盤。通常触らない
  modules/<name>/        # 機能 = モジュール。1機能1ディレクトリ
  module_registry.cpp    # 有効モジュール一覧。ここから消せばビルドに含まれない
  display.h / audio.h    # 共有の描画・音声ヘルパ（モジュールから直接includeして使う）
firmware/lib/
  screen_arbiter/        # 画面所有権の調停（純ロジック、nativeテスト対象）
  gesture/ wavparse/     # 共有ライブラリ
```

## 画面調停のルール

- モジュールは `svc_->requestScreen(priority, timeoutMs, now)` で画面を要求する
- grant条件: 画面が空き / 自分が所有中 / `新priority >= 現ownerのpriority`（同優先度は後勝ち）
- priorityの目安: 承認など割り込み禁止=100、通常の表示・通知=50
- timeout経過か横取りで `onScreenLost()` が呼ばれる。所有中は毎ループ `draw(now)` が呼ばれる
- 誰も所有していないときは core がベース顔（idle/working）を描く

## ジェスチャのルール

- core がタッチからジェスチャ（TAP/DOUBLETAP/SWIPE/NONE）を検出し、
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
````

- [ ] **Step 2: README.md のドキュメント一覧にリンク追加**

86-87行目の設計書リンク群に以下の1行を追加:

```markdown
- ファームウェアのモジュール構造と追加手順: [`docs/firmware-modules.md`](docs/firmware-modules.md)
```

- [ ] **Step 3: Commit**

```bash
git add docs/firmware-modules.md README.md
git commit -m "docs: ファームウェアのモジュール構造と追加手順を文書化"
```

---

### Task 8: 最終検証

- [ ] **Step 1: native全テスト**

Run: `pio test -e native`
Expected: test_gesture / test_wavparse / test_screen_arbiter 全PASS

- [ ] **Step 2: core2ビルド**

Run: `pio run -e core2`
Expected: SUCCESS。RAM/Flash使用率が従来から大きく増えていないことを目視確認

- [ ] **Step 3: 旧構成の残骸がないことを確認**

Run: `ls src/ src/core/ src/modules/`
Expected: `src/net.h` `src/net.cpp` `src/app_state.h` が存在しない。main.cpp が数行のみ

- [ ] **Step 4: 実機リグレッション（要デバイス・ユーザー実施）**

`pio run -e core2 -t upload` で書き込み後、specのチェックリストを確認:

- [ ] 起動→WiFi接続→アイドル顔アニメ
- [ ] daemonからの通知（顔+テキスト+WAV再生、4秒で復帰）
- [ ] /base working/idle でベース顔が切り替わる
- [ ] 承認要求→画面静止表示→タップでallow/スワイプでdeny→フィードバック顔→daemonへPOST
- [ ] 承認30秒放置でアイドル復帰
- [ ] アイドル中タップで使用率表示、5秒で復帰
- [ ] daemon未接続時タップで「まだつながってないのだ」
- [ ] GET /state が従来キー（daemonBase/recv/shown/touch/g/ready/lastId/dbg）を返す
