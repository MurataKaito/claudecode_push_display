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
