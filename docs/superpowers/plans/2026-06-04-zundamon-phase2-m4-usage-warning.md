# ずんだもん Phase 2 / M4 — 残量警告（閾値ポーリング）実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** daemon が使用率を定期ポーリングし、閾値（既定 50/80/95%）を新たに跨いだら、ずんだもんが声で警告する（既存 `/notify` を再利用）。

**Architecture:** 純粋関数 `checkThresholds`（ウィンドウ毎の発火管理）＋ `pollOnce`（ccusage→usage→閾値判定→警告コールバック）に分離してテスト可能にする。`startPoller` が setInterval で `pollOnce` を回す。警告は `warnSerif` で文言化し `synth`→`transport.notify(worried)`。firmware変更なし。

**Tech Stack:** 既存 daemon（Node/TS+vitest）。M3の `ccusage.ts`/`usage.ts` を再利用。

**前提:** ブランチ `feature/zundamon-push-display`。M3完了済み（`ccusage.ts`,`usage.ts`,`computeUsage`,`getActiveBlock` 稼働）。M4スコープのみ。

---

## ファイル構成（M4）

```
daemon/src/
  thresholds.ts         [新] 閾値跨ぎ判定（純粋）+ test
  poller.ts             [新] pollOnce / startPoller + test(pollOnce)
  serif.ts              [変] warnSerif 追加
  config.ts             [変] thresholds, pollIntervalSec 追加
  index.ts              [変] startPoller 配線
```

---

## Task 1: 閾値跨ぎ判定（純粋関数）

**Files:** Create `daemon/src/thresholds.ts`, Test `daemon/src/thresholds.test.ts`

- [ ] **Step 1: 失敗するテスト** — `daemon/src/thresholds.test.ts`

```ts
import { describe, it, expect } from "vitest";
import { checkThresholds, type ThresholdState } from "./thresholds.js";

const EMPTY: ThresholdState = { windowKey: null, fired: [] };
const TH = [50, 80, 95];

describe("checkThresholds", () => {
  it("初回に閾値を跨いだら最大の跨ぎ値を返す", () => {
    const r = checkThresholds(EMPTY, "win1", 55, TH);
    expect(r.crossed).toBe(50);
    expect(r.state.fired).toEqual([50]);
  });

  it("一度跨いだ閾値は再発火しない", () => {
    const r1 = checkThresholds(EMPTY, "win1", 55, TH);
    const r2 = checkThresholds(r1.state, "win1", 60, TH);
    expect(r2.crossed).toBeNull();
  });

  it("複数同時跨ぎは最大値を返し全てfired", () => {
    const r = checkThresholds(EMPTY, "win1", 97, TH);
    expect(r.crossed).toBe(95);
    expect(r.state.fired).toEqual([50, 80, 95]);
  });

  it("新しいウィンドウでfiredがリセットされ再発火する", () => {
    const r1 = checkThresholds(EMPTY, "win1", 97, TH);
    const r2 = checkThresholds(r1.state, "win2", 55, TH);
    expect(r2.crossed).toBe(50);
    expect(r2.state.fired).toEqual([50]);
  });

  it("閾値未満なら何も起きない", () => {
    const r = checkThresholds(EMPTY, "win1", 40, TH);
    expect(r.crossed).toBeNull();
    expect(r.state.fired).toEqual([]);
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- thresholds`
Expected: FAIL（未作成）

- [ ] **Step 3: 実装** — `daemon/src/thresholds.ts`

