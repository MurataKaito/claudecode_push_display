# トークン残量お知らせずんだもん — Phase 1 実装計画（足場→疎通→タスク完了通知）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Claude Code がタスクを終えると、机上の M5Stack Core2 のずんだもんが笑顔になり、VOICEVOX音声をCore2スピーカーで再生して「おしごと、おわったのだ！」と知らせる。

**Architecture:** Mac常駐の母艦デーモン(Node/TS)が Claude Code の `Stop` フックから `POST /event` を受け、VOICEVOX(`:50021`)でWAVを生成し、WiFi HTTP(`stackchan.local`)経由でM5へ `POST /notify` する。M5(PlatformIO/C++)は AsyncWebServer でWAVバイナリを受信し、表情を切り替えてスピーカー再生する。通信層は `Transport` IFで抽象化し、後でUSBへ差し替え可能。

**Tech Stack:** Node.js v22 + TypeScript + Fastify + vitest / bash + curl / PlatformIO(arduino, M5Unified, ESPAsyncWebServer, ArduinoJson) + Unity(native test) / VOICEVOX(speaker=3 ずんだもん)。

**前提:** リポジトリは `feature/zundamon-push-display` ブランチ。設計は `docs/superpowers/specs/2026-06-04-claudecode-push-display-design.md`。Phase 1 のスコープは M0〜M2 のみ（使用率表示・残量警告・承認・USBトランスポートは後続フェーズ）。

---

## ファイル構成（Phase 1 で作成）

```
daemon/
  package.json, tsconfig.json, vitest.config.ts
  src/
    config.ts            # 環境変数から設定をロード
    serif.ts             # イベント→ずんだもんセリフ(expr+text)
    voicevox.ts          # VOICEVOX呼び出し → WAV(Buffer)
    transport/index.ts   # Transport IF + Notification型
    transport/wifi.ts     # WiFiTransport(fetch→M5 /notify)
    server.ts            # Fastify createServer({transport, synth}) /event
    index.ts             # 起動エントリ（設定→依存配線→listen）
  src/*.test.ts          # 各モジュールの単体/統合テスト
hooks/
  stop.sh               # Stopフック: daemon /event を叩く
  stop.test.sh          # スタブdaemonに対する挙動テスト
firmware/
  platformio.ini
  include/secrets.h.example
  lib/wavparse/{wavparse.h, wavparse.cpp}   # 純粋ロジック(両env共有)
  test/test_wavparse/test_wavparse.cpp      # native Unityテスト
  src/{app_state.h, audio.h, audio.cpp, display.h, display.cpp, net.h, net.cpp, main.cpp}
assets/zundamon/
  README.md             # 立ち絵の入手・変換・LittleFS書込み手順
```

各ファイルは単一責務：`config`=設定、`serif`=文言、`voicevox`=音声合成、`transport/*`=送出、`server`=HTTP受け口。ファームは `wavparse`=解析(テスト可能), `audio`=再生, `display`=描画, `net`=通信, `app_state`=共有状態, `main`=配線。

---

## Task 1: デーモンの足場と設定モジュール

**Files:**
- Create: `daemon/package.json`
- Create: `daemon/tsconfig.json`
- Create: `daemon/vitest.config.ts`
- Create: `daemon/src/config.ts`
- Test: `daemon/src/config.test.ts`

- [ ] **Step 1: package.json を作成**

```json
{
  "name": "zundamon-daemon",
  "version": "0.1.0",
  "private": true,
  "type": "module",
  "scripts": {
    "dev": "tsx src/index.ts",
    "build": "tsc -p tsconfig.json",
    "start": "node dist/index.js",
    "test": "vitest run"
  },
  "dependencies": {
    "fastify": "^4.28.1"
  },
  "devDependencies": {
    "@types/node": "^22.5.0",
    "tsx": "^4.16.2",
    "typescript": "^5.5.4",
    "vitest": "^2.0.5"
  }
}
```

- [ ] **Step 2: tsconfig.json と vitest.config.ts を作成**

`daemon/tsconfig.json`:
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ES2022",
    "moduleResolution": "Bundler",
    "outDir": "dist",
    "rootDir": "src",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "types": ["node"]
  },
  "include": ["src"]
}
```

`daemon/vitest.config.ts`:
```ts
import { defineConfig } from "vitest/config";

export default defineConfig({
  test: { environment: "node", include: ["src/**/*.test.ts"] },
});
```

- [ ] **Step 3: 失敗するテストを書く** — `daemon/src/config.test.ts`

```ts
import { describe, it, expect } from "vitest";
import { loadConfig } from "./config.js";

describe("loadConfig", () => {
  it("空envなら既定値を返す", () => {
    const c = loadConfig({});
    expect(c).toEqual({
      port: 4920,
      voicevoxUrl: "http://127.0.0.1:50021",
      speakerId: 3,
      m5Url: "http://stackchan.local",
    });
  });

  it("env で上書きできる", () => {
    const c = loadConfig({
      ZUNDA_PORT: "5000",
      ZUNDA_VOICEVOX_URL: "http://localhost:60000",
      ZUNDA_SPEAKER_ID: "1",
      ZUNDA_M5_URL: "http://192.168.0.5",
    });
    expect(c.port).toBe(5000);
    expect(c.voicevoxUrl).toBe("http://localhost:60000");
    expect(c.speakerId).toBe(1);
    expect(c.m5Url).toBe("http://192.168.0.5");
  });
});
```

- [ ] **Step 4: テストを実行して失敗を確認**

Run: `cd daemon && npm install && npm test`
Expected: FAIL（`./config.js` が存在せず import エラー）

- [ ] **Step 5: 最小実装** — `daemon/src/config.ts`

```ts
export type Config = {
  port: number;
  voicevoxUrl: string;
  speakerId: number;
  m5Url: string;
};

