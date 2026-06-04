import { describe, it, expect } from "vitest";
import { saySynth } from "./sayfallback.js";

// macOSの say/afconvert を実行する統合テスト（このプロジェクトはMac前提）。
describe("saySynth", () => {
  it("WAV(RIFF)のBufferを生成する", async () => {
    const wav = await saySynth("テストなのだ");
    expect(wav.subarray(0, 4).toString("latin1")).toBe("RIFF");
    expect(wav.length).toBeGreaterThan(1000);
  }, 20000);
});
