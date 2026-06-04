import { describe, it, expect } from "vitest";
import { serifFor, warnSerif } from "./serif.js";

describe("serifFor", () => {
  it("done は笑顔で完了セリフ", () => {
    expect(serifFor({ type: "done" })).toEqual({
      expr: "happy",
      text: "おしごと、おわったのだ！",
    });
  });
});

describe("warnSerif", () => {
  it("困り顔で閾値を読み上げる", () => {
    const s = warnSerif(80);
    expect(s.expr).toBe("worried");
    expect(s.text).toContain("80");
  });
});
