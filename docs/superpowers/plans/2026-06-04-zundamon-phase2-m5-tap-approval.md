# ずんだもん Phase 2 / M5 — タップ承認（TAP OK / SWIPE NG）実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Claude Code が（既定 Bash）ツールを実行する前に、M5へ「CLAUDE BASH OK?」を表示し、**タップ=許可 / スワイプ=拒否** で応答する。

**Architecture:** PreToolUse フックが daemon `POST /approve` で承認を要求→daemonは承認を pending 登録しM5へ `POST /approve` をpush→M5は承認画面を出し、タップ/スワイプで `POST /approve_result/{id}` を daemon に返す→フックは `GET /approve_result/{id}` をポーリングし allow/deny を出力（タイムアウト時は ask=通常プロンプト）。承認ストアは純粋クラス `Approvals` に分離。M5のdaemonアドレスはM3の `g_daemonBase`、ジェスチャはM3の `GestureDetector` を再利用。

**Tech Stack:** 既存 daemon（Node/TS+Fastify+vitest, crypto.randomUUID）/ bash+curl+python3（フック）/ PlatformIO+M5Unified+ArduinoJson。

**前提:** ブランチ `feature/zundamon-push-display`。M3/M4完了済み（`g_daemonBase`,`GestureDetector`,transport heartbeat,config 稼働）。

---

## ファイル構成（M5）

```
daemon/src/
  approvals.ts          [新] 承認pendingストア（純粋）+ test
  transport/index.ts    [変] requestApproval 追加
  transport/wifi.ts     [変] requestApproval 実装(POST /approve JSON)
  config.ts             [変] approveTimeoutSec 追加
  server.ts             [変] POST /approve, GET/POST /approve_result/:id, ServerDeps拡張
  server.test.ts        [変] 既存モックに requestApproval/approvals/now/approveTtlMs 追加 + 承認テスト
  index.ts              [変] Approvals 配線
firmware/src/
  app_state.h           [変] PendingApprove g_approve
  net.cpp               [変] POST /approve ハンドラ
  display.h/.cpp        [変] showApprove(title, detail)
  main.cpp              [変] APPROVEモード（tap=allow/swipe=deny→POST result）
hooks/
  pretooluse.sh         [新] 承認要求→結果ポーリング→allow/deny/ask
  pretooluse.test.sh    [新] スタブdaemonで allow を検証
```

---

## Task 1: 承認ストア（daemon・純粋）

**Files:** Create `daemon/src/approvals.ts`, Test `daemon/src/approvals.test.ts`

- [ ] **Step 1: 失敗するテスト** — `daemon/src/approvals.test.ts`

```ts
import { describe, it, expect } from "vitest";
import { Approvals } from "./approvals.js";

describe("Approvals", () => {
  it("作成直後は pending", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.get("id1", 100)).toBe("pending");
  });

  it("resolve で allow/deny", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.resolve("id1", "allow")).toBe(true);
    expect(a.get("id1", 100)).toBe("allow");
  });

  it("期限超過で timeout", () => {
    const a = new Approvals();
    a.create("id1", 1000, 0);
    expect(a.get("id1", 2000)).toBe("timeout");
  });

  it("未知idは unknown", () => {
    const a = new Approvals();
    expect(a.get("nope", 0)).toBe("unknown");
    expect(a.resolve("nope", "allow")).toBe(false);
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- approvals`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/approvals.ts`

```ts
export type Decision = "allow" | "deny";
export type ApprovalStatus = Decision | "pending" | "timeout" | "unknown";

type Record = { decision: Decision | null; expiresAt: number };

export class Approvals {
  private map = new Map<string, Record>();

  create(id: string, ttlMs: number, now: number): void {
    this.map.set(id, { decision: null, expiresAt: now + ttlMs });
  }

  resolve(id: string, decision: Decision): boolean {
    const r = this.map.get(id);
    if (!r) return false;
    r.decision = decision;
    return true;
  }