```ts
export type ThresholdState = { windowKey: string | null; fired: number[] };

export function checkThresholds(
  state: ThresholdState,
  windowKey: string,
  percent: number,
  thresholds: number[],
): { crossed: number | null; state: ThresholdState } {
  const fired = state.windowKey === windowKey ? [...state.fired] : [];
  const newly = thresholds.filter((t) => percent >= t && !fired.includes(t));
  let crossed: number | null = null;
  if (newly.length > 0) {
    crossed = Math.max(...newly);
    fired.push(...newly);
    fired.sort((a, b) => a - b);
  }
  return { crossed, state: { windowKey, fired } };
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- thresholds`
Expected: PASS（5 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/thresholds.ts daemon/src/thresholds.test.ts
git commit -m "feat(daemon): threshold-crossing detector (per-window)"
```

---

## Task 2: 警告セリフ（serif.ts）

**Files:** Modify `daemon/src/serif.ts`, `daemon/src/serif.test.ts`

- [ ] **Step 1: 失敗するテストを追加** — `daemon/src/serif.test.ts` の `describe` 内に追加

```ts
  it("warnSerif は困り顔で閾値を読み上げる", async () => {
    const { warnSerif } = await import("./serif.js");
    const s = warnSerif(80);
    expect(s.expr).toBe("worried");
    expect(s.text).toContain("80");
  });
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- serif`
Expected: FAIL（warnSerif 未定義）

- [ ] **Step 3: 実装** — `daemon/src/serif.ts` の末尾に追加

```ts
export function warnSerif(threshold: number): Serif {
  return { expr: "worried", text: `もう ${threshold}％ つかったのだ、きをつけるのだ！` };
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- serif`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/serif.ts daemon/src/serif.test.ts
git commit -m "feat(daemon): warning serif for usage thresholds"
```

---

## Task 3: config に thresholds / pollIntervalSec 追加

**Files:** Modify `daemon/src/config.ts`, `daemon/src/config.test.ts`

- [ ] **Step 1: テスト更新** — `daemon/src/config.test.ts`

「空envなら既定値」の `toEqual` に2行追加:
```ts
      thresholds: [50, 80, 95],
      pollIntervalSec: 60,
```
「envで上書き」テスト末尾（`usageLimit` 行の後）に追加:
```ts
    expect(loadConfig({ ZUNDA_THRESHOLDS: "30,70" }).thresholds).toEqual([30, 70]);
    expect(loadConfig({ ZUNDA_POLL_SEC: "10" }).pollIntervalSec).toBe(10);
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- config`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/config.ts`

`Config` 型に2行追加:
```ts
  thresholds: number[];
  pollIntervalSec: number;
```
`loadConfig` の返却に追加（`usageLimit` の後）:
```ts
    thresholds: (env.ZUNDA_THRESHOLDS ?? "50,80,95")
      .split(",")
      .map((s) => Number(s.trim()))
      .filter((n) => !Number.isNaN(n)),
    pollIntervalSec: Number(env.ZUNDA_POLL_SEC ?? 60),
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- config`
Expected: PASS

- [ ] **Step 5: コミット**

```bash
git add daemon/src/config.ts daemon/src/config.test.ts
git commit -m "feat(daemon): thresholds + pollIntervalSec config"
```

---

## Task 4: ポーラ（pollOnce / startPoller）

**Files:** Create `daemon/src/poller.ts`, Test `daemon/src/poller.test.ts`

- [ ] **Step 1: 失敗するテスト** — `daemon/src/poller.test.ts`

```ts
import { describe, it, expect, vi } from "vitest";
import { pollOnce } from "./poller.js";
import type { ThresholdState } from "./thresholds.js";

const EMPTY: ThresholdState = { windowKey: null, fired: [] };

function deps(percent: number, onWarn: (t: number, p: number) => Promise<void>) {
  // percent から totalTokens を逆算（limit=100）
  return {
    getActiveBlock: async () => ({ totalTokens: percent, endTime: "2026-06-04T06:00:00.000Z" }),
    usageLimit: 100,
    thresholds: [50, 80, 95],
    now: () => Date.parse("2026-06-04T05:00:00.000Z"),
    onWarn,
  };
}

describe("pollOnce", () => {
  it("閾値を跨いだら onWarn(threshold, percent) を呼ぶ", async () => {
    const onWarn = vi.fn(async () => {});
    await pollOnce(EMPTY, deps(85, onWarn));
    expect(onWarn).toHaveBeenCalledWith(80, 85);
  });

  it("同ウィンドウで再度跨ぎ無しなら呼ばない", async () => {
    const onWarn = vi.fn(async () => {});
    const s1 = await pollOnce(EMPTY, deps(85, onWarn));
    onWarn.mockClear();
    await pollOnce(s1, deps(86, onWarn));
    expect(onWarn).not.toHaveBeenCalled();
  });
});
```

- [ ] **Step 2: テスト実行（失敗確認）**

Run: `cd daemon && npm test -- poller`
Expected: FAIL

- [ ] **Step 3: 実装** — `daemon/src/poller.ts`

```ts
import { computeUsage } from "./usage.js";
import { checkThresholds, type ThresholdState } from "./thresholds.js";
import type { ActiveBlock } from "./ccusage.js";

export type PollDeps = {
  getActiveBlock: () => Promise<ActiveBlock | null>;
  usageLimit: number;
  thresholds: number[];
  now: () => number;
  onWarn: (threshold: number, percent: number) => Promise<void>;
};

export async function pollOnce(state: ThresholdState, deps: PollDeps): Promise<ThresholdState> {
  const block = await deps.getActiveBlock();
  const usage = computeUsage(block, deps.usageLimit, deps.now());
  const key = usage.resetAt ?? "none";
  const { crossed, state: next } = checkThresholds(state, key, usage.percent, deps.thresholds);
  if (crossed !== null) await deps.onWarn(crossed, usage.percent);
  return next;
}

export function startPoller(deps: PollDeps, intervalMs: number): () => void {
  let state: ThresholdState = { windowKey: null, fired: [] };
  let stopped = false;
  const tick = async () => {
    if (stopped) return;
    try {
      state = await pollOnce(state, deps);
    } catch {
      /* ポーリング失敗は無視（次回再試行） */
    }
  };
  const handle = setInterval(tick, intervalMs);
  tick();
  return () => {
    stopped = true;
    clearInterval(handle);
  };
}
```

- [ ] **Step 4: テスト合格**

Run: `cd daemon && npm test -- poller`
Expected: PASS（2 tests）

- [ ] **Step 5: コミット**

```bash
git add daemon/src/poller.ts daemon/src/poller.test.ts
git commit -m "feat(daemon): usage poller (pollOnce/startPoller)"
```

---

## Task 5: index 配線（ポーラ起動）

**Files:** Modify `daemon/src/index.ts`

- [ ] **Step 1: 実装** — `daemon/src/index.ts`

import群に追加:
```ts
import { startPoller } from "./poller.js";
import { warnSerif } from "./serif.js";
```
`.then(() => { ... })` ブロック内の `setInterval(beat, 15000);` の後に追加:
```ts
    startPoller(
      {
        getActiveBlock,
        usageLimit: cfg.usageLimit,
        thresholds: cfg.thresholds,
        now: () => Date.now(),
        onWarn: async (threshold) => {
          const serif = warnSerif(threshold);
          const wav = await synthFn(serif.text);
          await transport.notify({ expr: serif.expr, text: serif.text, wav });
        },
      },
      cfg.pollIntervalSec * 1000,
    );
```

- [ ] **Step 2: テスト＆ビルド**

Run: `cd daemon && npm test && npm run build`
Expected: 全PASS・型エラー無し

- [ ] **Step 3: 実機E2E**

daemon再起動（旧daemon停止→`npm run dev`）。現在の使用率が95%超なので、**起動直後のポーリングで 95% 警告が1回発火**するはず。
Expected: M5が困り顔＋「もう 95％ つかったのだ、きをつけるのだ！」を表示＆発話。
（低い閾値で試したい場合: `ZUNDA_THRESHOLDS=1 ZUNDA_POLL_SEC=10 npm run dev`）

- [ ] **Step 4: コミット**

```bash
git add daemon/src/index.ts
git commit -m "feat(daemon): start usage poller for threshold warnings"
```

---

## 完了の定義（M4）
- `cd daemon && npm test` 全PASS（thresholds/poller/serif/config 追加分含む）、`npm run build` 成功
- 実機: daemon起動時に現使用率の最大跨ぎ閾値で1回警告発話（Task5 Step3）

## 次（別計画）
- **M5 タップ承認**: PreToolUseフック＋`/approve`＋M5タップ/スワイプ（双方向、`g_daemonBase` 再利用、settings.jsonにPreToolUse登録=ユーザ操作）
