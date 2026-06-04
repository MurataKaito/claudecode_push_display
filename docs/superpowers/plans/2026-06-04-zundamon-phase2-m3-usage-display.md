# ずんだもん Phase 2 / M3 — 使用率表示（画面2回タッチ）実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** M5の画面を2回タッチすると、daemon が `ccusage` で算出した「5時間枠の使用率%」をM5がゲージ表示する。

**Architecture:** daemon に `GET /usage`（ccusage→使用率算出）を追加。daemon は定期 `POST /heartbeat?port=` でM5に自分のアドレスを教え、M5はその送信元IP＋portを `g_daemonBase` として記憶。M5はダブルタップ検知時に `g_daemonBase/usage` を HTTPClient で取得しゲージ描画する。タッチのジェスチャ判定はテスト可能な純粋クラス `GestureDetector` に分離。

**Tech Stack:** 既存Phase1基盤（Node/TS+Fastify+vitest, PlatformIO+M5Unified）＋ HTTPClient/ArduinoJson(M5側)、ccusage(npx)。

**前提:** ブランチ `feature/zundamon-push-display`。Phase1完了済み（daemon/firmware稼働、`stackchan.local`疎通済み）。M3のみのスコープ（M4警告・M5承認は別計画）。

---

## ファイル構成（M3で作成/変更）

```
daemon/src/
  ccusage.ts            [新] npx ccusage blocks --active --json を実行・パース
  usage.ts              [新] ActiveBlock+limit+now → {percent,used,limit,resetAt,resetMin}
  config.ts             [変] usageLimit 追加
  transport/index.ts    [変] Transport に heartbeat(port) 追加
  transport/wifi.ts     [変] heartbeat 実装(POST /heartbeat?port=)
  server.ts             [変] GET /usage 追加（ServerDeps に getUsage）
  index.ts              [変] getUsage配線 + heartbeatインターバル
firmware/
  lib/gesture/gesture.h, gesture.cpp   [新] タップ/ダブルタップ/スワイプ判定(純粋)
  test/test_gesture/test_gesture.cpp   [新] native Unityテスト
  src/app_state.h       [変] g_daemonBase 追加
  src/net.cpp           [変] POST /heartbeat ハンドラ
  src/display.h/.cpp    [変] showUsage(percent,resetMin)
  src/main.cpp          [変] タッチ→GET /usage→ゲージ表示
```

---

## Task 1: ccusage 実行・パース（daemon）

**Files:** Create `daemon/src/ccusage.ts`, Test `daemon/src/ccusage.test.ts`

- [ ] **Step 1: 失敗するテスト** — `daemon/src/ccusage.test.ts`

実スキーマに基づくフィクスチャ。`runner` を注入して ccusage を実行せず検証。

```ts
import { describe, it, expect } from "vitest";
import { getActiveBlock } from "./ccusage.js";

const FIXTURE = JSON.stringify({
  blocks: [
    {
      id: "2026-06-04T01:00:00.000Z",
      startTime: "2026-06-04T01:00:00.000Z",
      endTime: "2026-06-04T06:00:00.000Z",
      isActive: true,
      totalTokens: 82818924,
      tokenCounts: { inputTokens: 21041, outputTokens: 354628 },
    },
  ],
});

describe("getActiveBlock", () => {
  it("active blockのtotalTokensとendTimeを返す", async () => {
    const block = await getActiveBlock(async () => FIXTURE);
    expect(block).toEqual({ totalTokens: 82818924, endTime: "2026-06-04T06:00:00.000Z" });
  });

  it("runnerが落ちたらnull", async () => {
    const block = await getActiveBlock(async () => {
      throw new Error("boom");
    });
    expect(block).toBeNull();
  });

  it("blocksが空ならnull", async () => {
    const block = await getActiveBlock(async () => JSON.stringify({ blocks: [] }));
    expect(block).toBeNull();
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- ccusage`
Expected: FAIL（`./ccusage.js` 未作成）

- [ ] **Step 3: 実装** — `daemon/src/ccusage.ts`