export function loadConfig(env: Record<string, string | undefined> = process.env): Config {
  return {
    port: Number(env.ZUNDA_PORT ?? 4920),
    voicevoxUrl: env.ZUNDA_VOICEVOX_URL ?? "http://127.0.0.1:50021",
    speakerId: Number(env.ZUNDA_SPEAKER_ID ?? 3),
    m5Url: env.ZUNDA_M5_URL ?? "http://stackchan.local",
  };
}
```

- [ ] **Step 6: テスト合格を確認**

Run: `cd daemon && npm test`
Expected: PASS（2 tests）

- [ ] **Step 7: コミット**

```bash
git add daemon/package.json daemon/tsconfig.json daemon/vitest.config.ts daemon/src/config.ts daemon/src/config.test.ts daemon/package-lock.json
git commit -m "feat(daemon): scaffold + config loader"
```

---

## Task 2: セリフ生成（serif.ts）

**Files:**
- Create: `daemon/src/serif.ts`
- Test: `daemon/src/serif.test.ts`

- [ ] **Step 1: 失敗するテストを書く** — `daemon/src/serif.test.ts`

```ts
import { describe, it, expect } from "vitest";
import { serifFor } from "./serif.js";

describe("serifFor", () => {
  it("done は笑顔で完了セリフ", () => {
    expect(serifFor({ type: "done" })).toEqual({
      expr: "happy",
      text: "おしごと、おわったのだ！",
    });
  });
});
```

- [ ] **Step 2: テストを実行して失敗を確認**

Run: `cd daemon && npm test -- serif`
Expected: FAIL（`./serif.js` 未作成）

- [ ] **Step 3: 最小実装** — `daemon/src/serif.ts`

```ts
export type Expr = "normal" | "happy" | "worried" | "surprised";
export type AppEvent = { type: "done"; summary?: string };
export type Serif = { expr: Expr; text: string };

export function serifFor(event: AppEvent): Serif {
  switch (event.type) {
    case "done":
      return { expr: "happy", text: "おしごと、おわったのだ！" };
    default:
      return { expr: "normal", text: "なのだ" };
  }
}
```

- [ ] **Step 4: テスト合格を確認**

Run: `cd daemon && npm test -- serif`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/serif.ts daemon/src/serif.test.ts
git commit -m "feat(daemon): serif generator for events"
```

---

## Task 3: VOICEVOX クライアント（voicevox.ts）

**Files:**
- Create: `daemon/src/voicevox.ts`
- Test: `daemon/src/voicevox.test.ts`

- [ ] **Step 1: 失敗するテストを書く** — `daemon/src/voicevox.test.ts`

`fetchImpl` を注入して VOICEVOX を呼ばずに検証する。

```ts
import { describe, it, expect, vi } from "vitest";
import { synth } from "./voicevox.js";

describe("synth", () => {
  it("audio_query→synthesis の順に呼び、WAVのBufferを返す", async () => {
    const calls: string[] = [];
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      calls.push(url);
      if (url.includes("/audio_query")) {
        return new Response(JSON.stringify({ accent_phrases: [] }), {
          status: 200,
          headers: { "Content-Type": "application/json" },
        });
      }
      // synthesis: WAVバイト(先頭RIFF)を返す
      return new Response(new Uint8Array([0x52, 0x49, 0x46, 0x46]), { status: 200 });
    }) as unknown as typeof fetch;

    const wav = await synth("こんにちは", {
      baseUrl: "http://vv.test",
      speakerId: 3,
      fetchImpl,
    });

    expect(Buffer.isBuffer(wav)).toBe(true);
    expect(wav.subarray(0, 4).toString("latin1")).toBe("RIFF");
    expect(calls[0]).toContain("/audio_query?");
    expect(calls[0]).toContain("speaker=3");
    expect(calls[1]).toContain("/synthesis?speaker=3");
  });

  it("audio_query が失敗したら例外", async () => {
    const fetchImpl = vi.fn(async () => new Response("", { status: 500 })) as unknown as typeof fetch;
    await expect(
      synth("x", { baseUrl: "http://vv.test", speakerId: 3, fetchImpl }),
    ).rejects.toThrow(/audio_query/);
  });
});
```

- [ ] **Step 2: テストを実行して失敗を確認**

Run: `cd daemon && npm test -- voicevox`
Expected: FAIL（`./voicevox.js` 未作成）

- [ ] **Step 3: 最小実装** — `daemon/src/voicevox.ts`

