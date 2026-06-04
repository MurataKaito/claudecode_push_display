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
