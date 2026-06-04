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