```ts
export type SynthOptions = {
  baseUrl: string;
  speakerId: number;
  fetchImpl?: typeof fetch;
};

export async function synth(text: string, opts: SynthOptions): Promise<Buffer> {
  const f = opts.fetchImpl ?? fetch;
  const q = new URLSearchParams({ text, speaker: String(opts.speakerId) });
  const queryRes = await f(`${opts.baseUrl}/audio_query?${q.toString()}`, { method: "POST" });
  if (!queryRes.ok) throw new Error(`audio_query failed: ${queryRes.status}`);
  const query = await queryRes.json();
  const synthRes = await f(`${opts.baseUrl}/synthesis?speaker=${opts.speakerId}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(query),
  });
  if (!synthRes.ok) throw new Error(`synthesis failed: ${synthRes.status}`);
  return Buffer.from(await synthRes.arrayBuffer());
}
```

- [ ] **Step 4: テスト合格を確認**

Run: `cd daemon && npm test -- voicevox`
Expected: PASS（2 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/voicevox.ts daemon/src/voicevox.test.ts
git commit -m "feat(daemon): VOICEVOX client (audio_query+synthesis)"
```

---

## Task 4: 通信層（Transport IF と WiFiTransport）

**Files:**
- Create: `daemon/src/transport/index.ts`
- Create: `daemon/src/transport/wifi.ts`
- Test: `daemon/src/transport/wifi.test.ts`

- [ ] **Step 1: IFと型を作成** — `daemon/src/transport/index.ts`

```ts
import type { Expr } from "../serif.js";

export type Notification = { expr: Expr; text: string; wav: Buffer };

export interface Transport {
  notify(n: Notification): Promise<void>;
}
```

- [ ] **Step 2: 失敗するテストを書く** — `daemon/src/transport/wifi.test.ts`

```ts
import { describe, it, expect, vi } from "vitest";
import { WiFiTransport } from "./wifi.js";

describe("WiFiTransport", () => {
  it("M5 /notify に expr/text クエリ＋WAVボディでPOSTする", async () => {
    let seenUrl = "";
    let seenBody: Uint8Array | undefined;
    let seenContentType = "";
    const fetchImpl = vi.fn(async (url: string, init?: RequestInit) => {
      seenUrl = url;
      seenBody = init?.body as Uint8Array;
      seenContentType = (init?.headers as Record<string, string>)["Content-Type"];
      return new Response("{\"ok\":true}", { status: 200 });
    }) as unknown as typeof fetch;

    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await t.notify({ expr: "happy", text: "やったのだ", wav: Buffer.from([1, 2, 3]) });

    expect(seenUrl).toContain("http://stackchan.local/notify?");
    expect(seenUrl).toContain("expr=happy");
    expect(decodeURIComponent(seenUrl)).toContain("text=やったのだ");
    expect(seenContentType).toBe("audio/wav");
    expect(Array.from(seenBody!)).toEqual([1, 2, 3]);
  });

  it("非2xxなら例外", async () => {
    const fetchImpl = vi.fn(async () => new Response("", { status: 503 })) as unknown as typeof fetch;
    const t = new WiFiTransport("http://stackchan.local", fetchImpl);
    await expect(
      t.notify({ expr: "normal", text: "x", wav: Buffer.from([0]) }),
    ).rejects.toThrow(/notify failed/);
  });
});
```

- [ ] **Step 3: テストを実行して失敗を確認**

Run: `cd daemon && npm test -- wifi`
Expected: FAIL（`./wifi.js` 未作成）

- [ ] **Step 4: 最小実装** — `daemon/src/transport/wifi.ts`

```ts
import type { Transport, Notification } from "./index.js";

export class WiFiTransport implements Transport {
  constructor(
    private m5Url: string,
    private fetchImpl: typeof fetch = fetch,
  ) {}

  async notify(n: Notification): Promise<void> {
    const q = new URLSearchParams({ expr: n.expr, text: n.text });
    const res = await this.fetchImpl(`${this.m5Url}/notify?${q.toString()}`, {
      method: "POST",
      headers: { "Content-Type": "audio/wav" },
      body: n.wav,
    });
    if (!res.ok) throw new Error(`notify failed: ${res.status}`);
  }
}
```

- [ ] **Step 5: テスト合格を確認**

Run: `cd daemon && npm test -- wifi`
Expected: PASS（2 tests）

- [ ] **Step 6: コミット**

```bash
git add daemon/src/transport/index.ts daemon/src/transport/wifi.ts daemon/src/transport/wifi.test.ts
git commit -m "feat(daemon): Transport interface + WiFiTransport"
```

---

## Task 5: HTTPサーバ（/event 受け口）

**Files:**
- Create: `daemon/src/server.ts`
- Test: `daemon/src/server.test.ts`

- [ ] **Step 1: 失敗するテストを書く** — `daemon/src/server.test.ts`

モックの transport と synth を注入し、`/event` の配線を検証する（Fastify の `inject` 使用）。

```ts
import { describe, it, expect, vi } from "vitest";
import { createServer } from "./server.js";
import type { Notification, Transport } from "./transport/index.js";

describe("createServer /event", () => {
  it("done を受けると synth→transport.notify を呼び 200 を返す", async () => {
    const notified: Notification[] = [];
    const transport: Transport = {
      notify: async (n) => {
        notified.push(n);
      },
    };
    const synth = vi.fn(async (_text: string) => Buffer.from("WAVDATA"));

    const app = createServer({ transport, synth });
    const res = await app.inject({
      method: "POST",
      url: "/event",
      payload: { type: "done" },
    });

    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual({ ok: true });
    expect(synth).toHaveBeenCalledWith("おしごと、おわったのだ！");
    expect(notified).toHaveLength(1);
    expect(notified[0].expr).toBe("happy");
    expect(notified[0].text).toBe("おしごと、おわったのだ！");
    expect(notified[0].wav.toString()).toBe("WAVDATA");
    await app.close();
  });
});
```

