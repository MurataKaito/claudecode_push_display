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
