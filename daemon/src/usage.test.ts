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