- [ ] **Step 2: テストを実行して失敗を確認**

Run: `cd daemon && npm test -- server`
Expected: FAIL（`./server.js` 未作成）

- [ ] **Step 3: 最小実装** — `daemon/src/server.ts`

```ts
import Fastify, { type FastifyInstance } from "fastify";
import type { Transport } from "./transport/index.js";
import { serifFor, type AppEvent } from "./serif.js";

export type ServerDeps = {
  transport: Transport;
  synth: (text: string) => Promise<Buffer>;
};

export function createServer(deps: ServerDeps): FastifyInstance {
  const app = Fastify({ logger: false });

  app.post("/event", async (req, reply) => {
    const event = req.body as AppEvent;
    const serif = serifFor(event);
    const wav = await deps.synth(serif.text);
    await deps.transport.notify({ expr: serif.expr, text: serif.text, wav });
    reply.send({ ok: true });
  });

  return app;
}
```

- [ ] **Step 4: テスト合格を確認**

Run: `cd daemon && npm test -- server`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/server.ts daemon/src/server.test.ts
git commit -m "feat(daemon): HTTP server with /event endpoint"
```

---

## Task 6: 起動エントリ（index.ts）と手動スモーク

**Files:**
- Create: `daemon/src/index.ts`

- [ ] **Step 1: エントリを作成** — `daemon/src/index.ts`

```ts
import { loadConfig } from "./config.js";
import { createServer } from "./server.js";
import { WiFiTransport } from "./transport/wifi.js";
import { synth } from "./voicevox.js";

const cfg = loadConfig();
const transport = new WiFiTransport(cfg.m5Url);
const app = createServer({
  transport,
  synth: (text) => synth(text, { baseUrl: cfg.voicevoxUrl, speakerId: cfg.speakerId }),
});

app
  .listen({ port: cfg.port, host: "0.0.0.0" })
  .then(() =>
    console.log(`zundamon daemon: :${cfg.port}  M5=${cfg.m5Url}  VOICEVOX=${cfg.voicevoxUrl}`),
  )
  .catch((e) => {
    console.error(e);
    process.exit(1);
  });
```

- [ ] **Step 2: 全テストとビルドを確認**

Run: `cd daemon && npm test && npm run build`
Expected: 全テスト PASS、`dist/` 生成、型エラー無し

- [ ] **Step 3: 手動スモーク（VOICEVOX起動済みで、M5は未接続でも可）**

VOICEVOX を起動（`http://127.0.0.1:50021/version` が応答）した上で：

Run（別ターミナル）: `cd daemon && npm run dev`
Run: `curl -s -X POST localhost:4920/event -H 'Content-Type: application/json' -d '{"type":"done"}'`
Expected:
- M5未接続なら `notify failed`/接続エラーがdaemonログに出る（VOICEVOX合成までは到達している＝想定内）。
- VOICEVOX未起動なら `audio_query failed`。
- ※ M5連携の正常確認は Task 13 で実施。ここでは「合成まで到達」を確認できればよい。

- [ ] **Step 4: コミット**

```bash
git add daemon/src/index.ts
git commit -m "feat(daemon): entrypoint wiring config+server+transport+voicevox"
```

---

## Task 7: Stop フック（hooks/stop.sh）

**Files:**
- Create: `hooks/stop.sh`
- Test: `hooks/stop.test.sh`

- [ ] **Step 1: 失敗するテストを書く** — `hooks/stop.test.sh`

Python の簡易スタブで `/event` への POST ボディをファイルに記録し、`stop.sh` がそれを叩くか検証する。

```bash
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'kill "${SRV_PID:-0}" 2>/dev/null || true; rm -rf "$TMP"' EXIT

# 受信ボディを $TMP/body に書くスタブサーバ (port 14920)
python3 - "$TMP/body" 14920 <<'PY' &
import sys, http.server
out, port = sys.argv[1], int(sys.argv[2])
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        open(out, "wb").write(self.rfile.read(n))
        self.send_response(200); self.end_headers(); self.wfile.write(b"{}")
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
SRV_PID=$!
sleep 0.5

echo '{"hook_event_name":"Stop"}' | ZUNDA_DAEMON_URL="http://127.0.0.1:14920" bash "$HERE/stop.sh"
sleep 0.3

grep -q '"type":"done"' "$TMP/body" || { echo "FAIL: body にdoneイベントが無い"; cat "$TMP/body"; exit 1; }
echo "PASS"
```

- [ ] **Step 2: テストを実行して失敗を確認**

Run: `bash hooks/stop.test.sh`
Expected: FAIL（`hooks/stop.sh` 未作成 → bash がスクリプトを開けずエラー）

- [ ] **Step 3: 最小実装** — `hooks/stop.sh`

```bash
#!/usr/bin/env bash
# Claude Code Stop hook: notify the zundamon daemon that a task finished.
# 失敗してもClaudeを止めない（常に exit 0）。
DAEMON_URL="${ZUNDA_DAEMON_URL:-http://127.0.0.1:4920}"
# フックのstdin(JSON)は読み捨てる
cat >/dev/null 2>&1 || true
curl -s -m 3 -X POST "$DAEMON_URL/event" \
  -H 'Content-Type: application/json' \
  -d '{"type":"done"}' >/dev/null 2>&1 || true
exit 0
```

- [ ] **Step 4: 実行権限を付与しテスト合格を確認**

