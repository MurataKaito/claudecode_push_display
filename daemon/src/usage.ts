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