  get(id: string, now: number): ApprovalStatus {
    const r = this.map.get(id);
    if (!r) return "unknown";
    if (r.decision) return r.decision;
    if (now > r.expiresAt) return "timeout";
    return "pending";
  }
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- approvals`
Expected: PASS（4 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/approvals.ts daemon/src/approvals.test.ts
git commit -m "feat(daemon): approval pending store"
```

---

## Task 2: transport に requestApproval 追加（daemon）

**Files:** Modify `daemon/src/transport/index.ts`, `daemon/src/transport/wifi.ts`, `daemon/src/transport/wifi.test.ts`

- [ ] **Step 1: IFに追加** — `daemon/src/transport/index.ts` の `interface Transport` を置き換え

```ts
export interface Transport {
  notify(n: Notification): Promise<void>;
  heartbeat(daemonPort: number): Promise<void>;
  requestApproval(id: string, title: string, detail: string): Promise<void>;
}
```

- [ ] **Step 2: 失敗するテストを追加** — `daemon/src/transport/wifi.test.ts` の describe 末尾に追加

```ts
  it("requestApproval は /approve に JSON を POST する", async () => {
    let seenUrl = "";
    let seenBody = "";
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      seenUrl = url;
      seenBody = String(init?.body ?? "");
      return new Response('{"ok":true}', { status: 200 });
    }) as unknown as typeof fetch;

    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await t.requestApproval("id1", "CLAUDE BASH OK?", "ls -la");

    expect(seenUrl).toBe("http://stackchan.local/approve");
    const body = JSON.parse(seenBody);
    expect(body).toEqual({ id: "id1", title: "CLAUDE BASH OK?", detail: "ls -la" });
  });
```

- [ ] **Step 3: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- wifi`
Expected: FAIL

- [ ] **Step 4: 実装** — `daemon/src/transport/wifi.ts` の class に追加（`heartbeat` の後）

```ts
  async requestApproval(id: string, title: string, detail: string): Promise<void> {
    await this.fetchImpl(`${this.m5Url}/approve`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id, title, detail }),
    });
  }
```

- [ ] **Step 5: テスト合格**

Run: `cd daemon && npm test -- wifi`
Expected: PASS（4 tests）

- [ ] **Step 6: コミット**

```bash
git add daemon/src/transport/index.ts daemon/src/transport/wifi.ts daemon/src/transport/wifi.test.ts
git commit -m "feat(daemon): transport requestApproval (POST /approve to M5)"
```

---

## Task 3: config に approveTimeoutSec（daemon）

**Files:** Modify `daemon/src/config.ts`, `daemon/src/config.test.ts`

- [ ] **Step 1: テスト更新** — `daemon/src/config.test.ts`

「空envなら既定値」の `toEqual` に1行追加:
```ts
      approveTimeoutSec: 30,
```
「envで上書き」末尾に追加:
```ts
    expect(loadConfig({ ZUNDA_APPROVE_TIMEOUT_SEC: "5" }).approveTimeoutSec).toBe(5);
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- config`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/config.ts`

`Config` 型に追加:
```ts
  approveTimeoutSec: number;
```
`loadConfig` 返却に追加（`pollIntervalSec` の後）:
```ts
    approveTimeoutSec: Number(env.ZUNDA_APPROVE_TIMEOUT_SEC ?? 30),
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- config`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/config.ts daemon/src/config.test.ts
git commit -m "feat(daemon): approveTimeoutSec config"
```

---

## Task 4: 承認エンドポイント（daemon server）

**Files:** Modify `daemon/src/server.ts`, `daemon/src/server.test.ts`

- [ ] **Step 1: テストを更新/追加** — `daemon/src/server.test.ts`

冒頭importに追加:
```ts
import { Approvals } from "./approvals.js";
```
共通モック transport を全テストで使えるよう、各 `const transport: Transport = {...}` に `requestApproval: async () => {}` を追加（/event と /usage の2箇所）。例（/event）:
```ts
    const transport: Transport = {
      notify: async (n) => {
        notified.push(n);
      },
      heartbeat: async () => {},
      requestApproval: async () => {},
    };
```
（/usage の transport も同様に `requestApproval: async () => {}` を追加）

各 `createServer({...})` 呼び出しに承認系depsを追加（/event と /usage の2箇所）:
```ts
      approvals: new Approvals(),
      approveTtlMs: 30000,
      now: () => 0,