Run: `chmod +x hooks/stop.sh hooks/stop.test.sh && bash hooks/stop.test.sh`
Expected: `PASS`

- [ ] **Step 5: コミット**

```bash
git add hooks/stop.sh hooks/stop.test.sh
git commit -m "feat(hooks): Stop hook posts done event to daemon"
```

---

## Task 8: ファームの足場（platformio.ini, secrets, native env）

**Files:**
- Create: `firmware/platformio.ini`
- Create: `firmware/include/secrets.h.example`
- Create: `firmware/.gitignore`

- [ ] **Step 1: platformio.ini を作成**

```ini
[platformio]
default_envs = core2

[env:core2]
platform = espressif32@^6.7.0
board = m5stack-core2
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5Unified@^0.2.2
    bblanchon/ArduinoJson@^7.1.0
    mathieucarbou/ESPAsyncWebServer@^3.3.12
build_flags = -DCORE_DEBUG_LEVEL=2

[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++17
```

- [ ] **Step 2: secrets テンプレートと .gitignore を作成**

`firmware/include/secrets.h.example`:
```cpp
#pragma once
// このファイルを secrets.h にコピーして自分のWiFi情報を記入する。
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-pass"
```

`firmware/.gitignore`:
```
.pio/
include/secrets.h
```

- [ ] **Step 3: PlatformIO が認識するか確認（まだソース無しでOK）**

Run: `cd firmware && pio pkg install -e native`
Expected: native 環境の依存解決が成功（ビルド対象が無くてもエラーにならない）

- [ ] **Step 4: コミット**

```bash
git add firmware/platformio.ini firmware/include/secrets.h.example firmware/.gitignore
git commit -m "chore(firmware): PlatformIO scaffold (core2 + native test env)"
```

---

## Task 9: WAVパーサ（純粋ロジック＋native Unityテスト）

**Files:**
- Create: `firmware/lib/wavparse/wavparse.h`
- Create: `firmware/lib/wavparse/wavparse.cpp`
- Test: `firmware/test/test_wavparse/test_wavparse.cpp`

- [ ] **Step 1: ヘッダを作成** — `firmware/lib/wavparse/wavparse.h`

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

struct WavInfo {
  uint32_t sampleRate;
  uint16_t channels;
  uint16_t bitsPerSample;
  size_t dataOffset;  // PCM本体の先頭オフセット
  size_t dataLen;     // PCM本体のバイト数
};

// RIFFチャンクを走査して fmt と data を見つける。成功時 true。
bool parseWav(const uint8_t* buf, size_t len, WavInfo* out);
```

- [ ] **Step 2: 失敗するテストを書く** — `firmware/test/test_wavparse/test_wavparse.cpp`

```cpp
#include <unity.h>
#include "wavparse.h"

// 24kHz/16bit/mono、PCM 4バイト(2サンプル)の最小WAV。
static const uint8_t WAV[] = {
  'R','I','F','F', 0x28,0,0,0, 'W','A','V','E',
  'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
  0xC0,0x5D,0,0,            // sampleRate = 24000
  0x80,0xBB,0,0,            // byteRate
  2,0, 16,0,               // blockAlign=2, bitsPerSample=16
  'd','a','t','a', 4,0,0,0, 0x11,0x22,0x33,0x44
};

void test_parses_fmt_and_data(void) {
  WavInfo info;
  TEST_ASSERT_TRUE(parseWav(WAV, sizeof(WAV), &info));
  TEST_ASSERT_EQUAL_UINT32(24000, info.sampleRate);
  TEST_ASSERT_EQUAL_UINT16(1, info.channels);
  TEST_ASSERT_EQUAL_UINT16(16, info.bitsPerSample);
  TEST_ASSERT_EQUAL_UINT32(4, info.dataLen);
  TEST_ASSERT_EQUAL_UINT8(0x22, WAV[info.dataOffset + 1]);
}

void test_rejects_non_riff(void) {
  const uint8_t bad[12] = {0};
  WavInfo info;
  TEST_ASSERT_FALSE(parseWav(bad, sizeof(bad), &info));
}

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_fmt_and_data);
  RUN_TEST(test_rejects_non_riff);
  return UNITY_END();
}
```

- [ ] **Step 3: テストを実行して失敗を確認**

Run: `cd firmware && pio test -e native`
Expected: FAIL（`parseWav` 未定義でリンクエラー）

- [ ] **Step 4: 最小実装** — `firmware/lib/wavparse/wavparse.cpp`

```cpp
#include "wavparse.h"
#include <string.h>

