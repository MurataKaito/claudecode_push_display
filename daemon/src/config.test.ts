import { describe, it, expect } from "vitest";
import { loadConfig } from "./config.js";

describe("loadConfig", () => {
  it("空envなら既定値を返す", () => {
    const c = loadConfig({});
    expect(c).toEqual({
      port: 4920,
      voicevoxUrl: "http://127.0.0.1:50021",
      speakerId: 3,
      m5Url: "http://stackchan.local",
    });
  });

  it("env で上書きできる", () => {
    const c = loadConfig({
      ZUNDA_PORT: "5000",
      ZUNDA_VOICEVOX_URL: "http://localhost:60000",
      ZUNDA_SPEAKER_ID: "1",
      ZUNDA_M5_URL: "http://192.168.0.5",
    });
    expect(c.port).toBe(5000);
    expect(c.voicevoxUrl).toBe("http://localhost:60000");
    expect(c.speakerId).toBe(1);
    expect(c.m5Url).toBe("http://192.168.0.5");
  });
});