```
すなわち /event は `createServer({ transport, synth, getUsage, approvals: new Approvals(), approveTtlMs: 30000, now: () => 0 })`、/usage も同様。

末尾に承認フローのテストを追加:
```ts
describe("createServer approval flow", () => {
  it("POST /approve→M5要求→POST result→GET resultでallow", async () => {
    let approved: { id: string; title: string; detail: string } | null = null;
    const transport: Transport = {
      notify: async () => {},
      heartbeat: async () => {},
      requestApproval: async (id, title, detail) => {
        approved = { id, title, detail };
      },
    };
    const approvals = new Approvals();
    const app = createServer({
      transport,
      synth: async () => Buffer.from("x"),
      getUsage: async () => ({ percent: 0, used: 0, limit: 1, resetAt: null, resetMin: 0 }),
      approvals,
      approveTtlMs: 30000,
      now: () => 0,
    });

    const r1 = await app.inject({
      method: "POST",
      url: "/approve",
      payload: { tool: "Bash", command: "ls -la" },
    });
    expect(r1.statusCode).toBe(200);
    const id = r1.json().id as string;
    expect(id).toBeTruthy();
    expect(approved).not.toBeNull();
    expect(approved!.title).toContain("Bash");

    const rp = await app.inject({ method: "GET", url: `/approve_result/${id}` });
    expect(rp.json()).toEqual({ decision: "pending" });

    await app.inject({
      method: "POST",
      url: `/approve_result/${id}`,
      payload: { decision: "allow" },
    });
    const r2 = await app.inject({ method: "GET", url: `/approve_result/${id}` });
    expect(r2.json()).toEqual({ decision: "allow" });
    await app.close();
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- server`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/server.ts`

import群に追加:
```ts
import { randomUUID } from "node:crypto";
import type { Approvals, Decision } from "./approvals.js";
```
`ServerDeps` を置き換え:
```ts
export type ServerDeps = {
  transport: Transport;
  synth: (text: string) => Promise<Buffer>;
  getUsage: () => Promise<Usage>;
  approvals: Approvals;
  approveTtlMs: number;
  now: () => number;
};
```
`return app;` の前に追加:
```ts
  app.post("/approve", async (req, reply) => {
    const { tool, command } = (req.body ?? {}) as { tool?: string; command?: string };
    const id = randomUUID();
    deps.approvals.create(id, deps.approveTtlMs, deps.now());
    const title = `CLAUDE ${tool ?? "TOOL"} OK?`;
    await deps.transport.requestApproval(id, title, command ?? "");
    reply.send({ id });
  });

  app.get("/approve_result/:id", async (req, reply) => {
    const { id } = req.params as { id: string };
    reply.send({ decision: deps.approvals.get(id, deps.now()) });
  });

  app.post("/approve_result/:id", async (req, reply) => {
    const { id } = req.params as { id: string };
    const { decision } = (req.body ?? {}) as { decision?: Decision };
    const ok = decision ? deps.approvals.resolve(id, decision) : false;
    reply.send({ ok });
  });
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- server`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/server.ts daemon/src/server.test.ts
git commit -m "feat(daemon): /approve + /approve_result endpoints"
```

---

## Task 5: index 配線（Approvals）（daemon）

**Files:** Modify `daemon/src/index.ts`

- [ ] **Step 1: 実装** — `daemon/src/index.ts`

import群に追加:
```ts
import { Approvals } from "./approvals.js";
```
`const transport = ...` の下に追加:
```ts
const approvals = new Approvals();
```
`createServer({...})` 呼び出しに承認depsを追加:
```ts
const app = createServer({
  transport,
  synth: synthFn,
  getUsage: async () => computeUsage(await getActiveBlock(), cfg.usageLimit, Date.now()),
  approvals,
  approveTtlMs: cfg.approveTimeoutSec * 1000,
  now: () => Date.now(),
});
```

- [ ] **Step 2: テスト＆ビルド**

Run: `cd daemon && npm test && npm run build`
Expected: 全PASS・型エラー無し

- [ ] **Step 3: コミット**

```bash
git add daemon/src/index.ts
git commit -m "feat(daemon): wire Approvals into server"
```

---

## Task 6: 承認の受信（firmware app_state + net）

**Files:** Modify `firmware/src/app_state.h`, `firmware/src/net.cpp`

- [ ] **Step 1: 共有状態追加** — `firmware/src/app_state.h` の `extern String g_daemonBase;` の下

```cpp
struct PendingApprove {
  volatile bool ready = false;  // 表示すべき承認あり
  String id;
  String title;
  String detail;
};
extern PendingApprove g_approve;
```

- [ ] **Step 2: 実装** — `firmware/src/net.cpp`

`String g_daemonBase;` の下に追加:
```cpp
PendingApprove g_approve;
```
`<ArduinoJson.h>` を include 群に追加（ファイル先頭の `#include <ESPAsyncWebServer.h>` の下）:
```cpp
#include <ArduinoJson.h>
```
`server.on("/heartbeat", ...)` の後に追加:
```cpp
  static String approveBuf;
  server.on(
      "/approve", HTTP_POST,
      [](AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); },
      nullptr,
      [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index == 0) approveBuf = "";
        for (size_t i = 0; i < len; i++) approveBuf += (char)data[i];
        if (index + len == total) {
          JsonDocument doc;
          if (deserializeJson(doc, approveBuf) == DeserializationError::Ok) {
            g_approve.id = (const char*)(doc["id"] | "");
            g_approve.title = (const char*)(doc["title"] | "CLAUDE OK?");
            g_approve.detail = (const char*)(doc["detail"] | "");
            g_approve.ready = true;
          }
        }
      });
```

- [ ] **Step 3: ビルド確認**

Run: `cd firmware && pio run -e core2`
Expected: SUCCESS

- [ ] **Step 4: コミット**

```bash
git add firmware/src/app_state.h firmware/src/net.cpp
git commit -m "feat(firmware): receive approval requests via POST /approve"
```

---

## Task 7: 承認画面（firmware display）

**Files:** Modify `firmware/src/display.h`, `firmware/src/display.cpp`

- [ ] **Step 1: 宣言追加** — `firmware/src/display.h` の `showUsage` の下

```cpp
void showApprove(const String& title, const String& detail);
```

- [ ] **Step 2: 実装** — `firmware/src/display.cpp` 末尾に追加

```cpp
void showApprove(const String& title, const String& detail) {
  M5.Display.fillScreen(TFT_NAVY);
  M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
  M5.Display.setTextDatum(middle_center);

  M5.Display.setTextSize(2);
  M5.Display.drawString(title.length() ? title : "CLAUDE OK?", 160, 45);

  M5.Display.setTextSize(1);
  // 長いコマンドは切り詰め
  String d = detail;
  if (d.length() > 38) d = d.substring(0, 37) + "…";
  M5.Display.drawString(d, 160, 110);

  M5.Display.setTextColor(TFT_GREENYELLOW, TFT_NAVY);
  M5.Display.drawString("TAP = OK   /   SWIPE = NG", 160, 175);
}
```

- [ ] **Step 3: ビルド確認**

Run: `cd firmware && pio run -e core2`
Expected: SUCCESS

- [ ] **Step 4: コミット**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat(firmware): approval screen (TAP=OK / SWIPE=NG)"
```

---

## Task 8: 承認モード（firmware main）

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

enum Mode { MODE_IDLE, MODE_USAGE, MODE_APPROVE };
static Mode mode = MODE_IDLE;
static uint32_t approveDeadline = 0;

static void requestAndShowUsage() {
  if (g_daemonBase.length() == 0) {
    showNotify("worried", "まだつながってないのだ");
    usageUntil = millis() + 2500;
    mode = MODE_USAGE;
    return;
  }
  HTTPClient http;
  http.begin(g_daemonBase + "/usage");
  http.setTimeout(4000);
  int code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (deserializeJson(doc, http.getString()) == DeserializationError::Ok) {
      showUsage(doc["percent"] | 0, doc["resetMin"] | 0);
    } else {
      showNotify("worried", "へんじがへんなのだ");
    }
  } else {
    showNotify("worried", "しゅとくしっぱいなのだ");
  }
  http.end();
  usageUntil = millis() + 5000;
  mode = MODE_USAGE;
}

static void postApproveResult(const char* decision) {
  if (g_daemonBase.length() == 0) return;
  HTTPClient http;
  http.begin(g_daemonBase + "/approve_result/" + g_approve.id);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);
  http.POST(String("{\"decision\":\"") + decision + "\"}");
  http.end();
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
  mode = MODE_IDLE;
}

void loop() {
  M5.update();
  uint32_t now = millis();

  // 承認要求の受信（最優先で画面を奪う）
  if (g_approve.ready) {
    g_approve.ready = false;
    showApprove(g_approve.title, g_approve.detail);
    mode = MODE_APPROVE;
    approveDeadline = now + 30000;
  }

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) gesture.down(now, t.x, t.y);
  if (t.wasReleased()) {
    Gesture g = gesture.up(now, t.x, t.y);
    if (mode == MODE_APPROVE) {
      if (g == GESTURE_TAP || g == GESTURE_DOUBLETAP) {
        postApproveResult("allow");
        showNotify("happy", "ゴーサインなのだ！");
        delay(1500);
        showIdle();
        mode = MODE_IDLE;
      } else if (g == GESTURE_SWIPE) {
        postApproveResult("deny");
        showNotify("worried", "やめておくのだ");
        delay(1500);
        showIdle();
        mode = MODE_IDLE;
      }
    } else if (g == GESTURE_DOUBLETAP && mode == MODE_IDLE) {
      requestAndShowUsage();
    }
  }

  // 通知（承認中は割り込ませない）
  if (g_notify.ready && mode != MODE_APPROVE) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) playWav(g_notify.wav, g_notify.wavLen);
    delay(3000);
    showIdle();
    mode = MODE_IDLE;
    usageUntil = 0;
  }

  // USAGE自動復帰
  if (mode == MODE_USAGE && usageUntil != 0 && now > usageUntil) {
    usageUntil = 0;
    showIdle();
    mode = MODE_IDLE;
  }

  // 承認タイムアウト（daemon側もtimeout→ask）
  if (mode == MODE_APPROVE && now > approveDeadline) {
    showIdle();
    mode = MODE_IDLE;
  }

  delay(10);
}
```

- [ ] **Step 2: ビルド＆書き込み**

Run: `cd firmware && pio run -e core2 -t upload --upload-port /dev/cu.usbserial-54780284771`
Expected: SUCCESS

- [ ] **Step 3: 実機スモーク（daemon直叩き）**

daemon起動中で、承認要求を手動で投げてM5に出るか確認:
```bash
ID=$(curl -s -X POST localhost:4920/approve -H 'Content-Type: application/json' -d '{"tool":"Bash","command":"ls -la /tmp"}' | python3 -c "import sys,json;print(json.load(sys.stdin)['id'])")
echo "id=$ID  → M5に『CLAUDE Bash OK?』が出るはず。タップ or スワイプして:"
sleep 6
curl -s localhost:4920/approve_result/$ID
```
Expected: M5に承認画面表示→タップで `{"decision":"allow"}`、スワイプで `{"decision":"deny"}`。

- [ ] **Step 4: コミット**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): approval mode (tap=allow / swipe=deny)"
```

---

## Task 9: PreToolUse フック

**Files:** Create `hooks/pretooluse.sh`, Test `hooks/pretooluse.test.sh`

- [ ] **Step 1: 失敗するテスト** — `hooks/pretooluse.test.sh`

スタブdaemン: `POST /approve`→`{"id":"t1"}`、`GET /approve_result/t1`→`{"decision":"allow"}`。フックが allow JSON を出すか検証。

```bash
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'kill "${SRV_PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT

python3 - 14921 <<'PY' &
import sys, json, http.server
port = int(sys.argv[1])
class H(http.server.BaseHTTPRequestHandler):
    def _send(self, obj):
        self.send_response(200); self.send_header("Content-Type","application/json"); self.end_headers()
        self.wfile.write(json.dumps(obj).encode())
    def do_POST(self):
        n=int(self.headers.get("Content-Length",0)); self.rfile.read(n)
        if self.path == "/approve": self._send({"id":"t1"})
        else: self._send({"ok":True})
    def do_GET(self):
        self._send({"decision":"allow"})
    def log_message(self,*a): pass
http.server.HTTPServer(("127.0.0.1",port),H).serve_forever()
PY
SRV_PID=$!
sleep 0.5

out=$(echo '{"tool_name":"Bash","tool_input":{"command":"ls"}}' | ZUNDA_DAEMON_URL="http://127.0.0.1:14921" bash "$HERE/pretooluse.sh")
echo "$out" | grep -q '"permissionDecision":"allow"' || { echo "FAIL: allowが出ない: $out"; exit 1; }
echo "PASS"
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `bash hooks/pretooluse.test.sh`
Expected: FAIL（`pretooluse.sh` 未作成）

- [ ] **Step 3: 実装** — `hooks/pretooluse.sh`

```bash
#!/usr/bin/env bash
# Claude Code PreToolUse hook: ロボット上で許可/拒否を仰ぐ。
# daemon不達・タイムアウト時は何も出力せず exit 0（=通常の許可フロー/ask）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"

input=$(cat)
tool=$(printf '%s' "$input" | python3 -c "import sys,json;print(json.load(sys.stdin).get('tool_name',''))" 2>/dev/null || echo "")
cmd=$(printf '%s' "$input" | python3 -c "import sys,json;d=json.load(sys.stdin);print((d.get('tool_input') or {}).get('command',''))" 2>/dev/null || echo "")

payload=$(python3 -c "import json,sys;print(json.dumps({'tool':sys.argv[1],'command':sys.argv[2]}))" "$tool" "$cmd" 2>/dev/null) || exit 0
resp=$(curl -s -m 5 -X POST "$DAEMON_URL/approve" -H 'Content-Type: application/json' -d "$payload") || exit 0
id=$(printf '%s' "$resp" | python3 -c "import sys,json;print(json.load(sys.stdin).get('id',''))" 2>/dev/null || echo "")
[ -z "$id" ] && exit 0

for _ in $(seq 1 60); do
  d=$(curl -s -m 3 "$DAEMON_URL/approve_result/$id" | python3 -c "import sys,json;print(json.load(sys.stdin).get('decision',''))" 2>/dev/null || echo "")
  case "$d" in
    allow)
      printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow","permissionDecisionReason":"stackchan tap"}}'
      exit 0;;
    deny)
      printf '%s\n' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"stackchan swipe"}}'
      exit 0;;
    timeout|unknown)
      exit 0;;
  esac
  sleep 0.5
done
exit 0
```

- [ ] **Step 4: 実行権限＆テスト合格**

Run: `chmod +x hooks/pretooluse.sh hooks/pretooluse.test.sh && bash hooks/pretooluse.test.sh`
Expected: `PASS`

- [ ] **Step 5: コミット**

```bash
git add hooks/pretooluse.sh hooks/pretooluse.test.sh
git commit -m "feat(hooks): PreToolUse approval via stackchan"
```

---

## Task 10: PreToolUseフック登録 ＋ E2E

**Files:** Modify `~/.claude/settings.json`（ユーザ操作）

- [ ] **Step 1: PreToolUseフックを登録（ユーザ操作）**

`~/.claude/settings.json` の `hooks` に `PreToolUse`（matcher=Bash）を追記。ハーネスがAIによる settings.json 自動編集をブロックするため、ユーザが次を `!` 実行:
```
!python3 - <<'PY'
import json,os
p=os.path.expanduser("~/.claude/settings.json")
d=json.load(open(p))
d.setdefault("hooks",{})["PreToolUse"]=[{"matcher":"Bash","hooks":[{"type":"command","command":"bash /Users/ka-murata/Documents/dotlog/claudecode_push_display/hooks/pretooluse.sh"}]}]
json.dump(d,open(p,"w"),indent=2,ensure_ascii=False)
print("PreToolUse registered")
PY
```
登録後、**新しいClaude Codeセッション**で有効化。

- [ ] **Step 2: E2E 手動検証**

前提: daemon起動中、M5疎通、PreToolUse登録済み・新セッション。
手順: 新セッションのClaudeに Bash コマンド（例: `ls`）を実行させる。
Expected: 実行前にM5へ「CLAUDE Bash OK?」表示→**タップで実行が進む / スワイプで拒否される**。無操作30秒で通常の許可プロンプトにフォールバック。

- [ ] **Step 3: README更新（Phase 2分）**

`README.md` の「Phase 2以降」記述を「実装済み」に更新（使用率表示・残量警告・タップ承認）。コミット:
```bash
git add README.md
git commit -m "docs: mark Phase 2 (usage/warning/approval) as implemented"
```

---

## 完了の定義（M5）
- `cd daemon && npm test` 全PASS（approvals/wifi/config/server 追加分）、`npm run build` 成功
- `bash hooks/pretooluse.test.sh` PASS
- 実機: 承認要求→M5表示→タップ=allow/スワイプ=deny（Task8 Step3）
- E2E: 新セッションでBash実行時にロボット承認（Task10 Step2、ユーザ操作）

## Phase 2 完了後
- ブランチ仕上げ（finishing-a-development-branch: main へマージ / PR）
- 任意: 立ち絵JPG投入（assets/zundamon README手順）、VOICEVOX起動で本物のずんだ声、USBトランスポート(M6)