static uint32_t rdU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rdU16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool parseWav(const uint8_t* buf, size_t len, WavInfo* out) {
  if (!buf || !out || len < 12) return false;
  if (memcmp(buf, "RIFF", 4) != 0) return false;
  if (memcmp(buf + 8, "WAVE", 4) != 0) return false;

  size_t pos = 12;
  bool haveFmt = false, haveData = false;
  while (pos + 8 <= len) {
    const uint8_t* id = buf + pos;
    uint32_t sz = rdU32(buf + pos + 4);
    size_t body = pos + 8;
    if (memcmp(id, "fmt ", 4) == 0 && body + 16 <= len) {
      out->channels = rdU16(buf + body + 2);
      out->sampleRate = rdU32(buf + body + 4);
      out->bitsPerSample = rdU16(buf + body + 14);
      haveFmt = true;
    } else if (memcmp(id, "data", 4) == 0) {
      out->dataOffset = body;
      out->dataLen = (body + sz <= len) ? (size_t)sz : (len - body);
      haveData = true;
    }
    pos = body + sz + (sz & 1);  // チャンクは偶数境界
  }
  return haveFmt && haveData;
}
```

- [ ] **Step 5: テスト合格を確認**

Run: `cd firmware && pio test -e native`
Expected: PASS（2 tests）

- [ ] **Step 6: コミット**

```bash
git add firmware/lib/wavparse/wavparse.h firmware/lib/wavparse/wavparse.cpp firmware/test/test_wavparse/test_wavparse.cpp
git commit -m "feat(firmware): WAV parser with native unit tests"
```

---

## Task 10: 音声再生（audio.cpp）

**Files:**
- Create: `firmware/src/audio.h`
- Create: `firmware/src/audio.cpp`

> 注: 実機専用（M5Unified依存）。native テストは Task 9 のパーサで担保済み。本タスクは Task 13 の実機検証で動作確認する。

- [ ] **Step 1: ヘッダを作成** — `firmware/src/audio.h`

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

void audioBegin();
// WAVバイト列を解析してスピーカー再生する。成功時 true。
// data はこの関数の戻り後も呼び出し側が再生終了まで保持すること。
bool playWav(const uint8_t* data, size_t len);
```

- [ ] **Step 2: 実装** — `firmware/src/audio.cpp`

```cpp
#include "audio.h"
#include <M5Unified.h>
#include "wavparse.h"

void audioBegin() {
  auto cfg = M5.Speaker.config();
  cfg.sample_rate = 24000;  // VOICEVOX 既定
  M5.Speaker.config(cfg);
  M5.Speaker.begin();
  M5.Speaker.setVolume(180);
}

bool playWav(const uint8_t* data, size_t len) {
  WavInfo info;
  if (!parseWav(data, len, &info)) return false;
  if (info.bitsPerSample != 16) return false;
  const int16_t* pcm = reinterpret_cast<const int16_t*>(data + info.dataOffset);
  const size_t samples = info.dataLen / 2;
  const bool stereo = info.channels > 1;
  // 引数: (data, len, sample_rate, stereo, repeat, channel=-1, stop_current=true)
  return M5.Speaker.playRaw(pcm, samples, info.sampleRate, stereo, 1, -1, true);
}
```

- [ ] **Step 3: コミット（ビルドは Task 13 で main と一緒に確認）**

```bash
git add firmware/src/audio.h firmware/src/audio.cpp
git commit -m "feat(firmware): WAV playback via M5 speaker"
```

---

## Task 11: 表示（display.cpp）

**Files:**
- Create: `firmware/src/display.h`
- Create: `firmware/src/display.cpp`

> 注: 実機専用。立ち絵JPGが LittleFS に在れば表示、無ければ色付きプレースホルダ顔を描画（end-to-end検証をアセット待ちにしないため）。テキスト枠は常に重畳。

- [ ] **Step 1: ヘッダを作成** — `firmware/src/display.h`

```cpp
#pragma once
#include <Arduino.h>

void displayBegin();
void showIdle();
// expr: "normal" | "happy" | "worried" | "surprised"
void showNotify(const String& expr, const String& text);
```

- [ ] **Step 2: 実装** — `firmware/src/display.cpp`

```cpp
#include "display.h"
#include <M5Unified.h>
#include <LittleFS.h>

static uint16_t exprColor(const String& expr) {
  if (expr == "happy") return TFT_GREEN;
  if (expr == "worried") return TFT_ORANGE;
  if (expr == "surprised") return TFT_CYAN;
  return TFT_DARKGREEN;  // normal
}

// 立ち絵があれば描画。無ければ簡易顔。
static void drawFace(const String& expr) {
  String path = "/" + expr + ".jpg";
  if (LittleFS.exists(path)) {
    M5.Display.drawJpgFile(LittleFS, path.c_str(), 0, 0, 320, 200);
    return;
  }
  M5.Display.fillRect(0, 0, 320, 200, TFT_BLACK);
  M5.Display.fillCircle(160, 100, 70, exprColor(expr));   // 顔
  M5.Display.fillCircle(135, 90, 10, TFT_WHITE);          // 目
  M5.Display.fillCircle(185, 90, 10, TFT_WHITE);
  M5.Display.fillCircle(135, 90, 4, TFT_BLACK);
  M5.Display.fillCircle(185, 90, 4, TFT_BLACK);
}

static void drawTextBox(const String& text) {
  M5.Display.fillRect(0, 200, 320, 40, TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString(text, 160, 220);
}

void displayBegin() {
  LittleFS.begin(true);
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  showIdle();
}

void showIdle() {
  drawFace("normal");
  drawTextBox("まってるのだ");
}

void showNotify(const String& expr, const String& text) {
  drawFace(expr);
  drawTextBox(text.length() ? text : "おしらせなのだ");
}
```

> 補足: 日本語テキスト表示には日本語フォントが必要。`M5.Display.setFont(&fonts::lgfxJapanGothic_20);` を `displayBegin()` で設定すると `drawString` が日本語対応になる（M5Unified同梱フォント）。Step 2 の `displayBegin()` 末尾に
> `M5.Display.setFont(&fonts::lgfxJapanGothic_20);` を追加すること。

- [ ] **Step 3: 上記フォント行を追加**