```ts
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const execFileP = promisify(execFile);

export type ActiveBlock = { totalTokens: number; endTime: string };
export type Runner = (cmd: string, args: string[]) => Promise<string>;

const defaultRunner: Runner = async (cmd, args) => {
  const { stdout } = await execFileP(cmd, args, { maxBuffer: 16 * 1024 * 1024 });
  return stdout;
};

export async function getActiveBlock(runner: Runner = defaultRunner): Promise<ActiveBlock | null> {
  try {
    const out = await runner("npx", ["-y", "ccusage@latest", "blocks", "--active", "--json"]);
    const data = JSON.parse(out);
    const block = data?.blocks?.[0];
    if (!block || typeof block.totalTokens !== "number" || typeof block.endTime !== "string") {
      return null;
    }
    return { totalTokens: block.totalTokens, endTime: block.endTime };
  } catch {
    return null;
  }
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- ccusage`
Expected: PASS（3 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/ccusage.ts daemon/src/ccusage.test.ts
git commit -m "feat(daemon): ccusage active-block reader"
```

---

## Task 2: 使用率算出（daemon）

**Files:** Create `daemon/src/usage.ts`, Test `daemon/src/usage.test.ts`

- [ ] **Step 1: 失敗するテスト** — `daemon/src/usage.test.ts`

```ts
import { describe, it, expect } from "vitest";
import { computeUsage } from "./usage.js";

