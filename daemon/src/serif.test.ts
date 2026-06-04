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