`displayBegin()` の `M5.Display.setRotation(1);` の前に次を挿入：
```cpp
  M5.Display.setFont(&fonts::lgfxJapanGothic_20);
```

- [ ] **Step 4: コミット**

```bash
git add firmware/src/display.h firmware/src/display.cpp
git commit -m "feat(firmware): display idle/notify with JPG or placeholder face"
```

---

## Task 12: 通信（net.cpp — AsyncWebServer /notify, バイナリ受信）

**Files:**
- Create: `firmware/src/app_state.h`
- Create: `firmware/src/net.h`
- Create: `firmware/src/net.cpp`

> 設計: AsyncWebServer のbodyハンドラ(async TCPタスク)では M5描画/再生を直接呼ばず、共有バッファへWAVを蓄積し `pending` フラグを立てる。`main` ループ(Task 13)が拾って描画・再生する。

- [ ] **Step 1: 共有状態を作成** — `firmware/src/app_state.h`

```cpp
#pragma once
#include <Arduino.h>

// async受信→mainループ受け渡し用の共有状態。
struct PendingNotify {
  volatile bool ready = false;  // mainが処理すべき通知あり
  String expr;
  String text;
  uint8_t* wav = nullptr;       // PSRAM上のWAVバッファ
  size_t wavLen = 0;
};

extern PendingNotify g_notify;
```

- [ ] **Step 2: net ヘッダを作成** — `firmware/src/net.h`

```cpp
#pragma once

void netBegin(const char* ssid, const char* pass);  // WiFi+mDNS+HTTP開始
```

- [ ] **Step 3: net 実装** — `firmware/src/net.cpp`

```cpp
#include "net.h"
#include "app_state.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>

PendingNotify g_notify;
static AsyncWebServer server(80);

// /notify?expr=&text=  body=WAVバイナリ
static void onNotifyBody(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                         size_t index, size_t total) {
  if (index == 0) {
    // 受信開始: 前のバッファを解放しPSRAMに確保
    if (g_notify.wav) { free(g_notify.wav); g_notify.wav = nullptr; }
    g_notify.wav = (uint8_t*)ps_malloc(total);
    g_notify.wavLen = 0;
  }
  if (g_notify.wav && index + len <= total) {
    memcpy(g_notify.wav + index, data, len);
    g_notify.wavLen = index + len;
  }
  if (index + len == total) {
    g_notify.expr = req->hasParam("expr") ? req->getParam("expr")->value() : String("normal");
    g_notify.text = req->hasParam("text") ? req->getParam("text")->value() : String("");
    g_notify.ready = true;  // mainが拾う
  }
}

void netBegin(const char* ssid, const char* pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(300);

  if (MDNS.begin("stackchan")) {
    MDNS.addService("http", "tcp", 80);
  }

  server.on(
    "/notify", HTTP_POST,
    [](AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); },
    nullptr, onNotifyBody);
  server.begin();
}
```

- [ ] **Step 4: コミット（ビルドは Task 13 で確認）**

```bash
git add firmware/src/app_state.h firmware/src/net.h firmware/src/net.cpp
git commit -m "feat(firmware): async /notify endpoint buffers WAV for main loop"
```

---

## Task 13: main 配線とビルド・実機検証

**Files:**
- Create: `firmware/src/main.cpp`

- [ ] **Step 1: main を作成** — `firmware/src/main.cpp`

```cpp
#include <M5Unified.h>
#include "secrets.h"
#include "display.h"
#include "audio.h"
#include "net.h"
#include "app_state.h"

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

  if (g_notify.ready) {
    g_notify.ready = false;
    showNotify(g_notify.expr, g_notify.text);
    if (g_notify.wav && g_notify.wavLen > 0) {
      playWav(g_notify.wav, g_notify.wavLen);   // 再生
    }
    // 簡易: 3秒後にIDLEへ（Phase1は非ブロッキング厳密化はしない）
    delay(3000);
    showIdle();
  }

  delay(10);
}
```

- [ ] **Step 2: secrets を用意してビルド**

Run: `cd firmware && cp -n include/secrets.h.example include/secrets.h` → `include/secrets.h` を自分のWiFi情報に編集
Run: `cd firmware && pio run -e core2`
Expected: コンパイル成功（`SUCCESS`）

- [ ] **Step 3: 書き込み＆シリアル監視**

M5Stack Core2 をUSB接続し：
Run: `cd firmware && pio run -e core2 -t upload && pio device monitor`
Expected: 画面に簡易ずんだもん顔＋「まってるのだ」が表示。シリアルにWiFi接続ログ。
Run: `dns-sd -q stackchan.local`（別ターミナル, macOS）
Expected: `stackchan.local` がIPに解決される

- [ ] **Step 4: 実機 /notify 検証（ダミーWAV）**

VOICEVOX を起動し、合成WAVを1つ作ってPOSTする：
```bash
TEXT='こんにちはなのだ'
curl -s -X POST "http://127.0.0.1:50021/audio_query?speaker=3&text=$(python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$TEXT")" -o /tmp/q.json
curl -s -X POST "http://127.0.0.1:50021/synthesis?speaker=3" -H 'Content-Type: application/json' -d @/tmp/q.json -o /tmp/zunda.wav
curl -s -X POST "http://stackchan.local/notify?expr=happy&text=$(python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$TEXT")" \
  -H 'Content-Type: audio/wav' --data-binary @/tmp/zunda.wav
```
Expected: M5が笑顔表示＋「こんにちはなのだ」表示＋スピーカーから音声再生。3秒後にIDLEへ。
（音が出ない場合は `audioBegin()` の `setVolume` を上げる、または `M5.Speaker.config().sample_rate` がWAVと一致するか確認）