describe("computeUsage", () => {
  const now = Date.parse("2026-06-04T05:00:00.000Z"); // endTimeの1時間前

  it("percent/used/resetMin を算出", () => {
    const u = computeUsage(
      { totalTokens: 50_000_000, endTime: "2026-06-04T06:00:00.000Z" },
      100_000_000,
      now,
    );
    expect(u.percent).toBe(50);
    expect(u.used).toBe(50_000_000);
    expect(u.limit).toBe(100_000_000);
    expect(u.resetAt).toBe("2026-06-04T06:00:00.000Z");
    expect(u.resetMin).toBe(60);
  });

  it("100%上限でクランプ", () => {
    const u = computeUsage(
      { totalTokens: 250_000_000, endTime: "2026-06-04T06:00:00.000Z" },
      100_000_000,
      now,
    );
    expect(u.percent).toBe(100);
  });

  it("blockがnullならゼロ", () => {
    const u = computeUsage(null, 100_000_000, now);
    expect(u).toEqual({ percent: 0, used: 0, limit: 100_000_000, resetAt: null, resetMin: 0 });
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- usage`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/usage.ts`

```ts
import type { ActiveBlock } from "./ccusage.js";

export type Usage = {
  percent: number;
  used: number;
  limit: number;
  resetAt: string | null;
  resetMin: number;
};

export function computeUsage(block: ActiveBlock | null, limit: number, nowMs: number): Usage {
  if (!block || limit <= 0) {
    return { percent: 0, used: 0, limit, resetAt: null, resetMin: 0 };
  }
  const used = block.totalTokens;
  const percent = Math.min(100, Math.round((used / limit) * 100));
  const resetMin = Math.max(0, Math.round((Date.parse(block.endTime) - nowMs) / 60000));
  return { percent, used, limit, resetAt: block.endTime, resetMin };
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- usage`
Expected: PASS（3 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/usage.ts daemon/src/usage.test.ts
git commit -m "feat(daemon): compute usage percent from active block"
```

---

## Task 3: config に usageLimit 追加（daemon）

**Files:** Modify `daemon/src/config.ts`, `daemon/src/config.test.ts`

- [ ] **Step 1: テストを更新** — `daemon/src/config.test.ts` の2つの `expect` を差し替え

「空envなら既定値」テストの `toEqual` を次に変更:
```ts
    expect(c).toEqual({
      port: 4920,
      voicevoxUrl: "http://127.0.0.1:50021",
      speakerId: 3,
      m5Url: "http://stackchan.local",
      usageLimit: 100_000_000,
    });
```
「envで上書き」テストの末尾（`expect(c.m5Url)...` の後）に追加:
```ts
    expect(loadConfig({ ZUNDA_USAGE_LIMIT: "5000000" }).usageLimit).toBe(5_000_000);
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- config`
Expected: FAIL（usageLimit 未定義）

- [ ] **Step 3: 実装** — `daemon/src/config.ts` の `Config` 型と `loadConfig` に追加

`Config` 型に行を追加:
```ts
  usageLimit: number;
```
`loadConfig` の返却オブジェクトに行を追加（`m5Url` の次）:
```ts
    usageLimit: Number(env.ZUNDA_USAGE_LIMIT ?? 100_000_000),
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- config`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/config.ts daemon/src/config.test.ts
git commit -m "feat(daemon): add usageLimit config"
```

---

## Task 4: Transport に heartbeat 追加（daemon）

**Files:** Modify `daemon/src/transport/index.ts`, `daemon/src/transport/wifi.ts`, `daemon/src/transport/wifi.test.ts`

- [ ] **Step 1: IFに heartbeat を追加** — `daemon/src/transport/index.ts`

`interface Transport` を次に置き換え:
```ts
export interface Transport {
  notify(n: Notification): Promise<void>;
  heartbeat(daemonPort: number): Promise<void>;
}
```

- [ ] **Step 2: 失敗するテストを追加** — `daemon/src/transport/wifi.test.ts` の `describe` 末尾に追加

```ts
  it("heartbeat は /heartbeat?port= に POST する", async () => {
    let seenUrl = "";
    let seenMethod = "";
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      seenUrl = url;
      seenMethod = init?.method ?? "";
      return new Response('{"ok":true}', { status: 200 });
    }) as unknown as typeof fetch;

    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await t.heartbeat(4920);

    expect(seenUrl).toBe("http://stackchan.local/heartbeat?port=4920");
    expect(seenMethod).toBe("POST");
  });
```

- [ ] **Step 3: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- wifi`
Expected: FAIL（heartbeat 未実装）

- [ ] **Step 4: 実装** — `daemon/src/transport/wifi.ts` の class に追加（`notify` の後）

```ts
  async heartbeat(daemonPort: number): Promise<void> {
    await this.fetchImpl(`${this.m5Url}/heartbeat?port=${daemonPort}`, { method: "POST" });
  }
```

- [ ] **Step 5: テスト合格**

Run: `cd daemon && npm test -- wifi`
Expected: PASS（3 tests）

- [ ] **Step 6: コミット**

```bash
git add daemon/src/transport/index.ts daemon/src/transport/wifi.ts daemon/src/transport/wifi.test.ts
git commit -m "feat(daemon): transport heartbeat to announce daemon address"
```

---

## Task 5: GET /usage エンドポイント（daemon）

**Files:** Modify `daemon/src/server.ts`, `daemon/src/server.test.ts`

- [ ] **Step 1: 失敗するテストを追加** — `daemon/src/server.test.ts`

ファイル冒頭の import に `Usage` を足し、既存 `/event` テストの transport モックに `heartbeat` を、createServer 呼び出しに `getUsage` を追加（Task4でTransport IFに heartbeat が増えたため、付けないと `tsc` が落ちる）。最後に新 describe を追加。

import 追加:
```ts
import type { Usage } from "./usage.js";
```
既存 `/event` テスト内の `const transport: Transport = { notify: async (n) => { notified.push(n); } };` を次に置き換え:
```ts
    const transport: Transport = {
      notify: async (n) => {
        notified.push(n);
      },
      heartbeat: async () => {},
    };
```
同テスト内の `const app = createServer({ transport, synth });` を次に置き換え:
```ts
    const getUsage = async (): Promise<Usage> => ({
      percent: 42, used: 1, limit: 2, resetAt: null, resetMin: 0,
    });
    const app = createServer({ transport, synth, getUsage });
```
新テスト追加:
```ts
describe("createServer /usage", () => {
  it("getUsage の結果を返す", async () => {
    const transport = { notify: async () => {}, heartbeat: async () => {} };
    const synth = async () => Buffer.from("x");
    const usage: Usage = { percent: 73, used: 7, limit: 10, resetAt: "Z", resetMin: 12 };
    const app = createServer({ transport, synth, getUsage: async () => usage });
    const res = await app.inject({ method: "GET", url: "/usage" });
    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual(usage);
    await app.close();
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- server`
Expected: FAIL（getUsage 未対応／型不一致）

- [ ] **Step 3: 実装** — `daemon/src/server.ts`

import に追加:
```ts
import type { Usage } from "./usage.js";
```
`ServerDeps` を置き換え:
```ts
export type ServerDeps = {
  transport: Transport;
  synth: (text: string) => Promise<Buffer>;
  getUsage: () => Promise<Usage>;
};
```
`return app;` の前に追加:
```ts
  app.get("/usage", async (_req, reply) => {
    reply.send(await deps.getUsage());
  });
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- server`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/server.ts daemon/src/server.test.ts
git commit -m "feat(daemon): GET /usage endpoint"
```

---

## Task 6: index 配線（getUsage + heartbeat）（daemon）

**Files:** Modify `daemon/src/index.ts`

- [ ] **Step 1: 実装** — `daemon/src/index.ts` を次の内容に置き換え

```ts
import { loadConfig } from "./config.js";
import { createServer } from "./server.js";
import { WiFiTransport } from "./transport/wifi.js";
import { synth } from "./voicevox.js";
import { saySynth } from "./sayfallback.js";
import { getActiveBlock } from "./ccusage.js";
import { computeUsage } from "./usage.js";

const cfg = loadConfig();
const transport = new WiFiTransport(cfg.m5Url);

async function synthFn(text: string): Promise<Buffer> {
  try {
    return await synth(text, { baseUrl: cfg.voicevoxUrl, speakerId: cfg.speakerId });
  } catch (e) {
    console.warn(`VOICEVOX不可(${(e as Error).message}) → say にフォールバック`);
    return await saySynth(text);
  }
}

const app = createServer({
  transport,
  synth: synthFn,
  getUsage: async () => computeUsage(await getActiveBlock(), cfg.usageLimit, Date.now()),
});

app
  .listen({ port: cfg.port, host: "0.0.0.0" })
  .then(() => {
    console.log(`zundamon daemon: :${cfg.port}  M5=${cfg.m5Url}  VOICEVOX=${cfg.voicevoxUrl}`);
    // M5にdaemonアドレスを定期通知（起動直後にも1回）
    const beat = () => transport.heartbeat(cfg.port).catch(() => {});
    beat();
    setInterval(beat, 15000);
  })
  .catch((e) => {
    console.error(e);
    process.exit(1);
  });
```

- [ ] **Step 2: テスト＆ビルド確認**

Run: `cd daemon && npm test && npm run build`
Expected: 全PASS・型エラー無し

- [ ] **Step 3: コミット**

```bash
git add daemon/src/index.ts
git commit -m "feat(daemon): wire getUsage + periodic heartbeat"
```

---

## Task 7: ジェスチャ判定（firmware・純粋ロジック＋nativeテスト）

**Files:** Create `firmware/lib/gesture/gesture.h`, `firmware/lib/gesture/gesture.cpp`, Test `firmware/test/test_gesture/test_gesture.cpp`

- [ ] **Step 1: ヘッダ** — `firmware/lib/gesture/gesture.h`

```cpp
#pragma once
#include <stdint.h>

enum Gesture { GESTURE_NONE, GESTURE_TAP, GESTURE_DOUBLETAP, GESTURE_SWIPE };

// 指が触れた瞬間 down()、離れた瞬間 up() を呼ぶ。up() がジェスチャを返す。
// 直近TAPから doubleMs 以内の2回目TAPは DOUBLETAP。移動量が swipeThresh 以上は SWIPE。
class GestureDetector {
 public:
  void down(uint32_t t, int x, int y);
  Gesture up(uint32_t t, int x, int y);

 private:
  uint32_t downT_ = 0;
  int downX_ = 0;
  int downY_ = 0;
  uint32_t lastTapT_ = 0;
  int swipeThresh_ = 40;
  uint32_t doubleMs_ = 400;
  uint32_t tapMaxMs_ = 300;
};
```

- [ ] **Step 2: 失敗するテスト** — `firmware/test/test_gesture/test_gesture.cpp`

```cpp
#include <unity.h>
#include "gesture.h"

void test_single_tap(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_TAP, g.up(100, 102, 101));
}

void test_double_tap(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_TAP, g.up(100, 100, 100));
  g.down(200, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_DOUBLETAP, g.up(300, 100, 100));
}

void test_swipe(void) {
  GestureDetector g;
  g.down(0, 10, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_SWIPE, g.up(150, 200, 100));
}

void test_slow_press_is_none(void) {
  GestureDetector g;
  g.down(0, 100, 100);
  TEST_ASSERT_EQUAL_INT(GESTURE_NONE, g.up(5000, 100, 100));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_single_tap);
  RUN_TEST(test_double_tap);
  RUN_TEST(test_swipe);
  RUN_TEST(test_slow_press_is_none);
  return UNITY_END();
}
```

- [ ] **Step 3: テスト実行（失敗確認）**

Run: `cd firmware && pio test -e native`
Expected: FAIL（gesture 未実装でリンクエラー）

- [ ] **Step 4: 実装** — `firmware/lib/gesture/gesture.cpp`

```cpp
#include "gesture.h"
#include <stdlib.h>

void GestureDetector::down(uint32_t t, int x, int y) {
  downT_ = t;
  downX_ = x;
  downY_ = y;
}

Gesture GestureDetector::up(uint32_t t, int x, int y) {
  int dist = abs(x - downX_) + abs(y - downY_);
  uint32_t dur = t - downT_;
  if (dist >= swipeThresh_) {
    lastTapT_ = 0;
    return GESTURE_SWIPE;
  }
  if (dur <= tapMaxMs_) {
    if (lastTapT_ != 0 && (t - lastTapT_) <= doubleMs_) {
      lastTapT_ = 0;
      return GESTURE_DOUBLETAP;
    }
    lastTapT_ = t;
    return GESTURE_TAP;
  }
  lastTapT_ = 0;
  return GESTURE_NONE;
}
```

- [ ] **Step 5: テスト合格**

Run: `cd firmware && pio test -e native`
Expected: PASS（test_wavparse と test_gesture、合計2スイート）

- [ ] **Step 6: コミット**

```bash
git add firmware/lib/gesture/gesture.h firmware/lib/gesture/gesture.cpp firmware/test/test_gesture/test_gesture.cpp
git commit -m "feat(firmware): gesture detector (tap/double-tap/swipe) with native tests"
```

---

## Task 8: daemonアドレス記憶（firmware net /heartbeat）

**Files:** Modify `firmware/src/app_state.h`, `firmware/src/net.cpp`

- [ ] **Step 1: 共有状態に追加** — `firmware/src/app_state.h` の `extern PendingNotify g_notify;` の下に追加

```cpp
extern String g_daemonBase;  // 例: "http://172.20.10.5:4920"（heartbeatで学習）
```

- [ ] **Step 2: 実装** — `firmware/src/net.cpp`

`PendingNotify g_notify;` の下に定義を追加:
```cpp
String g_daemonBase;
```
`netBegin` の `server.on("/notify", ...)` 登録の直後に追加:
```cpp
  server.on("/heartbeat", HTTP_POST, [](AsyncWebServerRequest* req) {
    String ip = req->client()->remoteIP().toString();
    int port = req->hasParam("port") ? req->getParam("port")->value().toInt() : 4920;
    g_daemonBase = "http://" + ip + ":" + String(port);
    req->send(200, "application/json", "{\"ok\":true}");
  });
```

- [ ] **Step 3: ビルド確認**

Run: `cd firmware && pio run -e core2`
Expected: SUCCESS

- [ ] **Step 4: コミット**

```bash
git add firmware/src/app_state.h firmware/src/net.cpp
git commit -m "feat(firmware): learn daemon address from /heartbeat"
```

---

## Task 9: 使用率ゲージ描画（firmware display）

**Files:** Modify `firmware/src/display.h`, `firmware/src/display.cpp`

- [ ] **Step 1: ヘッダに宣言追加** — `firmware/src/display.h` の `showNotify` 宣言の下

```cpp
void showUsage(int percent, int resetMin);
```

- [ ] **Step 2: 実装** — `firmware/src/display.cpp` の末尾に追加

```cpp
void showUsage(int percent, int resetMin) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);

  // 大きな%表示
  M5.Display.setTextSize(2);
  M5.Display.drawString(String(percent) + "%", 160, 55);

  // バーゲージ
  const int x = 20, y = 95, w = 280, h = 30;
  M5.Display.drawRect(x, y, w, h, TFT_WHITE);
  int fill = (w - 2) * percent / 100;
  uint16_t c = percent >= 80 ? TFT_RED : (percent >= 50 ? TFT_ORANGE : TFT_GREEN);
  M5.Display.fillRect(x + 1, y + 1, fill, h - 2, c);

  // リセットまでの時間
  M5.Display.setTextSize(1);
  M5.Display.drawString("あと " + String(resetMin) + "分でリセットなのだ", 160, 160);
}
```

- [ ] **Step 3: ビルド確認**

Run: `cd firmware && pio run -e core2`
Expected: SUCCESS

- [ ] **Step 4: コミット**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat(firmware): usage gauge screen"
```

---

## Task 10: タッチ→使用率取得（firmware main）

**Files:** Modify `firmware/src/main.cpp`

- [ ] **Step 1: 実装** — `firmware/src/main.cpp` を次の内容に置き換え

```cpp
#include <M5Unified.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "display.h"
#include "audio.h"
#include "net.h"
#include "app_state.h"
#include "gesture.h"

static GestureDetector gesture;
static uint32_t usageUntil = 0;  // この時刻(ms)までUSAGE表示

static void requestAndShowUsage() {
  if (g_daemonBase.length() == 0) {
    showNotify("worried", "まだつながってないのだ");
    usageUntil = millis() + 2500;
    return;
  }
  HTTPClient http;
  http.begin(g_daemonBase + "/usage");
  http.setTimeout(4000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    JsonDocument doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      int percent = doc["percent"] | 0;
      int resetMin = doc["resetMin"] | 0;
      showUsage(percent, resetMin);
    } else {
      showNotify("worried", "へんじがへんなのだ");
    }
  } else {
    showNotify("worried", "しゅとくしっぱいなのだ");
  }
  http.end();
  usageUntil = millis() + 5000;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  displayBegin();
  audioBegin();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print("WiFi...");
  netBegin(WIFI_SSID, WIFI_PASS);
  showIdle();
}

void loop() {
  M5.update();
  uint32_t now = millis();

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gesture.down(now, t.x, t.y);
  if (t.wasReleased()) {
    Gesture g = gesture.up(now, t.x, t.y);
    if (g == GESTURE_DOUBLETAP && usageUntil == 0) {
      requestAndShowUsage();
    }
  }

  if (g_notify.ready) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) {
      playWav(g_notify.wav, g_notify.wavLen);
    }
    delay(3000);
    showIdle();
    usageUntil = 0;
  }

  if (usageUntil != 0 && now > usageUntil) {
    usageUntil = 0;
    showIdle();
  }

  delay(10);
}
```

- [ ] **Step 2: ビルド＆書き込み**

Run: `cd firmware && pio run -e core2 -t upload --upload-port /dev/cu.usbserial-54780284771`
Expected: SUCCESS、M5再起動

- [ ] **Step 3: 実機検証**

前提: daemon起動中（`cd daemon && npm run dev`）、M5が `stackchan.local` で疎通、heartbeat受信済み（起動後~15秒待つ or daemon再起動）。
手順: M5の画面を**素早く2回タッチ**。
Expected: 使用率%＋バーゲージ＋「あと N分でリセットなのだ」が表示され、約5秒後にIDLEへ戻る。

確認用（daemon側の/usageを直接叩いて値を見る）:
```bash
curl -s localhost:4920/usage | python3 -m json.tool
```
Expected: `{"percent":..,"used":..,"limit":100000000,"resetAt":"..","resetMin":..}`

- [ ] **Step 4: コミット**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): double-tap fetches and shows usage gauge"
```

---

## 完了の定義（M3）

- `cd daemon && npm test` 全PASS（ccusage/usage/config/wifi/server 追加分含む）、`npm run build` 成功
- `cd firmware && pio test -e native` PASS（wavparse + gesture）
- 実機: ダブルタップ→使用率ゲージ表示（Task10 Step3）
- `curl localhost:4920/usage` が妥当なJSONを返す

## 次（別計画）
- **M4 残量警告**: daemonに使用率ポーラ＋閾値判定（ccusage/usage を再利用）→ 既存 `/notify` で warned 発話
- **M5 タップ承認**: PreToolUseフック＋`/approve`＋M5タップ/スワイプ（双方向、`g_daemonBase` を再利用）