- [ ] **Step 5: コミット**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): main wiring; notify shows expr+text+plays WAV"
```

---

## Task 14: 立ち絵アセットの手順整備（任意・差し替え可能化）

**Files:**
- Create: `assets/zundamon/README.md`

- [ ] **Step 1: アセット手順を記載** — `assets/zundamon/README.md`

```markdown
# ずんだもん立ち絵（M5用）

## 入手
東北ずん子・ずんだもん公式サイトの「立ち絵」素材から、表情差分（ノーマル/笑顔/困り/驚き）をダウンロード。
利用は「キャラクター利用の手引き」に従い、所定のクレジットを表記する。

## 変換（320x200 JPGへ）
ImageMagick 例（背景を白で平坦化し縮小）:
```
magick normal.png   -background white -flatten -resize 320x200^ -gravity center -extent 320x200 normal.jpg
magick happy.png     -background white -flatten -resize 320x200^ -gravity center -extent 320x200 happy.jpg
magick worried.png   -background white -flatten -resize 320x200^ -gravity center -extent 320x200 worried.jpg
magick surprised.png -background white -flatten -resize 320x200^ -gravity center -extent 320x200 surprised.jpg
```

## M5へ書き込み（LittleFS）
`firmware/data/` に `normal.jpg/happy.jpg/worried.jpg/surprised.jpg` を置き：
```
cd firmware && pio run -e core2 -t uploadfs
```
書き込み後、`display.cpp` の `drawFace()` が自動でJPGを使う（無ければプレースホルダ顔）。
```

- [ ] **Step 2: コミット**

```bash
git add assets/zundamon/README.md
git commit -m "docs(assets): zundamon portrait prep + LittleFS upload guide"
```

---

## Task 15: Stopフック登録・E2E・README/クレジット

**Files:**
- Modify: `~/.claude/settings.json`（Stopフック登録）
- Create: `README.md`（リポジトリ）

- [ ] **Step 1: Stopフックを settings.json に登録**

`~/.claude/settings.json` の `hooks` に追記（既存設定は保持。`<repo>` は本リポジトリ絶対パス）:
```json
{
  "hooks": {
    "Stop": [
      { "hooks": [ { "type": "command", "command": "bash <repo>/hooks/stop.sh" } ] }
    ]
  }
}
```
登録後、Claude Code を再読込（新規セッション）して有効化。

- [ ] **Step 2: E2E 手動検証**

前提: VOICEVOX起動、daemon起動(`cd daemon && npm run dev`)、M5起動(`stackchan.local`解決可)。
手順: 別の Claude Code セッションで何か短いタスクを完了させる（応答が終わって停止する）。
Expected: 停止時に Stopフック→daemon→VOICEVOX→M5 の順で発火し、**ずんだもんが笑顔＋「おしごと、おわったのだ！」を発話**する。

- [ ] **Step 3: README とクレジットを作成** — `README.md`

```markdown
# claudecode_push_display — トークン残量お知らせずんだもん

Claude Code の状態を、机上の M5Stack Core2 のずんだもんが知らせる。
（ProtoPedia #8507 にインスパイア）

## Phase 1（実装済み）
Claude Code のタスク完了を、ずんだもんが表情＋VOICEVOX音声で通知。

## 構成
- `daemon/` … Mac常駐デーモン（フック受け口・VOICEVOX・M5通知）
- `hooks/`  … Claude Code フック（Stop）
- `firmware/` … M5Stack Core2 ファーム（PlatformIO）
- `assets/` … ずんだもん立ち絵

## セットアップ
1. VOICEVOX を起動（`http://127.0.0.1:50021`）
2. `cd daemon && npm install && npm run dev`
3. `firmware/include/secrets.h` にWiFi情報を記入し `pio run -e core2 -t upload`
4. `~/.claude/settings.json` に Stopフックを登録
5. Mac と M5 を同一ネットワーク（自宅WiFi / iPhoneテザリング）に接続

## クレジット
- 音声: VOICEVOX:ずんだもん
- キャラクター: 東北ずん子・ずんだもんプロジェクト（キャラクター利用の手引きに従う）
```

- [ ] **Step 4: 最終コミット**

```bash
git add README.md
git commit -m "docs: Phase 1 README and credits (VOICEVOX:ずんだもん)"
```

---

## 完了の定義（Phase 1）

- `cd daemon && npm test` 全PASS、`npm run build` 成功
- `bash hooks/stop.test.sh` PASS
- `cd firmware && pio test -e native` PASS
- 実機E2E: Claude Code のタスク完了で、ずんだもんが笑顔＋発話する（Task 15 Step 2）

## 後続フェーズ（別計画として作成予定）
- **Phase 2 (M3)**: 画面ダブルタップ→`/usage`→使用率ゲージ（ccusage連携、M5タッチ、daemonアドレス学習/heartbeat）
- **Phase 3 (M4)**: 使用率ポーラ＋閾値警告
- **Phase 4 (M5)**: PreToolUseフック＋`/approve`＋タップ/スワイプ承認
- **Phase 5 (M6)**: フォールバック（PC発話/say/ask）、USBトランスポート、仕上げ
